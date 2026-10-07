#pragma once
#include "document.h"
#include <QCache>
#include <QWidget>
#include <functional>
namespace ps {
enum class Tool {
    Move,
    Brush,
    Eraser,
    Clone,
    Heal,
    RectangleSelect,
    EllipseSelect,
    Lasso,
    Wand,
    Crop,
    RectangleShape,
    EllipseShape,
    Text,
    Eyedropper,
    Hand
};
class Canvas : public QWidget {
    Q_OBJECT
  public:
    explicit Canvas(Document *document, QWidget *parent = nullptr);
    Document *document;
    Tool tool = Tool::Move;
    QColor foreground = QColor("#628cff");
    double brushSize = 32, brushOpacity = 1;
    bool maskTarget = false;
    int tolerance = 32;
    std::function<void(QPointF)> createText;
    std::function<void(QColor)> colorPicked;
    std::function<void(const QString &)> message;
    std::function<void(const QStringList &)> filesDropped;
    void invalidate();
    void invalidateRegion(QRect region);
    int tilesRendered = 0;
    bool pressureSize = true, pressureOpacity = true;
    void setTool(Tool value);
    void fit();
    void actualSize();
    void zoomBy(double factor);
    double zoom() const {
        return scale;
    }
    QPointF documentPoint(QPointF point) const;
    QPointF widgetPoint(QPointF point) const;
    void abortGesture();

  protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void tabletEvent(QTabletEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void dragEnterEvent(QDragEnterEvent *) override;
    void dropEvent(QDropEvent *) override;

  private:
    QCache<quint64, QImage> tiles{256};
    QImage repairSource;
    QPointF samplePoint, repairOffset;
    QString sampleLayer;
    bool hasSample = false, movingSelection = false;
    double pressure = 1;
    bool pressed = false, panning = false, space = false, hasResized = false, gestureEdited = false;
    bool scaling = false;
    double scale = 1;
    QPointF pan, start, last, panStart, widgetStart, cursor;
    QPainterPath draft, oldSelection;
    Qt::KeyboardModifiers modifiers;
    QVector<Layer> originalLayers;
    QTransform view() const;
    void selection(QPainterPath path);
    void updateCursor();
};
} // namespace ps
