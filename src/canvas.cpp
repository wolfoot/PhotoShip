#include "canvas.h"
#include <QDragEnterEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPointer>
#include <QTabletEvent>
#include <QUrl>
#include <QWheelEvent>
#include <cmath>
namespace ps {
Canvas::Canvas(Document *d, QWidget *parent) : QWidget(parent), document(d) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumSize(200, 150);
    setObjectName("canvas");
    document->regionChanged = [self = QPointer<Canvas>(this)](QRect region) {
        if (self)
            self->invalidateRegion(region);
    };
}
QTransform Canvas::view() const {
    QTransform t;
    t.translate(width() / 2.0 + pan.x(), height() / 2.0 + pan.y());
    t.scale(scale, scale);
    t.translate(-document->state.size.width() / 2.0, -document->state.size.height() / 2.0);
    return t;
}
QPointF Canvas::documentPoint(QPointF p) const {
    return view().inverted().map(p);
}
QPointF Canvas::widgetPoint(QPointF p) const {
    return view().map(p);
}
void Canvas::invalidate() {
    tiles.clear();
    update();
}
void Canvas::invalidateRegion(QRect region) {
    if (region.isEmpty())
        return;
    QRect area = region.adjusted(-2, -2, 2, 2).intersected(QRect(QPoint(), document->state.size));
    for (int y = area.top() / 256; y <= area.bottom() / 256; ++y)
        for (int x = area.left() / 256; x <= area.right() / 256; ++x)
            tiles.remove((quint64(y) << 32) | quint32(x));
    update(view().mapRect(QRectF(area)).toAlignedRect().adjusted(-3, -3, 3, 3));
}
void Canvas::setTool(Tool value) {
    abortGesture();
    tool = value;
    updateCursor();
    update();
}
void Canvas::fit() {
    scale = qBound(.02,
                   qMin((width() - 80.0) / document->state.size.width(),
                        (height() - 80.0) / document->state.size.height()),
                   8.0);
    pan = {};
    update();
    if (message)
        message(QString("Zoom %1%").arg(qRound(scale * 100)));
}
void Canvas::actualSize() {
    scale = 1;
    pan = {};
    update();
    if (message)
        message("Zoom 100%");
}
void Canvas::zoomBy(double factor) {
    scale = qBound(.02, scale * factor, 32.0);
    update();
    if (message)
        message(QString("Zoom %1%").arg(qRound(scale * 100)));
}
void Canvas::updateCursor() {
    setCursor(space || tool == Tool::Hand ? Qt::OpenHandCursor
              : tool == Tool::Move        ? Qt::ArrowCursor
                                          : Qt::CrossCursor);
}
void Canvas::paintEvent(QPaintEvent *event) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#202329"));
    p.setRenderHint(QPainter::Antialiasing);
    QTransform t = view();
    QRectF bounds = t.mapRect(QRectF(QPointF(), document->state.size));
    p.fillRect(bounds.adjusted(-2, -2, 2, 2), QColor("#101217"));
    p.save();
    p.setClipRect(bounds);
    const int tile = 12;
    int left = qMax(0, int(std::floor(bounds.left() / tile)) * tile),
        top = qMax(0, int(std::floor(bounds.top() / tile)) * tile);
    int right = qMin(width(), int(std::ceil(bounds.right()))),
        bottom = qMin(height(), int(std::ceil(bounds.bottom())));
    for (int y = top; y < bottom; y += tile)
        for (int x = left; x < right; x += tile)
            p.fillRect(QRect(x, y, tile, tile),
                       ((x / tile + y / tile) % 2) ? QColor("#a1a4aa") : QColor("#c5c7cc"));
    p.setTransform(t);
    p.setRenderHint(QPainter::SmoothPixmapTransform, scale < 1);
    QRect visible = t.inverted()
                        .mapRect(QRectF(event->rect()))
                        .toAlignedRect()
                        .adjusted(-1, -1, 1, 1)
                        .intersected(QRect(QPoint(), document->state.size));
    if (!visible.isEmpty())
        for (int y = visible.top() / 256; y <= visible.bottom() / 256; ++y)
            for (int x = visible.left() / 256; x <= visible.right() / 256; ++x) {
                quint64 key = (quint64(y) << 32) | quint32(x);
                QImage *image = tiles.object(key);
                if (!image) {
                    QRect region = QRect(x * 256, y * 256, 256, 256)
                                       .adjusted(-1, -1, 1, 1)
                                       .intersected(QRect(QPoint(), document->state.size));
                    image = new QImage(document->compositeTile(region));
                    tiles.insert(key, image);
                    ++tilesRendered;
                }
                QRect tile =
                    QRect(x * 256, y * 256, 256, 256).intersected(QRect(QPoint(), document->state.size));
                QPoint origin(qMax(0, x * 256 - 1), qMax(0, y * 256 - 1));
                // Partition in device pixels: fractional view coordinates otherwise leave one-pixel gaps.
                QRectF device = t.mapRect(QRectF(tile));
                int left = std::ceil(device.left()), top = std::ceil(device.top());
                QRect clip(left, top, int(std::ceil(device.right())) - left,
                           int(std::ceil(device.bottom())) - top);
                p.save();
                p.resetTransform();
                p.setClipRect(clip, Qt::IntersectClip);
                p.setTransform(t);
                p.setRenderHint(QPainter::Antialiasing, false);
                p.drawImage(origin, *image);
                p.restore();
            }
    p.restore();
    p.save();
    p.setTransform(t);
    p.setClipRect(QRectF(QPointF(), document->state.size));
    if (!document->state.selection.isEmpty()) {
        QPen black(Qt::black, 1 / scale);
        black.setCosmetic(false);
        p.setPen(black);
        p.drawPath(document->state.selection);
        QPen white(Qt::white, 1 / scale, Qt::DashLine);
        p.setPen(white);
        p.drawPath(document->state.selection);
    }
    if (!draft.isEmpty()) {
        p.setPen(QPen(QColor("#78a8ff"), 1 / scale, Qt::DashLine));
        p.setBrush(QColor(98, 140, 255, 35));
        p.drawPath(draft);
    }
    const Layer *l = document->active();
    if (l && !l->isGroup() && tool == Tool::Move) {
        QSizeF source = l->image.isNull() ? l->size : l->image.size();
        QPolygonF poly = l->transform().map(QPolygonF(QRectF(QPointF(), source)));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor("#78a8ff"), 1 / scale));
        p.drawPolygon(poly);
        p.setBrush(Qt::white);
        for (int i = 0; i < 4; ++i)
            p.drawRect(QRectF(poly[i] - QPointF(3 / scale, 3 / scale), QSizeF(6 / scale, 6 / scale)));
    }
    if ((tool == Tool::Brush || tool == Tool::Eraser || tool == Tool::Clone || tool == Tool::Heal) &&
        underMouse()) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(Qt::black, 2 / scale));
        p.drawEllipse(cursor, brushSize / 2, brushSize / 2);
        p.setPen(QPen(Qt::white, 1 / scale));
        p.drawEllipse(cursor, brushSize / 2, brushSize / 2);
    }
    p.restore();
}
void Canvas::resizeEvent(QResizeEvent *) {
    if (!hasResized) {
        hasResized = true;
        fit();
    }
}
void Canvas::selection(QPainterPath path) {
    QPainterPath canvas;
    canvas.addRect(QRectF(QPointF(), document->state.size));
    path = path.intersected(canvas);
    document->state.selection = modifiers.testFlag(Qt::AltModifier)     ? oldSelection.subtracted(path)
                                : modifiers.testFlag(Qt::ShiftModifier) ? oldSelection.united(path)
                                                                        : path;
    document->notify();
}
void Canvas::mousePressEvent(QMouseEvent *e) {
    setFocus();
    cursor = documentPoint(e->position());
    if (e->button() != Qt::LeftButton && e->button() != Qt::MiddleButton)
        return;
    pressed = true;
    start = last = cursor;
    widgetStart = e->position();
    modifiers = e->modifiers();
    oldSelection = document->state.selection;
    gestureEdited = false;
    panning = e->button() == Qt::MiddleButton || space || tool == Tool::Hand;
    if (panning) {
        panStart = pan;
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    QRectF bounds(QPointF(), document->state.size);
    if (!bounds.contains(start)) {
        pressed = false;
        return;
    }
    Layer *l = document->active();
    if (tool == Tool::Brush || tool == Tool::Eraser || tool == Tool::Clone || tool == Tool::Heal) {
        if (!l || l->isGroup() ||
            ((!maskTarget || tool == Tool::Clone || tool == Tool::Heal) && l->kind != "raster")) {
            pressed = false;
            if (message)
                message("Select a raster layer to paint, or rasterize the current layer first.");
            return;
        }
        if (tool == Tool::Clone || tool == Tool::Heal) {
            if (modifiers.testFlag(Qt::AltModifier)) {
                samplePoint = start;
                sampleLayer = l->id;
                hasSample = true;
                pressed = false;
                if (message)
                    message("Repair source selected. Paint on this layer to clone/heal.");
                return;
            }
            if (!hasSample || sampleLayer != l->id) {
                pressed = false;
                if (message)
                    message("Alt-click this raster layer to select a source first.");
                return;
            }
            repairSource = l->image.copy();
            repairOffset = samplePoint - start;
        }
        document->beginStroke(maskTarget             ? "Paint mask"
                              : tool == Tool::Heal   ? "Heal stroke"
                              : tool == Tool::Clone  ? "Clone stroke"
                              : tool == Tool::Eraser ? "Erase"
                                                     : "Brush stroke",
                              maskTarget && tool != Tool::Clone && tool != Tool::Heal);
        double diameter = brushSize * (pressureSize ? pressure : 1),
               alpha = brushOpacity * (pressureOpacity ? pressure : 1);
        if (tool == Tool::Clone || tool == Tool::Heal)
            document->repair(start, start, repairOffset, diameter, alpha, tool == Tool::Heal, repairSource);
        else
            document->paint(start, start, diameter, foreground, alpha, tool == Tool::Eraser, maskTarget);
        gestureEdited = true;
        update();
    } else if (tool == Tool::Move) {
        scaling = false;
        movingSelection = false;
        if (l && l->kind == "raster" && !document->state.selection.isEmpty() &&
            document->state.selection.contains(start)) {
            movingSelection = true;
            draft = document->state.selection;
            return;
        }
        if (l && !l->isGroup()) {
            QSizeF source = l->image.isNull() ? l->size : l->image.size();
            QPointF handle = l->transform().map(QPointF(source.width(), source.height()));
            scaling = QLineF(widgetPoint(handle), e->position()).length() < 10;
        }
        if (!scaling && (!l || !l->isGroup())) {
            QString hit;
            for (int i = document->state.layers.size() - 1; i >= 0; --i) {
                const auto &candidate = document->state.layers[i];
                if (candidate.isGroup() || !candidate.visible)
                    continue;
                bool visible = true;
                QString parent = candidate.parent;
                while (!parent.isEmpty()) {
                    int pi = document->index(parent);
                    if (pi < 0) {
                        visible = false;
                        break;
                    }
                    visible = visible && document->state.layers[pi].visible;
                    parent = document->state.layers[pi].parent;
                }
                if (!visible)
                    continue;
                QPoint local = candidate.transform().inverted().map(start).toPoint();
                QImage pixels = candidate.pixels(QRect(local, QSize(1, 1)));
                if (!pixels.isNull() && qAlpha(pixels.pixel(0, 0)) > 0) {
                    hit = candidate.id;
                    break;
                }
            }
            if (!hit.isEmpty()) {
                if (!document->selectedMembers().contains(hit))
                    document->setSelected({hit}, hit);
                else
                    document->notify();
            } else {
                pressed = false;
                return;
            }
        }
        if (!document->active()) {
            pressed = false;
            return;
        }
        originalLayers = document->state.layers;
        document->begin(scaling ? "Scale layer" : "Move layer");
    } else if (tool == Tool::Wand) {
        QString error;
        if (!document->magicWand(start.toPoint(), tolerance, modifiers.testFlag(Qt::ShiftModifier),
                                 modifiers.testFlag(Qt::AltModifier), &error) &&
            message)
            message(error);
        pressed = false;
    } else if (tool == Tool::Text) {
        pressed = false;
        if (createText)
            createText(start);
    } else if (tool == Tool::Eyedropper) {
        QImage image = document->composite();
        QColor color = image.pixelColor(start.toPoint());
        if (colorPicked)
            colorPicked(color);
        pressed = false;
    } else if (tool == Tool::Lasso) {
        draft = {};
        draft.moveTo(start);
    }
}
void Canvas::mouseMoveEvent(QMouseEvent *e) {
    cursor = documentPoint(e->position());
    if (!pressed) {
        update();
        return;
    }
    if (panning) {
        pan = panStart + e->position() - widgetStart;
        update();
        return;
    }
    if (tool == Tool::Brush || tool == Tool::Eraser || tool == Tool::Clone || tool == Tool::Heal) {
        double diameter = brushSize * (pressureSize ? pressure : 1),
               alpha = brushOpacity * (pressureOpacity ? pressure : 1);
        if (tool == Tool::Clone || tool == Tool::Heal)
            document->repair(last, cursor, repairOffset, diameter, alpha, tool == Tool::Heal, repairSource);
        else
            document->paint(last, cursor, diameter, foreground, alpha, tool == Tool::Eraser, maskTarget);
        last = cursor;
        update();
    } else if (tool == Tool::Move) {
        if (movingSelection) {
            QTransform t;
            t.translate(cursor.x() - start.x(), cursor.y() - start.y());
            draft = t.map(oldSelection);
            update();
            return;
        }
        Layer *l = document->active();
        if (!l)
            return;
        int idx = document->index(l->id);
        const auto &original = originalLayers[idx];
        if (scaling) {
            QSizeF source = original.image.isNull() ? original.size : original.image.size();
            QPointF local = original.transform().inverted().map(cursor);
            double sx = qMax(1.0, local.x()) / source.width(), sy = qMax(1.0, local.y()) / source.height();
            if (modifiers.testFlag(Qt::ShiftModifier))
                sx = sy = qMax(sx, sy);
            QSizeF size(qBound(1.0, original.size.width() * sx, double(MaxSide)),
                        qBound(1.0, original.size.height() * sy, double(MaxSide)));
            if (original.kind != "raster" &&
                (!Document::validSize(size.toSize(), MaxSourcePixels) ||
                 document->pixelCount() -
                         qint64(qRound(original.size.width())) * qRound(original.size.height()) +
                         qint64(qRound(size.width())) * qRound(size.height()) >
                     MaxSourcePixels))
                return;
            if (original.image.isNull() && !original.mask.isNull())
                l->mask = original.mask.scaled(size.toSize());
            l->size = size;
            l->position = original.position;
            QPointF corner = original.transform().map(QPointF());
            l->position += corner - l->transform().map(QPointF());
        } else {
            auto ids = document->selectedMembers();
            QPointF delta = cursor - start;
            for (int i = 0; i < document->state.layers.size(); ++i)
                if (ids.contains(document->state.layers[i].id))
                    document->state.layers[i].position = originalLayers[i].position + delta;
        }
        gestureEdited = true;
        invalidate();
    } else if (tool == Tool::Lasso) {
        draft.lineTo(cursor);
        update();
    } else {
        QRectF box(start, cursor);
        box = box.normalized();
        if (modifiers.testFlag(Qt::ShiftModifier)) {
            double side = qMin(box.width(), box.height());
            box.setSize(QSizeF(side, side));
        }
        draft = {};
        if (tool == Tool::EllipseSelect || tool == Tool::EllipseShape)
            draft.addEllipse(box);
        else
            draft.addRect(box);
        update();
    }
}
void Canvas::mouseReleaseEvent(QMouseEvent *e) {
    if (!pressed)
        return;
    if (e->button() != Qt::LeftButton && e->button() != Qt::MiddleButton)
        return;
    if (panning) {
        pressed = panning = false;
        updateCursor();
        return;
    }
    if (movingSelection) {
        QString error;
        if (!document->moveSelection(documentPoint(e->position()) - start,
                                     modifiers.testFlag(Qt::AltModifier), &error) &&
            message)
            message(error);
        movingSelection = false;
    } else if (tool == Tool::Brush || tool == Tool::Eraser || tool == Tool::Clone || tool == Tool::Heal ||
               tool == Tool::Move) {
        if (gestureEdited)
            document->commit();
        else
            document->cancel();
    } else if (tool == Tool::RectangleSelect || tool == Tool::EllipseSelect || tool == Tool::Lasso) {
        if (tool == Tool::Lasso)
            draft.closeSubpath();
        selection(draft);
    } else if (tool == Tool::Crop) {
        document->crop(draft.boundingRect().toAlignedRect());
    } else if (tool == Tool::RectangleShape || tool == Tool::EllipseShape) {
        QRectF bounds = draft.boundingRect();
        QString error;
        if (bounds.width() >= 1 && bounds.height() >= 1 &&
            !document->addShape(tool == Tool::EllipseShape ? "ellipse" : "rectangle", foreground, bounds,
                                &error) &&
            message)
            message(error);
    }
    draft = {};
    pressed = false;
    originalLayers.clear();
    repairSource = {};
    update();
}
void Canvas::tabletEvent(QTabletEvent *e) {
    if (tool != Tool::Brush && tool != Tool::Eraser && tool != Tool::Clone && tool != Tool::Heal) {
        e->ignore();
        return;
    }
    pressure = qBound(.02, e->pressure(), 1.0);
    QEvent::Type type = e->type() == QEvent::TabletPress     ? QEvent::MouseButtonPress
                        : e->type() == QEvent::TabletRelease ? QEvent::MouseButtonRelease
                                                             : QEvent::MouseMove;
    QMouseEvent mouse(type, e->position(), e->globalPosition(),
                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton,
                      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, e->modifiers());
    if (type == QEvent::MouseButtonPress) {
        mousePressEvent(&mouse);
    } else if (type == QEvent::MouseButtonRelease) {
        mouseReleaseEvent(&mouse);
        pressure = 1;
    } else
        mouseMoveEvent(&mouse);
    e->accept();
}
void Canvas::wheelEvent(QWheelEvent *e) {
    QPointF anchor = documentPoint(e->position());
    scale = qBound(.02, scale * std::pow(1.0015, e->angleDelta().y()), 32.0);
    pan += e->position() - widgetPoint(anchor);
    update();
    if (message)
        message(QString("Zoom %1%").arg(qRound(scale * 100)));
    e->accept();
}
void Canvas::abortGesture() {
    if (pressed && !panning &&
        (tool == Tool::Brush || tool == Tool::Eraser || tool == Tool::Clone || tool == Tool::Heal ||
         tool == Tool::Move))
        document->cancel();
    draft = {};
    repairSource = {};
    movingSelection = false;
    pressed = panning = false;
    originalLayers.clear();
    invalidate();
}
void Canvas::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) {
        space = true;
        updateCursor();
        e->accept();
    } else if (e->key() == Qt::Key_Escape) {
        abortGesture();
        e->accept();
    } else
        QWidget::keyPressEvent(e);
}
void Canvas::keyReleaseEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Space) {
        space = false;
        updateCursor();
        e->accept();
    } else
        QWidget::keyReleaseEvent(e);
}
void Canvas::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}
void Canvas::dropEvent(QDropEvent *e) {
    QStringList files;
    for (const auto &url : e->mimeData()->urls())
        if (url.isLocalFile())
            files << url.toLocalFile();
    if (filesDropped)
        filesDropped(files);
    e->acceptProposedAction();
}
} // namespace ps
