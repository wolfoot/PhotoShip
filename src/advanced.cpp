#include "document.h"
#include <QColorSpace>
#include <QPainterPathStroker>
#include <QSet>
#include <algorithm>
#include <cmath>
namespace ps {
QStringList Document::selectedRoots() const {
    QStringList ids = state.selected;
    if (ids.isEmpty() || !ids.contains(state.active))
        ids = {state.active};
    QStringList result;
    for (const auto &l : state.layers)
        if (ids.contains(l.id)) {
            bool child = false;
            for (const auto &id : ids)
                if (id != l.id && descendant(l, id))
                    child = true;
            if (!child)
                result << l.id;
        }
    return result;
}
QStringList Document::selectedMembers() const {
    QStringList result;
    for (const auto &id : selectedRoots())
        result << descendants(id);
    return result;
}
void Document::setSelected(QStringList ids, QString activeID) {
    ids.removeDuplicates();
    for (int i = ids.size() - 1; i >= 0; --i)
        if (index(ids[i]) < 0)
            ids.removeAt(i);
    state.selected = ids;
    state.active = ids.contains(activeID) ? activeID : ids.value(0);
    lastChange = {};
    notify();
}
bool Document::dropLayers(QStringList ids, QString parent, QString before, QString *error) {
    auto fail = [&](QString e) {
        if (error)
            *error = e;
        return false;
    };
    if (!parent.isEmpty() && (index(parent) < 0 || !state.layers[index(parent)].isGroup()))
        return fail("Drop target must be a group.");
    QStringList roots, members;
    for (const auto &l : state.layers)
        if (ids.contains(l.id)) {
            bool child = false;
            for (const auto &id : ids)
                if (id != l.id && descendant(l, id))
                    child = true;
            if (!child) {
                roots << l.id;
                members << descendants(l.id);
            }
        }
    if (roots.isEmpty())
        return false;
    if (members.contains(parent) || members.contains(before))
        return fail("Cannot move a layer into itself or its descendants.");
    if (!before.isEmpty() && (index(before) < 0 || state.layers[index(before)].parent != parent))
        return fail("Invalid sibling target.");
    edit("Move selected layers", [&] {
        QVector<Layer> block;
        for (auto l : state.layers)
            if (members.contains(l.id)) {
                if (roots.contains(l.id))
                    l.parent = parent;
                block << l;
            }
        for (int i = state.layers.size() - 1; i >= 0; --i)
            if (members.contains(state.layers[i].id))
                state.layers.removeAt(i);
        int at = state.layers.size();
        if (!before.isEmpty())
            at = index(before);
        else if (!parent.isEmpty()) {
            at = index(parent) + 1;
            auto children = descendants(parent);
            for (int i = at; i < state.layers.size(); ++i)
                if (children.contains(state.layers[i].id))
                    at = i + 1;
        }
        for (int i = 0; i < block.size(); ++i)
            state.layers.insert(at + i, block[i]);
        state.selected = roots;
        state.active = roots.last();
    });
    return true;
}
bool Document::mergeSelected(QString *error) {
    auto fail = [&](QString e) {
        if (error)
            *error = e;
        return false;
    };
    auto roots = selectedRoots();
    if (roots.size() < 2)
        return fail("Select at least two adjacent sibling layers.");
    QString parent = state.layers[index(roots[0])].parent;
    QStringList siblings;
    for (const auto &l : state.layers)
        if (l.parent == parent)
            siblings << l.id;
    int first = siblings.indexOf(roots.first());
    for (int i = 0; i < roots.size(); ++i)
        if (siblings.value(first + i) != roots[i])
            return fail("Merge requires adjacent layers in the same group.");
    QString ancestor = parent;
    while (!ancestor.isEmpty()) {
        const auto &g = state.layers[index(ancestor)];
        if (g.opacity != 1)
            return fail("Merge inside a translucent group would change its appearance; set ancestor group "
                        "opacity to 100% first.");
        ancestor = g.parent;
    }
    int after = siblings.indexOf(roots.last()) + 1;
    if (after < siblings.size() && state.layers[index(siblings[after])].clipped)
        return fail("Include the dependent clipping stack before merging its base layers.");
    auto members = selectedMembers();
    Document temp;
    temp.state = state;
    temp.state.layers.clear();
    for (auto l : state.layers)
        if (members.contains(l.id)) {
            if (l.clipped || l.blend != "Normal" || l.kind == "adjustment")
                return fail("Merge supports Normal layers without clipping or adjustment dependencies. "
                            "Rasterize/flatten dependencies first.");
            if (roots.contains(l.id))
                l.parent = {};
            temp.state.layers << l;
        }
    qint64 replaced = 0;
    for (const auto &l : temp.state.layers)
        if (!l.isGroup())
            replaced += l.image.isNull() ? qint64(l.size.width()) * qRound(l.size.height())
                                         : qint64(l.image.width()) * l.image.height();
    if (pixelCount() - replaced + qint64(state.size.width()) * state.size.height() > MaxSourcePixels)
        return fail("Merged layer exceeds the source pixel budget.");
    Layer merged;
    merged.name = "Merged layers";
    merged.parent = parent;
    merged.size = state.size;
    merged.image = temp.composite();
    edit("Merge layers", [&] {
        int at = index(roots[0]);
        for (int i = state.layers.size() - 1; i >= 0; --i)
            if (members.contains(state.layers[i].id))
                state.layers.removeAt(i);
        state.layers.insert(at, merged);
        state.active = merged.id;
        state.selected = {merged.id};
    });
    return true;
}
bool Document::moveSelection(QPointF delta, bool copy, QString *error) {
    Layer *l = active();
    if (!l || l->kind != "raster" || state.selection.isEmpty()) {
        if (error)
            *error = "Select pixels on a raster layer first.";
        return false;
    }
    if (delta.isNull())
        return true;
    Layer original = *l;
    QTransform inverse = original.transform().inverted();
    QImage selected(original.image.size(), QImage::Format_ARGB32_Premultiplied);
    selected.fill(Qt::transparent);
    {
        QPainter q(&selected);
        q.setRenderHint(QPainter::Antialiasing);
        q.setTransform(inverse);
        q.setClipPath(state.selection);
        q.setTransform(QTransform());
        q.drawImage(QPoint(), original.image);
    }
    QImage mask(original.image.size(), QImage::Format_ARGB32_Premultiplied);
    mask.fill(Qt::transparent);
    {
        QPainter q(&mask);
        q.setRenderHint(QPainter::Antialiasing);
        q.setTransform(inverse);
        q.fillPath(state.selection, Qt::white);
    }
    QTransform translation;
    translation.translate(delta.x(), delta.y());
    QTransform sourceTranslation = original.transform() * translation * inverse;
    QRect selectedBounds =
        inverse.map(state.selection).boundingRect().toAlignedRect().intersected(original.image.rect());
    QRect expanded =
        original.image.rect().united(sourceTranslation.mapRect(QRectF(selectedBounds)).toAlignedRect());
    qint64 oldPixels = qint64(original.image.width()) * original.image.height(),
           newPixels = qint64(expanded.width()) * expanded.height();
    if (!validSize(expanded.size(), MaxSourcePixels) ||
        pixelCount() - oldPixels + newPixels > MaxSourcePixels) {
        if (error)
            *error = "Moved pixels exceed the source pixel budget.";
        return false;
    }
    qint64 masks = 0;
    for (const auto &layer : state.layers)
        masks += qint64(layer.mask.width()) * layer.mask.height();
    if (!original.mask.isNull() && masks - oldPixels + newPixels > MaxSourcePixels) {
        if (error)
            *error = "Moved pixels exceed the mask pixel budget.";
        return false;
    }
    QSizeF newSize(original.size.width() * expanded.width() / original.image.width(),
                   original.size.height() * expanded.height() / original.image.height());
    if (newSize.width() > MaxSide || newSize.height() > MaxSide) {
        if (error)
            *error = "Expanded layer exceeds the transform size limit.";
        return false;
    }
    edit(copy ? "Copy selected pixels" : "Move selected pixels", [&] {
        QImage moved(expanded.size(), QImage::Format_ARGB32_Premultiplied);
        moved.fill(Qt::transparent);
        moved.setColorSpace(original.image.colorSpace());
        {
            QPainter q(&moved);
            q.translate(-expanded.topLeft());
            q.drawImage(QPoint(), original.image);
            if (!copy) {
                q.setCompositionMode(QPainter::CompositionMode_DestinationOut);
                q.drawImage(QPoint(), mask);
            }
            q.setCompositionMode(QPainter::CompositionMode_SourceOver);
            q.setTransform(sourceTranslation, true);
            q.drawImage(QPoint(), selected);
        }
        l = active();
        l->image = moved;
        l->size = newSize;
        l->position += original.transform().map(QPointF(expanded.topLeft())) - l->transform().map(QPointF());
        if (!original.mask.isNull()) {
            l->mask = QImage(expanded.size(), QImage::Format_ARGB32_Premultiplied);
            l->mask.fill(Qt::white);
            QPainter q(&l->mask);
            q.drawImage(-expanded.topLeft(), original.mask);
        }
        state.selection = translation.map(state.selection);
    });
    return true;
}

bool Document::addAdjustment(QString kind, double first, double second, double third, QVector<QPointF> curve,
                             QString *error) {
    if (!canAdd(0, 1, error))
        return false;
    Layer l;
    l.kind = "adjustment";
    l.name = kind;
    l.adjustment = kind;
    l.first = first;
    l.second = second;
    l.third = third;
    l.curve = curve;
    l.size = state.size;
    if (!state.selection.isEmpty()) {
        qint64 masks = qint64(state.size.width()) * state.size.height();
        for (const auto &layer : state.layers)
            masks += qint64(layer.mask.width()) * layer.mask.height();
        if (masks > MaxSourcePixels) {
            if (error)
                *error = "Adjustment mask exceeds the mask pixel budget.";
            return false;
        }
        l.mask = QImage(state.size, QImage::Format_ARGB32_Premultiplied);
        l.mask.fill(Qt::black);
        QPainter q(&l.mask);
        q.setRenderHint(QPainter::Antialiasing);
        q.fillPath(state.selection, Qt::white);
    }
    edit("Add adjustment layer", [&] {
        insert(l);
        state.selected = {l.id};
    });
    return true;
}
QImage adjustedImage(const QImage &source, const Layer &l) {
    QImage straight = source.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < straight.height(); ++y) {
        auto row = reinterpret_cast<QRgb *>(straight.scanLine(y));
        for (int x = 0; x < straight.width(); ++x) {
            QColor c = QColor::fromRgba(row[x]);
            if (!c.alpha())
                continue;
            if (l.adjustment == "Hue / Saturation") {
                float h, s, v, a;
                c.getHsvF(&h, &s, &v, &a);
                h = h < 0 ? 0 : h;
                c.setHsvF(std::fmod(h + l.first / 360 + 1, 1), qBound(0., s * (1 + l.second / 100), 1.),
                          qBound(0., v + l.third / 100, 1.), a);
            } else {
                auto ch = [&](double v) {
                    if (l.adjustment == "Invert")
                        return 1 - v;
                    if (l.adjustment == "Levels")
                        return std::pow(
                            qBound(0., (v - l.first / 255) / qMax(.00001, (l.third - l.first) / 255), 1.),
                            1 / qMax(.01, l.second));
                    if (l.adjustment == "Curves" && !l.curve.isEmpty()) {
                        if (v <= l.curve.first().x())
                            return l.curve.first().y();
                        for (int i = 1; i < l.curve.size(); ++i)
                            if (v <= l.curve[i].x()) {
                                double t = (v - l.curve[i - 1].x()) /
                                           qMax(.00001, l.curve[i].x() - l.curve[i - 1].x());
                                return l.curve[i - 1].y() * (1 - t) + l.curve[i].y() * t;
                            }
                        return l.curve.last().y();
                    }
                    return v;
                };
                c.setRgbF(qBound(0., ch(c.redF()), 1.), qBound(0., ch(c.greenF()), 1.),
                          qBound(0., ch(c.blueF()), 1.), c.alphaF());
            }
            row[x] = c.rgba();
        }
    }
    return straight.convertToFormat(QImage::Format_ARGB32_Premultiplied);
}
void Document::repair(QPointF from, QPointF to, QPointF offset, double diameter, double opacity, bool heal,
                      const QImage &source) {
    auto *l = active();
    if (!l || l->kind != "raster" || source.isNull())
        return;
    QTransform inverse = l->transform().inverted();
    QPointF a = inverse.map(from), b = inverse.map(to), shift = inverse.map(to + offset) - b;
    double scale = std::sqrt(std::abs(l->transform().determinant()));
    double radius = qMax(.5, diameter / (2 * qMax(.0001, scale)));
    QRect bounds = QRectF(a, b)
                       .normalized()
                       .adjusted(-radius - 2, -radius - 2, radius + 2, radius + 2)
                       .toAlignedRect()
                       .intersected(l->image.rect());
    captureTiles(l->image, bounds);
    QImage patch(bounds.size(), QImage::Format_ARGB32_Premultiplied);
    patch.fill(Qt::transparent);
    QPainter q(&patch);
    q.translate(-bounds.topLeft());
    q.setRenderHint(QPainter::Antialiasing);
    q.setOpacity(opacity);
    QPainterPath stroke;
    stroke.moveTo(a);
    stroke.lineTo(b);
    QPainterPathStroker stroker;
    stroker.setWidth(2 * radius);
    stroker.setCapStyle(Qt::RoundCap);
    QPainterPath area = stroker.createStroke(stroke);
    if (a == b)
        area.addEllipse(a, radius, radius);
    if (!state.selection.isEmpty())
        area = area.intersected(inverse.map(state.selection));
    q.setClipPath(area);
    q.drawImage(-shift, source);
    q.end();
    if (heal) {
        QColor target = l->image.pixelColor(qBound(0, qRound(b.x()), l->image.width() - 1),
                                            qBound(0, qRound(b.y()), l->image.height() - 1)),
               sample = source.pixelColor(qBound(0, qRound(b.x() + shift.x()), source.width() - 1),
                                          qBound(0, qRound(b.y() + shift.y()), source.height() - 1));
        QImage straight = patch.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < straight.height(); ++y) {
            auto row = reinterpret_cast<QRgb *>(straight.scanLine(y));
            for (int x = 0; x < straight.width(); ++x) {
                auto c = QColor::fromRgba(row[x]);
                c.setRed(qBound(0, c.red() + target.red() - sample.red(), 255));
                c.setGreen(qBound(0, c.green() + target.green() - sample.green(), 255));
                c.setBlue(qBound(0, c.blue() + target.blue() - sample.blue(), 255));
                row[x] = c.rgba();
            }
        }
        patch = straight.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    }
    {
        QPainter painter(&l->image);
        painter.drawImage(bounds.topLeft(), patch);
    }
    QRect dirty =
        l->transform().mapRect(QRectF(bounds)).toAlignedRect().intersected(QRect(QPoint(), state.size));
    lastChange = lastChange.united(dirty);
    if (regionChanged)
        regionChanged(dirty);
}
} // namespace ps
