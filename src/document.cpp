#include "document.h"
#include <QColorSpace>
#include <QSet>
#include <algorithm>
#include <cmath>
#include <cstdlib>
extern "C" {
#include "WandPixels.h"
}
namespace ps {
QTransform Layer::transform() const {
    const QSize source =
        image.isNull() ? QSize(qMax(1, qRound(size.width())), qMax(1, qRound(size.height()))) : image.size();
    QTransform t;
    t.translate(position.x() + size.width() / 2, position.y() + size.height() / 2);
    t.rotate(rotation);
    t.translate(-size.width() / 2, -size.height() / 2);
    t.scale(size.width() / source.width(), size.height() / source.height());
    t.translate(flipX ? source.width() : 0, flipY ? source.height() : 0);
    t.scale(flipX ? -1 : 1, flipY ? -1 : 1);
    return t;
}
QImage Layer::pixels() const {
    QImage result;
    if (kind == "raster")
        result = image;
    else if (!isGroup()) {
        const QSize source = image.isNull()
                                 ? QSize(qMax(1, qRound(size.width())), qMax(1, qRound(size.height())))
                                 : image.size();
        result = QImage(source, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        if (kind == "text") {
            p.setFont(font);
            p.setPen(color);
            p.drawText(QRectF(QPointF(0, 0), source), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
        } else if (shape == "ellipse")
            p.drawEllipse(QRectF(0, 0, source.width(), source.height()));
        else
            p.drawRect(QRectF(0, 0, source.width(), source.height()));
    }
    if (!mask.isNull() && maskEnabled && !result.isNull()) {
        result = result.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        const QImage coverage = mask.convertToFormat(QImage::Format_Grayscale8).scaled(result.size());
        for (int y = 0; y < result.height(); ++y) {
            QRgb *line = reinterpret_cast<QRgb *>(result.scanLine(y));
            const uchar *m = coverage.constScanLine(y);
            for (int x = 0; x < result.width(); ++x) {
                QRgb c = line[x];
                int a = m[x];
                line[x] =
                    qRgba(qRed(c) * a / 255, qGreen(c) * a / 255, qBlue(c) * a / 255, qAlpha(c) * a / 255);
            }
        }
    }
    return result;
}
Document Document::create(QSize size) {
    Document d;
    d.state.size = size;
    Layer layer;
    layer.name = "Background";
    layer.size = size;
    layer.image = QImage(size, QImage::Format_ARGB32_Premultiplied);
    layer.image.fill(Qt::white);
    layer.image.setColorSpace(QColorSpace::SRgb);
    d.state.layers.append(layer);
    d.state.active = layer.id;
    return d;
}
bool Document::validSize(QSize size, qint64 budget) {
    return size.width() > 0 && size.height() > 0 && size.width() <= MaxSide && size.height() <= MaxSide &&
           qint64(size.width()) * size.height() <= budget;
}
int Document::index(const QString &id) const {
    for (int i = 0; i < state.layers.size(); ++i)
        if (state.layers[i].id == id)
            return i;
    return -1;
}
Layer *Document::active() {
    int i = index(state.active);
    return i < 0 ? nullptr : &state.layers[i];
}
const Layer *Document::active() const {
    int i = index(state.active);
    return i < 0 ? nullptr : &state.layers[i];
}
bool Document::descendant(const Layer &layer, const QString &id) const {
    QString p = layer.parent;
    for (int i = 0; !p.isEmpty() && i < MaxLayers; ++i) {
        if (p == id)
            return true;
        int j = index(p);
        if (j < 0)
            return false;
        p = state.layers[j].parent;
    }
    return false;
}
QStringList Document::descendants(const QString &id) const {
    QStringList ids{id};
    for (const auto &l : state.layers)
        if (descendant(l, id))
            ids << l.id;
    return ids;
}
qint64 Document::pixelCount() const {
    qint64 n = 0;
    for (const auto &l : state.layers)
        if (!l.isGroup())
            n += l.image.isNull() ? qint64(qRound(l.size.width())) * qRound(l.size.height())
                                  : qint64(l.image.width()) * l.image.height();
    return n;
}
void Document::begin(const QString &label) {
    if (editing)
        return;
    pending = {state, label, revision};
    editing = true;
}
void Document::commit() {
    if (!editing)
        return;
    past.append(pending);
    future.clear();
    revision = qMax(nextRevision, revision + 1);
    nextRevision = revision + 1;
    editing = false;
    trimHistory();
    notify();
}
void Document::cancel() {
    if (!editing)
        return;
    state = pending.state;
    revision = pending.revision;
    editing = false;
    notify();
}
void Document::edit(const QString &label, const std::function<void()> &op) {
    begin(label);
    op();
    commit();
}
void Document::undo() {
    if (editing || past.isEmpty())
        return;
    auto e = past.takeLast();
    future.append({state, e.label, revision});
    state = e.state;
    revision = e.revision;
    notify();
}
void Document::redo() {
    if (editing || future.isEmpty())
        return;
    auto e = future.takeLast();
    past.append({state, e.label, revision});
    state = e.state;
    revision = e.revision;
    trimHistory();
    notify();
}
void Document::trimHistory() {
    // QImage snapshots share unchanged storage. Edited layers detach once per stroke.
    // ponytail: bounded whole-layer snapshots; tile deltas when large-document painting is added.
    auto bytes = [&]() {
        QSet<qint64> seen;
        qint64 total = 0;
        auto visit = [&](const State &s) {
            for (const auto &l : s.layers)
                for (const auto &im : {l.image, l.mask})
                    if (!im.isNull() && !seen.contains(im.cacheKey())) {
                        seen.insert(im.cacheKey());
                        total += im.sizeInBytes();
                    }
        };
        visit(state);
        for (const auto &e : past)
            visit(e.state);
        for (const auto &e : future)
            visit(e.state);
        return total;
    };
    while (past.size() + future.size() > 100 || bytes() > 256LL * 1024 * 1024) {
        if (!past.isEmpty())
            past.removeFirst();
        else if (!future.isEmpty())
            future.removeFirst();
        else
            break;
    }
    pending = {};
}
bool Document::canAdd(qint64 pixels, int count, QString *error) const {
    if (state.layers.size() + count > MaxLayers || pixelCount() + pixels > MaxSourcePixels) {
        if (error)
            *error = "Document limit reached: 256 layers or 32 million source pixels.";
        return false;
    }
    return true;
}
void Document::insert(Layer layer) {
    const Layer *a = active();
    QString parent = a ? (a->isGroup() ? a->id : a->parent) : QString();
    layer.parent = parent;
    int at = state.layers.size();
    if (a) {
        auto ids = descendants(a->id);
        at = index(a->id) + 1;
        while (at < state.layers.size() && ids.contains(state.layers[at].id))
            ++at;
    }
    state.active = layer.id;
    state.layers.insert(at, layer);
}
bool Document::addImage(const QImage &source, const QString &name, QString *error) {
    if (!validSize(source.size(), MaxSourcePixels)) {
        if (error)
            *error = "Image is empty or exceeds the image size limit.";
        return false;
    }
    if (!canAdd(qint64(source.width()) * source.height(), 1, error))
        return false;
    edit("Import image", [&]() {
        Layer l;
        l.name = name;
        l.image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        l.size = source.size();
        l.position =
            QPointF((state.size.width() - l.size.width()) / 2, (state.size.height() - l.size.height()) / 2);
        insert(l);
    });
    return true;
}
bool Document::addBlank(QString *error) {
    if (!canAdd(qint64(state.size.width()) * state.size.height(), 1, error))
        return false;
    edit("New layer", [&]() {
        Layer l;
        l.size = state.size;
        l.name = "Empty layer";
        l.image = QImage(state.size, QImage::Format_ARGB32_Premultiplied);
        l.image.fill(Qt::transparent);
        insert(l);
    });
    return true;
}
bool Document::addGroup(QString *error) {
    if (!canAdd(0, 1, error))
        return false;
    edit("New group", [&]() {
        Layer l;
        l.name = "Group";
        l.kind = "group";
        l.size = state.size;
        insert(l);
    });
    return true;
}
bool Document::addText(const QString &text, QFont font, QColor color, QRectF bounds, QString *error) {
    if (text.size() > 100000 || font.pointSizeF() < 1 || font.pointSizeF() > 1000 || !color.isValid()) {
        if (error)
            *error = "Text is too long, or its font size / color is invalid.";
        return false;
    }
    if (!validSize(bounds.size().toSize()) ||
        !canAdd(qint64(qRound(bounds.width())) * qRound(bounds.height()), 1, error))
        return false;
    edit("Add text", [&]() {
        Layer l;
        l.kind = "text";
        l.name = text.left(30);
        l.text = text;
        l.font = font;
        l.color = color;
        l.position = bounds.topLeft();
        l.size = bounds.size();
        insert(l);
    });
    return true;
}
bool Document::addShape(QString shape, QColor color, QRectF bounds, QString *error) {
    if (!QStringList{"rectangle", "ellipse"}.contains(shape) || !color.isValid()) {
        if (error)
            *error = "Invalid shape or color.";
        return false;
    }
    if (!validSize(bounds.size().toSize()) ||
        !canAdd(qint64(qRound(bounds.width())) * qRound(bounds.height()), 1, error))
        return false;
    edit("Add shape", [&]() {
        Layer l;
        l.kind = "shape";
        l.name = shape == "ellipse" ? "Ellipse" : "Rectangle";
        l.shape = shape;
        l.color = color;
        l.position = bounds.topLeft();
        l.size = bounds.size();
        insert(l);
    });
    return true;
}
bool Document::duplicate(QString *error) {
    const Layer *a = active();
    if (!a)
        return false;
    auto ids = descendants(a->id);
    qint64 pixels = 0;
    for (const auto &l : state.layers)
        if (ids.contains(l.id) && !l.isGroup())
            pixels += l.image.isNull() ? qint64(qRound(l.size.width())) * qRound(l.size.height())
                                       : qint64(l.image.width()) * l.image.height();
    if (!canAdd(pixels, ids.size(), error))
        return false;
    edit("Duplicate layer", [&]() {
        QMap<QString, QString> map;
        for (const auto &id : ids)
            map[id] = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QVector<Layer> copies;
        int at = 0;
        for (int i = 0; i < state.layers.size(); ++i)
            if (ids.contains(state.layers[i].id)) {
                auto l = state.layers[i];
                at = i + 1;
                l.id = map[l.id];
                if (map.contains(l.parent))
                    l.parent = map[l.parent];
                if (l.id == map[ids[0]])
                    l.name += " copy";
                copies.append(l);
            }
        for (int i = 0; i < copies.size(); ++i)
            state.layers.insert(at + i, copies[i]);
        state.active = map[ids[0]];
    });
    return true;
}
void Document::remove() {
    if (!active())
        return;
    edit("Delete layer", [&]() {
        auto ids = descendants(state.active);
        for (int i = state.layers.size() - 1; i >= 0; --i)
            if (ids.contains(state.layers[i].id))
                state.layers.removeAt(i);
        state.active = state.layers.isEmpty() ? QString() : state.layers.last().id;
    });
}
void Document::reorder(int direction) {
    const Layer *a = active();
    if (!a)
        return;
    QStringList siblings;
    for (const auto &l : state.layers)
        if (l.parent == a->parent)
            siblings << l.id;
    int n = siblings.indexOf(a->id);
    int target = n + direction;
    if (target < 0 || target >= siblings.size())
        return;
    edit("Reorder layer", [&]() {
        auto ids = descendants(state.active);
        auto other = descendants(siblings[target]);
        QVector<Layer> block;
        int otherAt = -1;
        for (int i = 0; i < state.layers.size(); ++i) {
            if (ids.contains(state.layers[i].id))
                block.append(state.layers[i]);
        }
        for (int i = state.layers.size() - 1; i >= 0; --i)
            if (ids.contains(state.layers[i].id))
                state.layers.removeAt(i);
        for (int i = 0; i < state.layers.size(); ++i)
            if (other.contains(state.layers[i].id)) {
                if (otherAt < 0)
                    otherAt = i;
                if (direction > 0)
                    otherAt = i + 1;
            }
        for (int i = 0; i < block.size(); ++i)
            state.layers.insert(otherAt + i, block[i]);
    });
}
void Document::reparent(const QString &parent) {
    Layer *a = active();
    if (!a || a->parent == parent || parent == a->id)
        return;
    int p = index(parent);
    if (!parent.isEmpty() && (p < 0 || !state.layers[p].isGroup() || descendant(state.layers[p], a->id)))
        return;
    edit("Move to group", [&]() {
        auto ids = descendants(state.active);
        QVector<Layer> block;
        for (const auto &l : state.layers)
            if (ids.contains(l.id))
                block.append(l);
        for (int i = state.layers.size() - 1; i >= 0; --i)
            if (ids.contains(state.layers[i].id))
                state.layers.removeAt(i);
        block[0].parent = parent;
        int at = state.layers.size();
        if (!parent.isEmpty()) {
            auto members = descendants(parent);
            for (int i = 0; i < state.layers.size(); ++i)
                if (members.contains(state.layers[i].id))
                    at = i + 1;
        }
        for (int i = 0; i < block.size(); ++i)
            state.layers.insert(at + i, block[i]);
    });
}
QStringList Document::blendModes() {
    return {"Normal",      "Multiply",   "Screen",     "Overlay",    "Darken",     "Lighten",
            "Color Dodge", "Color Burn", "Hard Light", "Soft Light", "Difference", "Exclusion"};
}
static QPainter::CompositionMode composition(const QString &name) {
    static const QMap<QString, QPainter::CompositionMode> modes = {
        {"Multiply", QPainter::CompositionMode_Multiply},
        {"Screen", QPainter::CompositionMode_Screen},
        {"Overlay", QPainter::CompositionMode_Overlay},
        {"Darken", QPainter::CompositionMode_Darken},
        {"Lighten", QPainter::CompositionMode_Lighten},
        {"Color Dodge", QPainter::CompositionMode_ColorDodge},
        {"Color Burn", QPainter::CompositionMode_ColorBurn},
        {"Hard Light", QPainter::CompositionMode_HardLight},
        {"Soft Light", QPainter::CompositionMode_SoftLight},
        {"Difference", QPainter::CompositionMode_Difference},
        {"Exclusion", QPainter::CompositionMode_Exclusion}};
    return modes.value(name, QPainter::CompositionMode_SourceOver);
}
void Document::render(QPainter &p) const {
    p.save();
    p.setClipRect(QRect(QPoint(), state.size), Qt::IntersectClip);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    p.setRenderHint(QPainter::Antialiasing);
    for (const auto &l : state.layers) {
        if (l.isGroup() || !l.visible)
            continue;
        double opacity = l.opacity;
        bool visible = true;
        QString parent = l.parent;
        for (int depth = 0; !parent.isEmpty() && depth < MaxLayers; ++depth) {
            int i = index(parent);
            if (i < 0) {
                visible = false;
                break;
            }
            const auto &g = state.layers[i];
            visible = visible && g.visible;
            opacity *= g.opacity;
            parent = g.parent;
        }
        if (!visible || opacity <= 0)
            continue;
        p.save();
        p.setOpacity(opacity);
        p.setCompositionMode(composition(l.blend));
        p.setRenderHint(QPainter::SmoothPixmapTransform, !l.nearest);
        p.setTransform(l.transform(), true);
        p.drawImage(QPointF(), l.pixels());
        p.restore();
    }
    p.restore();
}
QImage Document::composite() const {
    QImage result(state.size, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);
    result.setColorSpace(QColorSpace::SRgb);
    result.setDotsPerMeterX(qRound(state.resolution / .0254));
    result.setDotsPerMeterY(qRound(state.resolution / .0254));
    QPainter p(&result);
    render(p);
    return result;
}
void Document::crop(QRect bounds) {
    bounds = bounds.intersected(QRect(QPoint(), state.size));
    if (bounds.isEmpty() || bounds == QRect(QPoint(), state.size))
        return;
    edit("Crop canvas", [&]() {
        state.size = bounds.size();
        for (auto &l : state.layers)
            l.position -= bounds.topLeft();
        state.selection = {};
    });
}
void Document::paint(QPointF from, QPointF to, double diameter, QColor color, double opacity, bool erase,
                     bool onMask) {
    Layer *l = active();
    if (!l || l->isGroup() || (!onMask && l->kind != "raster"))
        return;
    QSize source = l->image.isNull() ? l->size.toSize() : l->image.size();
    if (onMask && l->mask.isNull()) {
        l->mask = QImage(source, QImage::Format_ARGB32_Premultiplied);
        l->mask.fill(Qt::white);
    }
    QImage &target = onMask ? l->mask : l->image;
    bool invertible = false;
    QTransform inverse = l->transform().inverted(&invertible);
    if (!invertible)
        return;
    QPainter p(&target);
    p.setRenderHint(QPainter::Antialiasing);
    p.setTransform(inverse);
    p.setClipRect(QRect(QPoint(), state.size));
    if (!state.selection.isEmpty())
        p.setClipPath(state.selection, Qt::IntersectClip);
    if (onMask) {
        color =
            erase ? Qt::black : QColor::fromRgb(qGray(color.rgb()), qGray(color.rgb()), qGray(color.rgb()));
    } else if (erase)
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    color.setAlphaF(color.alphaF() * qBound(0.0, opacity, 1.0));
    p.setPen(QPen(color, diameter, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (from == to) {
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawEllipse(to, diameter / 2, diameter / 2);
    } else
        p.drawLine(from, to);
}
void Document::fill(QColor color, bool onMask) {
    Layer *l = active();
    if (!l || l->isGroup() || (!onMask && l->kind != "raster"))
        return;
    edit("Fill", [&]() {
        l = active();
        QSize source = l->image.isNull() ? l->size.toSize() : l->image.size();
        if (onMask && l->mask.isNull()) {
            l->mask = QImage(source, QImage::Format_ARGB32_Premultiplied);
            l->mask.fill(Qt::white);
        }
        QImage &target = onMask ? l->mask : l->image;
        QPainter p(&target);
        p.setTransform(l->transform().inverted());
        p.setClipRect(QRect(QPoint(), state.size));
        if (!state.selection.isEmpty())
            p.setClipPath(state.selection, Qt::IntersectClip);
        if (onMask) {
            int v = qGray(color.rgb());
            color = QColor(v, v, v);
        }
        p.fillRect(QRect(QPoint(), state.size), color);
    });
}
bool Document::magicWand(QPoint point, int tolerance, bool add, bool subtract, QString *error) {
    if (!QRect(QPoint(), state.size).contains(point))
        return false;
    QImage image = composite().convertToFormat(QImage::Format_RGBA8888_Premultiplied);
    QByteArray mask(image.width() * image.height(), 0);
    if (wand_mask(image.constBits(), image.width(), image.height(), image.bytesPerLine(), point.x(),
                  point.y(), 0, tolerance, 1, reinterpret_cast<uint8_t *>(mask.data())) < 0) {
        if (error)
            *error = "Not enough memory for selection.";
        return false;
    }
    int32_t *points = nullptr, *loops = nullptr;
    size_t count = 0, loopCount = 0;
    int status = wand_trace(reinterpret_cast<const uint8_t *>(mask.constData()), image.width(),
                            image.height(), &points, &count, &loops, &loopCount);
    if (status != 0) {
        free(points);
        free(loops);
        if (error)
            *error = "Selection is too complex or memory is exhausted. Try a simpler area.";
        return false;
    }
    QPainterPath path;
    path.setFillRule(Qt::WindingFill);
    size_t cursor = 0;
    for (size_t i = 0; i < loopCount; ++i) {
        for (int j = 0; j < loops[i]; ++j) {
            QPointF p(points[cursor * 2], points[cursor * 2 + 1]);
            if (j == 0)
                path.moveTo(p);
            else
                path.lineTo(p);
            ++cursor;
        }
        path.closeSubpath();
    }
    free(points);
    free(loops);
    state.selection = subtract ? state.selection.subtracted(path) : add ? state.selection.united(path) : path;
    notify();
    return true;
}
void Document::adjust(const QString &kind, double first, double second, double third,
                      const QVector<QPointF> &curve) {
    Layer *l = active();
    if (!l || l->kind != "raster")
        return;
    edit(kind, [&]() {
        l = active();
        QImage straight = l->image.convertToFormat(QImage::Format_ARGB32);
        const QTransform transform = l->transform();
        for (int y = 0; y < straight.height(); ++y) {
            QRgb *row = reinterpret_cast<QRgb *>(straight.scanLine(y));
            for (int x = 0; x < straight.width(); ++x) {
                if (!state.selection.isEmpty() &&
                    !state.selection.contains(transform.map(QPointF(x + .5, y + .5))))
                    continue;
                QColor c = QColor::fromRgba(row[x]);
                if (!c.alpha())
                    continue;
                double r = c.redF(), g = c.greenF(), b = c.blueF();
                if (kind == "Hue / Saturation") {
                    float h, s, v, a;
                    c.getHsvF(&h, &s, &v, &a);
                    h = h < 0 ? 0 : h;
                    h = std::fmod(h + first / 360.0 + 1.0, 1.0);
                    c.setHsvF(h, qBound(0.0, s * (1 + second / 100.0), 1.0),
                              qBound(0.0, v + third / 100.0, 1.0), a);
                } else {
                    auto channel = [&](double value) {
                        if (kind == "Invert")
                            return 1 - value;
                        if (kind == "Levels")
                            return std::pow(
                                qBound(0.0, (value - first / 255.0) / qMax(.00001, (third - first) / 255.0),
                                       1.0),
                                1 / qMax(.01, second));
                        if (kind == "Curves" && !curve.isEmpty()) {
                            if (value <= curve.first().x())
                                return curve.first().y();
                            for (int i = 1; i < curve.size(); ++i)
                                if (value <= curve[i].x()) {
                                    double t = (value - curve[i - 1].x()) /
                                               qMax(.00001, curve[i].x() - curve[i - 1].x());
                                    return curve[i - 1].y() * (1 - t) + curve[i].y() * t;
                                }
                            return curve.last().y();
                        }
                        return value;
                    };
                    c.setRgbF(qBound(0.0, channel(r), 1.0), qBound(0.0, channel(g), 1.0),
                              qBound(0.0, channel(b), 1.0), c.alphaF());
                }
                row[x] = c.rgba();
            }
        }
        l->image = straight.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    });
}
} // namespace ps
