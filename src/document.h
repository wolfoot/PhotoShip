#pragma once
#include <QColor>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QUuid>
#include <QVector>
#include <functional>

namespace ps {
constexpr qint64 MaxCanvasPixels = 16'000'000;
constexpr qint64 MaxSourcePixels = 32'000'000;
constexpr int MaxSide = 8192;
constexpr int MaxLayers = 256;
struct Layer {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name = "Layer";
    QString parent;
    QString kind = "raster";
    QImage image, mask;
    bool visible = true, maskEnabled = true;
    bool flipX = false, flipY = false, nearest = false;
    double opacity = 1;
    QString blend = "Normal";
    QPointF position;
    QSizeF size;
    double rotation = 0;
    QString text;
    QFont font = QFont("Sans Serif", 36);
    QColor color = Qt::black;
    QString shape = "rectangle";
    bool isGroup() const {
        return kind == "group";
    }
    QTransform transform() const;
    QImage pixels() const;
};
struct State {
    QSize size = QSize(1280, 800);
    double resolution = 72;
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QVector<Layer> layers;
    QString active;
    QPainterPath selection;
};
struct HistoryEntry {
    State state;
    QString label;
    quint64 revision;
};
class Document {
  public:
    State state;
    QString filePath;
    quint64 revision = 0, savedRevision = 0;
    QVector<HistoryEntry> past, future;
    std::function<void()> changed;
    static Document create(QSize size);
    Layer *active();
    const Layer *active() const;
    int index(const QString &id) const;
    bool descendant(const Layer &layer, const QString &id) const;
    QStringList descendants(const QString &id) const;
    qint64 pixelCount() const;
    void begin(const QString &label);
    void commit();
    void cancel();
    void edit(const QString &label, const std::function<void()> &operation);
    void undo();
    void redo();
    bool dirty() const {
        return revision != savedRevision;
    }
    void markSaved() {
        savedRevision = revision;
        notify();
    }
    void notify() {
        if (changed)
            changed();
    }
    bool addImage(const QImage &image, const QString &name, QString *error = nullptr);
    bool addBlank(QString *error = nullptr);
    bool addGroup(QString *error = nullptr);
    bool addText(const QString &text, QFont font, QColor color, QRectF bounds, QString *error = nullptr);
    bool addShape(QString shape, QColor color, QRectF bounds, QString *error = nullptr);
    bool duplicate(QString *error = nullptr);
    void remove();
    void reorder(int direction);
    void reparent(const QString &parent);
    void crop(QRect bounds);
    void paint(QPointF from, QPointF to, double diameter, QColor color, double opacity, bool erase,
               bool mask);
    void fill(QColor color, bool mask);
    bool magicWand(QPoint point, int tolerance, bool add, bool subtract, QString *error = nullptr);
    void adjust(const QString &kind, double first, double second, double third,
                const QVector<QPointF> &curve = {});
    QImage composite() const;
    void render(QPainter &painter) const;
    static QStringList blendModes();
    static bool validSize(QSize size, qint64 budget = MaxCanvasPixels);

  private:
    HistoryEntry pending;
    bool editing = false;
    quint64 nextRevision = 1;
    void trimHistory();
    bool canAdd(qint64 pixels, int count, QString *error) const;
    void insert(Layer layer);
};
bool loadProject(const QString &path, State &result, QString &error);
bool saveProject(const QString &path, const State &state, QString &error);
bool readImage(const QString &path, QImage &result, QString &error);
bool exportImage(const QString &path, const QImage &image, int quality, QString &error);
} // namespace ps
