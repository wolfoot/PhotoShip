#pragma once
#include "canvas.h"
#include "persistence.h"
#include <QMainWindow>
#include <QMap>
#include <QSet>
#include <QTimer>
#include <memory>
#include <vector>
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QLabel;
class QSpinBox;
class QAction;
namespace ps {
class Window : public QMainWindow {
    Q_OBJECT
  public:
    Window();
    ~Window() override;
    bool openPath(const QString &path);
    void newDocument(QSize size = QSize(1280, 800));
    Document *currentDocument() const;
    Canvas *currentCanvas() const;
    void demo();
    bool writeScreenshot(const QString &path);

  protected:
    void closeEvent(QCloseEvent *) override;
    void changeEvent(QEvent *) override;

  private:
    std::vector<std::unique_ptr<Document>> documents;
    QTabWidget *tabs = nullptr;
    QTreeWidget *layers;
    QComboBox *blend, *parentGroup;
    QDoubleSpinBox *opacity, *x, *y, *layerWidth, *layerHeight, *rotation;
    QCheckBox *maskTarget, *maskEnabled;
    QPushButton *colorButton;
    QLabel *details;
    QSpinBox *brushSize, *brushOpacity, *tolerance;
    QAction *undoAction, *redoAction;
    Tool currentTool = Tool::Move;
    QColor foreground = QColor("#628cff");
    bool syncing = false, closeWindowAfterSave = false;
    Persistence *persistence;
    QTimer recoveryTimer, recoveryInterval;
    QSet<Document *> closeAfterSave;
    QMap<QString, quint64> recoveredRevision;
    QSet<QString> recovering;
    void recoverDocuments();
    void snapshotRecovery();
    void clearRecovery(Document *document);
    void addDocument(Document document);
    void refresh();
    void setTool(Tool tool);
    void setColor(QColor color);
    void tools();
    void panels();
    void menus();
    void newDialog();
    void importImages(const QStringList &paths = {});
    bool save(bool saveAs = false);
    void exportDialog();
    bool confirmClose(Document *document);
    void closeTab(int index);
    void editText(QPointF at = QPointF(-1, -1));
    void adjustDialog(const QString &kind);
    void maskAction(const QString &action);
    void transformChanged();
    void error(const QString &message);
    void run(const std::function<bool(Document *, QString *)> &operation);
    QAction *action(const QString &name, const QKeySequence &key, const std::function<void()> &callback);
};
} // namespace ps
