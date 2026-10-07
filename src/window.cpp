#include "window.h"
#include "i18n.h"
#include "psd.h"
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
namespace ps {
class LayerTree : public QTreeWidget {
  public:
    std::function<void(QStringList, QString, QString)> moveLayers;
    void dropEvent(QDropEvent *event) override {
        QStringList ids;
        for (auto *item : selectedItems())
            ids << item->data(0, Qt::UserRole).toString();
        auto *target = itemAt(event->position().toPoint());
        QString parent, before;
        auto pos = dropIndicatorPosition();
        if (target) {
            auto id = target->data(0, Qt::UserRole).toString();
            if (pos == OnItem && target->data(0, Qt::UserRole + 1).toBool())
                parent = id;
            else {
                auto *container = target->parent();
                parent = container ? container->data(0, Qt::UserRole).toString() : QString();
                if (pos == BelowItem)
                    before = id;
                else {
                    int i = container ? container->indexOfChild(target) : indexOfTopLevelItem(target);
                    auto *above =
                        i > 0 ? (container ? container->child(i - 1) : topLevelItem(i - 1)) : nullptr;
                    if (above)
                        before = above->data(0, Qt::UserRole).toString();
                }
            }
        }
        if (!target && topLevelItemCount())
            before = topLevelItem(topLevelItemCount() - 1)->data(0, Qt::UserRole).toString();
        event->ignore();
        if (moveLayers)
            moveLayers(ids, parent, before);
    }
};
Window::Window() {
    persistence = new Persistence(this);
    recoveryTimer.setSingleShot(true);
    recoveryTimer.setInterval(1500);
    recoveryInterval.setInterval(30000);
    connect(&recoveryTimer, &QTimer::timeout, this, &Window::snapshotRecovery);
    connect(&recoveryInterval, &QTimer::timeout, this, &Window::snapshotRecovery);
    recoveryInterval.start();
    setObjectName("editorWindow");
    resize(1440, 920);
    setMinimumSize(900, 620);
    setWindowTitle("PhotoShip");
    setDockNestingEnabled(true);
    tabs = new QTabWidget(this);
    tabs->setObjectName("documents");
    tabs->setDocumentMode(true);
    tabs->setTabsClosable(true);
    setCentralWidget(tabs);
    tools();
    panels();
    menus();
    translateWidgets(this);
    for (int i = 0; i < blend->count(); ++i)
        blend->setItemText(i, ui(blend->itemData(i).toString()));
    setWindowIcon(applicationIcon());
    connect(tabs, &QTabWidget::currentChanged, this, [this]() {
        for (const auto &d : documents)
            for (int i = 0; i < tabs->count(); ++i) {
                auto *c = qobject_cast<Canvas *>(tabs->widget(i));
                if (c && c->document == d.get() && c != currentCanvas())
                    c->abortGesture();
            }
        refresh();
    });
    connect(tabs, &QTabWidget::tabCloseRequested, this, &Window::closeTab);
    statusBar()->showMessage(ui("Ready  •  Drop images onto the canvas to import them"));
    newDocument();
    if (!qEnvironmentVariableIsSet("PHOTOSHIP_TESTING") && !qEnvironmentVariableIsSet("PIXELSTUDIO_TESTING"))
        QTimer::singleShot(0, this, &Window::recoverDocuments);
}
void Window::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() != QEvent::LanguageChange || !tabs)
        return;
    translateWidgets(this);
    {
        QSignalBlocker blocker(blend);
        for (int i = 0; i < blend->count(); ++i)
            blend->setItemText(i, ui(blend->itemData(i).toString()));
    }
    for (auto *choice : findChildren<QAction *>())
        if (choice->property("languageCode").isValid())
            choice->setChecked(choice->property("languageCode").toString() == currentLanguage());
    setColor(foreground);
    refresh();
    statusBar()->showMessage(ui("Ready  •  Drop images onto the canvas to import them"));
}
Window::~Window() {
    recoveryTimer.stop();
    recoveryInterval.stop();
    persistence->wait();
}
Document *Window::currentDocument() const {
    auto *c = currentCanvas();
    return c ? c->document : nullptr;
}
Canvas *Window::currentCanvas() const {
    return qobject_cast<Canvas *>(tabs->currentWidget());
}
QAction *Window::action(const QString &name, const QKeySequence &key, const std::function<void()> &callback) {
    auto *a = new QAction(name, this);
    a->setShortcut(key);
    connect(a, &QAction::triggered, this, [this, callback]() {
        if (currentCanvas())
            currentCanvas()->abortGesture();
        callback();
    });
    return a;
}
void Window::addDocument(Document document) {
    auto d = std::make_unique<Document>(std::move(document));
    auto *raw = d.get();
    if (raw->recoveryPath.isEmpty())
        raw->recoveryPath = QDir(recoveryDirectory())
                                .filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".psproj");
    documents.push_back(std::move(d));
    auto *c = new Canvas(raw, this);
    c->setTool(currentTool);
    c->foreground = foreground;
    c->pressureSize = findChild<QCheckBox *>("pressureSize")->isChecked();
    c->pressureOpacity = findChild<QCheckBox *>("pressureOpacity")->isChecked();
    c->brushSize = brushSize->value();
    c->brushOpacity = brushOpacity->value() / 100.0;
    c->tolerance = tolerance->value();
    c->message = [this](const QString &message) { statusBar()->showMessage(ui(message), 7000); };
    c->colorPicked = [this](QColor color) { setColor(color); };
    c->createText = [this](QPointF at) { editText(at); };
    c->filesDropped = [this](const QStringList &paths) { importImages(paths); };
    raw->regionChanged = [c](QRect region) { c->invalidateRegion(region); };
    raw->changed = [this, c, raw]() {
        if (raw->lastChange.isEmpty())
            c->invalidate();
        else
            c->invalidateRegion(raw->lastChange);
        refresh();
        if (raw->dirty() && !raw->isEditing())
            recoveryTimer.start();
    };
    int i = tabs->addTab(c, "Untitled");
    tabs->setCurrentIndex(i);
    refresh();
}
void Window::newDocument(QSize size) {
    if (!Document::validSize(size)) {
        error("Canvas must be at most 8192 pixels per side and 16 million pixels total.");
        return;
    }
    addDocument(Document::create(size));
}
void Window::error(const QString &message) {
    QMessageBox::warning(this, "PhotoShip",
                         ui(message.isEmpty() ? "The operation could not be completed." : message));
}
void Window::run(const std::function<bool(Document *, QString *)> &op) {
    if (!currentDocument())
        return;
    QString message;
    if (!op(currentDocument(), &message))
        error(message);
}
void Window::setColor(QColor color) {
    if (!color.isValid())
        return;
    foreground = color;
    colorButton->setStyleSheet("background: " + color.name() +
                               "; color: " + (color.lightness() < 128 ? "white" : "black") +
                               "; border:1px solid #818997; border-radius:4px;");
    colorButton->setText(color.name().toUpper());
    for (int i = 0; i < tabs->count(); ++i)
        qobject_cast<Canvas *>(tabs->widget(i))->foreground = color;
}
void Window::setTool(Tool tool) {
    currentTool = tool;
    for (int i = 0; i < tabs->count(); ++i)
        qobject_cast<Canvas *>(tabs->widget(i))->setTool(tool);
}
void Window::tools() {
    auto *toolbar = addToolBar("Tools");
    toolbar->setObjectName("toolPalette");
    toolbar->setMovable(false);
    addToolBar(Qt::LeftToolBarArea, toolbar);
    toolbar->setOrientation(Qt::Vertical);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto *group = new QActionGroup(this);
    group->setExclusive(true);
    struct Definition {
        QString label;
        QString key;
        Tool tool;
    };
    const QVector<Definition> definitions = {{"V  Move", "V", Tool::Move},
                                             {"B  Brush", "B", Tool::Brush},
                                             {"E  Eraser", "E", Tool::Eraser},
                                             {"S  Clone", "S", Tool::Clone},
                                             {"J  Heal", "J", Tool::Heal},
                                             {"M  Marquee", "M", Tool::RectangleSelect},
                                             {"◯  Ellipse", "Shift+M", Tool::EllipseSelect},
                                             {"L  Lasso", "L", Tool::Lasso},
                                             {"W  Wand", "W", Tool::Wand},
                                             {"C  Crop", "C", Tool::Crop},
                                             {"U  Rectangle", "U", Tool::RectangleShape},
                                             {"◯  Shape", "Shift+U", Tool::EllipseShape},
                                             {"T  Text", "T", Tool::Text},
                                             {"I  Picker", "I", Tool::Eyedropper},
                                             {"H  Hand", "H", Tool::Hand}};
    for (const auto &def : definitions) {
        auto *a = action(def.label, QKeySequence(def.key), [this, def]() { setTool(def.tool); });
        a->setCheckable(true);
        if (def.tool == Tool::Move)
            a->setChecked(true);
        group->addAction(a);
        toolbar->addAction(a);
    }
    auto *options = addToolBar("Tool options");
    options->setObjectName("toolOptions");
    options->setMovable(false);
    options->addWidget(new QLabel("  PHOTOSHIP    "));
    auto *pressureSize = new QCheckBox("Pressure size");
    pressureSize->setObjectName("pressureSize");
    pressureSize->setChecked(true);
    options->addWidget(pressureSize);
    auto *pressureOpacity = new QCheckBox("Pressure opacity");
    pressureOpacity->setObjectName("pressureOpacity");
    pressureOpacity->setChecked(true);
    options->addWidget(pressureOpacity);
    auto syncPressure = [this, pressureSize, pressureOpacity] {
        for (int i = 0; i < tabs->count(); ++i) {
            auto *c = qobject_cast<Canvas *>(tabs->widget(i));
            c->pressureSize = pressureSize->isChecked();
            c->pressureOpacity = pressureOpacity->isChecked();
        }
    };
    connect(pressureSize, &QCheckBox::toggled, this, syncPressure);
    connect(pressureOpacity, &QCheckBox::toggled, this, syncPressure);
    colorButton = new QPushButton;
    colorButton->setMinimumWidth(95);
    colorButton->setObjectName("foregroundColor");
    options->addWidget(colorButton);
    connect(colorButton, &QPushButton::clicked, this, [this]() {
        setColor(QColorDialog::getColor(foreground, this, ui("Foreground color"),
                                        QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog));
    });
    setColor(foreground);
    options->addSeparator();
    options->addWidget(new QLabel("  Size "));
    brushSize = new QSpinBox;
    brushSize->setObjectName("brushSize");
    brushSize->setRange(1, 1000);
    brushSize->setValue(32);
    brushSize->setSuffix(" px");
    options->addWidget(brushSize);
    options->addWidget(new QLabel("  Opacity "));
    brushOpacity = new QSpinBox;
    brushOpacity->setRange(1, 100);
    brushOpacity->setValue(100);
    brushOpacity->setSuffix(" %");
    options->addWidget(brushOpacity);
    options->addWidget(new QLabel("  Wand tolerance "));
    tolerance = new QSpinBox;
    tolerance->setRange(0, 255);
    tolerance->setValue(32);
    options->addWidget(tolerance);
    connect(brushSize, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        for (int i = 0; i < tabs->count(); ++i)
            qobject_cast<Canvas *>(tabs->widget(i))->brushSize = v;
    });
    connect(brushOpacity, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        for (int i = 0; i < tabs->count(); ++i)
            qobject_cast<Canvas *>(tabs->widget(i))->brushOpacity = v / 100.0;
    });
    connect(tolerance, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
        for (int i = 0; i < tabs->count(); ++i)
            qobject_cast<Canvas *>(tabs->widget(i))->tolerance = v;
    });
    addAction(action("Smaller brush", QKeySequence("["),
                     [this]() { brushSize->setValue(qMax(1, brushSize->value() - 5)); }));
    addAction(
        action("Larger brush", QKeySequence("]"), [this]() { brushSize->setValue(brushSize->value() + 5); }));
}
void Window::panels() {
    auto *dock = new QDockWidget("Layers", this);
    dock->setObjectName("layersDock");
    dock->setMinimumWidth(270);
    auto *body = new QWidget;
    auto *layout = new QVBoxLayout(body);
    auto *form = new QFormLayout;
    blend = new QComboBox;
    blend->setObjectName("blendMode");
    for (const auto &mode : Document::blendModes())
        blend->addItem(mode, mode);
    form->addRow("Blend", blend);
    opacity = new QDoubleSpinBox;
    opacity->setRange(0, 100);
    opacity->setDecimals(1);
    opacity->setSuffix(" %");
    opacity->setObjectName("layerOpacity");
    form->addRow("Opacity", opacity);
    layout->addLayout(form);
    auto *tree = new LayerTree;
    layers = tree;
    tree->moveLayers = [this](QStringList ids, QString parent, QString before) {
        QString session = currentDocument() ? currentDocument()->recoveryPath : QString();
        QTimer::singleShot(0, this, [this, ids, parent, before, session] {
            if (!currentDocument() || currentDocument()->recoveryPath != session)
                return;
            currentCanvas()->abortGesture();
            run([&](Document *d, QString *e) { return d->dropLayers(ids, parent, before, e); });
        });
    };
    layers->setDragDropMode(QAbstractItemView::InternalMove);
    layers->setDefaultDropAction(Qt::MoveAction);
    layers->setObjectName("layerTree");
    layers->setHeaderHidden(true);
    layers->setIconSize(QSize(38, 30));
    layers->setSelectionMode(QAbstractItemView::ExtendedSelection);
    layout->addWidget(layers);
    auto *buttons = new QHBoxLayout;
    auto button = [&](const QString &name, const std::function<void()> &callback) {
        auto *b = new QPushButton(name);
        b->setMaximumWidth(60);
        buttons->addWidget(b);
        connect(b, &QPushButton::clicked, this, callback);
    };
    button("+", [this]() { run([](Document *d, QString *e) { return d->addBlank(e); }); });
    button("Group", [this]() { run([](Document *d, QString *e) { return d->addGroup(e); }); });
    button("Copy", [this]() { run([](Document *d, QString *e) { return d->duplicate(e); }); });
    button("↑", [this]() {
        if (currentDocument())
            currentDocument()->reorder(1);
    });
    button("↓", [this]() {
        if (currentDocument())
            currentDocument()->reorder(-1);
    });
    button("−", [this]() {
        if (currentDocument())
            currentDocument()->remove();
    });
    layout->addLayout(buttons);
    auto *maskRow = new QHBoxLayout;
    maskTarget = new QCheckBox("Edit mask");
    maskTarget->setObjectName("maskTarget");
    maskEnabled = new QCheckBox("Mask enabled");
    maskRow->addWidget(maskTarget);
    maskRow->addWidget(maskEnabled);
    layout->addLayout(maskRow);
    dock->setWidget(body);
    addDockWidget(Qt::RightDockWidgetArea, dock);
    connect(maskTarget, &QCheckBox::toggled, this, [this](bool on) {
        if (syncing)
            return;
        if (auto *c = currentCanvas())
            c->maskTarget = on;
    });
    connect(maskEnabled, &QCheckBox::toggled, this, [this](bool on) {
        if (syncing)
            return;
        auto *d = currentDocument();
        if (d && d->active())
            d->edit("Toggle mask", [&]() { d->active()->maskEnabled = on; });
    });
    connect(blend, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        const QString value = blend->currentData().toString();
        if (syncing)
            return;
        auto *d = currentDocument();
        if (d && d->active() && !d->active()->isGroup() && d->active()->blend != value)
            d->edit("Blend mode", [&]() { d->active()->blend = value; });
    });
    connect(opacity, &QDoubleSpinBox::editingFinished, this, [this]() {
        if (syncing)
            return;
        auto *d = currentDocument();
        if (d && d->active() && d->active()->opacity != opacity->value() / 100)
            d->edit("Layer opacity", [&]() { d->active()->opacity = opacity->value() / 100; });
    });
    connect(layers, &QTreeWidget::itemSelectionChanged, this, [this] {
        if (syncing || !currentDocument())
            return;
        QStringList ids;
        for (auto *item : layers->selectedItems())
            ids << item->data(0, Qt::UserRole).toString();
        QString active =
            layers->currentItem() ? layers->currentItem()->data(0, Qt::UserRole).toString() : QString();
        QString session = currentDocument()->recoveryPath;
        // Rebuilding the tree inside its selection signal destroys items still in use by Qt.
        QTimer::singleShot(0, this, [this, ids, active, session] {
            auto *d = currentDocument();
            if (!d || d->recoveryPath != session)
                return;
            currentCanvas()->abortGesture();
            currentCanvas()->maskTarget = false;
            d->setSelected(ids, active);
        });
    });
    connect(layers, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int) {
        if (syncing)
            return;
        auto *d = currentDocument();
        if (!d)
            return;
        int i = d->index(item->data(0, Qt::UserRole).toString());
        if (i < 0)
            return;
        QString name = item->text(0).left(4096);
        bool visible = item->checkState(0) == Qt::Checked;
        if (d->state.layers[i].name == name && d->state.layers[i].visible == visible)
            return;
        d->edit("Layer properties", [&]() {
            d->state.layers[i].name = name;
            d->state.layers[i].visible = visible;
        });
    });
    connect(layers, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *, int) {
        if (currentDocument() && currentDocument()->active() && currentDocument()->active()->kind == "text")
            editText();
    });
    auto *inspector = new QDockWidget("Properties", this);
    inspector->setObjectName("propertiesDock");
    auto *properties = new QWidget;
    auto *propertyLayout = new QVBoxLayout(properties);
    auto *transform = new QFormLayout;
    auto spin = [&](const QString &label, double min, double max) {
        auto *s = new QDoubleSpinBox;
        s->setRange(min, max);
        s->setDecimals(1);
        s->setKeyboardTracking(false);
        transform->addRow(label, s);
        connect(s, &QDoubleSpinBox::editingFinished, this, &Window::transformChanged);
        return s;
    };
    x = spin("X", -1000000, 1000000);
    y = spin("Y", -1000000, 1000000);
    layerWidth = spin("Width", 1, MaxSide);
    layerHeight = spin("Height", 1, MaxSide);
    rotation = spin("Rotation", -360000, 360000);
    rotation->setSuffix("°");
    parentGroup = new QComboBox;
    transform->addRow("Group", parentGroup);
    propertyLayout->addLayout(transform);
    details = new QLabel;
    details->setWordWrap(true);
    propertyLayout->addWidget(details);
    auto *editTextButton = new QPushButton("Edit text / shape");
    propertyLayout->addWidget(editTextButton);
    propertyLayout->addStretch();
    inspector->setWidget(properties);
    addDockWidget(Qt::RightDockWidgetArea, inspector);
    resizeDocks({dock, inspector}, {430, 290}, Qt::Vertical);
    connect(parentGroup, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        if (!syncing && currentDocument())
            currentDocument()->reparent(parentGroup->currentData().toString());
    });
    connect(editTextButton, &QPushButton::clicked, this, [this]() {
        auto *d = currentDocument();
        if (!d || !d->active())
            return;
        if (d->active()->kind == "text")
            editText();
        else if (d->active()->kind == "shape") {
            QColor color =
                QColorDialog::getColor(d->active()->color, this, ui("Shape color"),
                                       QColorDialog::ShowAlphaChannel | QColorDialog::DontUseNativeDialog);
            if (color.isValid())
                d->edit("Shape color", [&]() { d->active()->color = color; });
        } else
            statusBar()->showMessage(ui("Select a text or shape layer."), 5000);
    });
}
void Window::menus() {
    auto *file = menuBar()->addMenu("&File");
    file->addAction(action("&New…", QKeySequence::New, [this]() { newDialog(); }));
    file->addAction(action("&Open image / project…", QKeySequence::Open, [this]() {
        QString path = QFileDialog::getOpenFileName(this, ui("Open image or project"), {},
                                                    ui("Images / PSD / Project manifest (*.png *.jpg *.jpeg "
                                                       "*.bmp *.webp *.psd manifest.json);;All files (*)"),
                                                    nullptr, QFileDialog::DontUseNativeDialog);
        if (!path.isEmpty())
            openPath(path);
    }));
    file->addAction(action("Open project folder…", {}, [this]() {
        QString path =
            QFileDialog::getExistingDirectory(this, ui("Open .psproj or .comp folder"), {},
                                              QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
        if (!path.isEmpty())
            openPath(path);
    }));
    file->addAction(
        action("Import images as layers…", QKeySequence("Ctrl+Shift+O"), [this]() { importImages(); }));
    file->addSeparator();
    file->addAction(action("&Save project", QKeySequence::Save, [this]() { save(); }));
    file->addAction(action("Save project &as…", QKeySequence::SaveAs, [this]() { save(true); }));
    file->addAction(action("Export PNG / JPEG…", QKeySequence("Ctrl+Shift+E"), [this]() { exportDialog(); }));
    file->addSeparator();
    file->addAction(
        action("Close document", QKeySequence::Close, [this]() { closeTab(tabs->currentIndex()); }));
    file->addAction(action("Exit", QKeySequence::Quit, [this]() { close(); }));
    auto *edit = menuBar()->addMenu("&Edit");
    undoAction = action("Undo", QKeySequence::Undo, [this]() {
        if (currentDocument())
            currentDocument()->undo();
    });
    redoAction = action("Redo", QKeySequence::Redo, [this]() {
        if (currentDocument())
            currentDocument()->redo();
    });
    edit->addAction(undoAction);
    edit->addAction(redoAction);
    auto *redoAlt = action("Redo", QKeySequence("Ctrl+Shift+Z"), [this]() {
        if (currentDocument())
            currentDocument()->redo();
    });
    addAction(redoAlt);
    edit->addSeparator();
    edit->addAction(action("Copy merged", QKeySequence("Ctrl+Shift+C"), [this]() {
        if (currentDocument())
            QApplication::clipboard()->setImage(currentDocument()->composite());
    }));
    edit->addAction(action("Paste image as layer", QKeySequence::Paste, [this]() {
        QImage image = QApplication::clipboard()->image();
        if (!image.isNull())
            run([&](Document *d, QString *e) { return d->addImage(image, "Clipboard", e); });
    }));
    edit->addAction(action("Fill with foreground", QKeySequence("Alt+Backspace"), [this]() {
        if (currentDocument())
            currentDocument()->fill(foreground, currentCanvas()->maskTarget);
    }));
    auto *layer = menuBar()->addMenu("&Layer");
    layer->addAction(action("New raster layer", QKeySequence("Ctrl+Shift+N"),
                            [this]() { run([](Document *d, QString *e) { return d->addBlank(e); }); }));
    layer->addAction(action("New group", QKeySequence("Ctrl+G"),
                            [this]() { run([](Document *d, QString *e) { return d->addGroup(e); }); }));
    layer->addAction(action("Duplicate", QKeySequence("Ctrl+J"),
                            [this]() { run([](Document *d, QString *e) { return d->duplicate(e); }); }));
    layer->addAction(action("Delete layer", QKeySequence("Delete"), [this]() {
        if (currentDocument())
            currentDocument()->remove();
    }));
    layer->addAction(action("Raise layer", QKeySequence("Ctrl+]"), [this]() {
        if (currentDocument())
            currentDocument()->reorder(1);
    }));
    layer->addAction(action("Lower layer", QKeySequence("Ctrl+["), [this]() {
        if (currentDocument())
            currentDocument()->reorder(-1);
    }));
    layer->addSeparator();
    for (const QString &name : {QString("Add white mask"), QString("Mask from selection"),
                                QString("Invert mask"), QString("Remove mask")})
        layer->addAction(action(name, {}, [this, name]() { maskAction(name); }));
    layer->addSeparator();
    layer->addAction(action("Rasterize text / shape", {}, [this]() {
        auto *d = currentDocument();
        if (!d || !d->active() || !QStringList{"text", "shape"}.contains(d->active()->kind))
            return;
        d->edit("Rasterize layer", [&]() {
            auto *l = d->active();
            Layer unmasked = *l;
            unmasked.mask = {};
            l->image = unmasked.pixels();
            l->kind = "raster";
        });
    }));
    layer->addAction(action("Flip horizontally", {}, [this]() {
        auto *d = currentDocument();
        if (d && d->active() && !d->active()->isGroup())
            d->edit("Flip layer", [&]() { d->active()->flipX = !d->active()->flipX; });
    }));
    layer->addAction(action("Flip vertically", {}, [this]() {
        auto *d = currentDocument();
        if (d && d->active() && !d->active()->isGroup())
            d->edit("Flip layer", [&]() { d->active()->flipY = !d->active()->flipY; });
    }));
    layer->addAction(action("Merge selected layers", QKeySequence("Ctrl+E"),
                            [this] { run([](Document *d, QString *e) { return d->mergeSelected(e); }); }));
    layer->addAction(action("Toggle clipping mask", QKeySequence("Ctrl+Alt+G"), [this] {
        auto *d = currentDocument();
        if (d && d->active() && !d->active()->isGroup() && d->active()->kind != "adjustment")
            d->edit("Clipping mask", [&] { d->active()->clipped = !d->active()->clipped; });
    }));
    layer->addAction(action("Edit adjustment layer…", {}, [this] {
        auto *d = currentDocument();
        if (d && d->active() && d->active()->kind == "adjustment")
            adjustDialog(d->active()->adjustment);
    }));
    auto *select = menuBar()->addMenu("&Select");
    select->addAction(action("All", QKeySequence::SelectAll, [this]() {
        if (currentDocument()) {
            currentDocument()->state.selection = {};
            currentDocument()->state.selection.addRect(QRectF(QPointF(), currentDocument()->state.size));
            currentDocument()->notify();
        }
    }));
    select->addAction(action("Deselect", QKeySequence("Ctrl+D"), [this]() {
        if (currentDocument()) {
            currentDocument()->state.selection = {};
            currentDocument()->notify();
        }
    }));
    select->addAction(action("Invert selection", QKeySequence("Ctrl+Shift+I"), [this]() {
        if (currentDocument()) {
            QPainterPath all;
            all.addRect(QRectF(QPointF(), currentDocument()->state.size));
            currentDocument()->state.selection = all.subtracted(currentDocument()->state.selection);
            currentDocument()->notify();
        }
    }));
    auto *image = menuBar()->addMenu("&Image");
    image->addAction(action("Canvas size…", {}, [this]() {
        auto *d = currentDocument();
        if (!d)
            return;
        QDialog dialog(this);
        dialog.setWindowTitle("Canvas size");
        QFormLayout form(&dialog);
        QSpinBox w, h;
        w.setRange(1, MaxSide);
        h.setRange(1, MaxSide);
        w.setValue(d->state.size.width());
        h.setValue(d->state.size.height());
        form.addRow("Width", &w);
        form.addRow("Height", &h);
        QLabel note("Layers keep their coordinates; excess pixels remain available.");
        note.setWordWrap(true);
        form.addRow(&note);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        form.addRow(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        translateWidgets(&dialog);
        if (dialog.exec() == QDialog::Accepted) {
            QSize size(w.value(), h.value());
            if (!Document::validSize(size)) {
                error("Canvas exceeds the 16 million pixel limit.");
                return;
            }
            d->edit("Canvas size", [&]() {
                d->state.size = size;
                d->state.selection = {};
            });
        }
    }));
    image->addAction(action("Crop to selection", {}, [this]() {
        if (currentDocument() && !currentDocument()->state.selection.isEmpty())
            currentDocument()->crop(currentDocument()->state.selection.boundingRect().toAlignedRect());
    }));
    auto *adjustments = image->addMenu("Adjustments");
    for (const QString &kind :
         {QString("Levels"), QString("Curves"), QString("Hue / Saturation"), QString("Invert")})
        adjustments->addAction(action(kind, {}, [this, kind]() { adjustDialog(kind); }));
    auto *view = menuBar()->addMenu("&View");
    view->addAction(action("Fit canvas", QKeySequence("Ctrl+0"), [this]() {
        if (currentCanvas())
            currentCanvas()->fit();
    }));
    view->addAction(action("Actual pixels", QKeySequence("Ctrl+1"), [this]() {
        if (currentCanvas())
            currentCanvas()->actualSize();
    }));
    view->addAction(action("Zoom in", QKeySequence::ZoomIn, [this]() {
        if (currentCanvas())
            currentCanvas()->zoomBy(1.25);
    }));
    view->addAction(action("Zoom out", QKeySequence::ZoomOut, [this]() {
        if (currentCanvas())
            currentCanvas()->zoomBy(.8);
    }));
    view->addSeparator();
    for (auto *dock : findChildren<QDockWidget *>())
        view->addAction(dock->toggleViewAction());
    auto *languages = menuBar()->addMenu("Language / 语言");
    languages->setObjectName("languageMenu");
    auto *languageGroup = new QActionGroup(this);
    languageGroup->setExclusive(true);
    for (const auto &code : languageCodes()) {
        auto *choice = languages->addAction(languageName(code));
        choice->setProperty("languageCode", code);
        choice->setCheckable(true);
        choice->setChecked(code == currentLanguage());
        languageGroup->addAction(choice);
        connect(choice, &QAction::triggered, this, [code] { setLanguage(code); });
    }
    auto *help = menuBar()->addMenu("&Help");
    help->addAction(action("Quick guide", QKeySequence("F1"), [this]() {
        QMessageBox::information(
            this, "Quick guide",
            ui("B / E: brush / eraser. [ / ]: brush size. V: move or scale.\n"
               "M / Shift+M / L / W: selections; Shift adds, Alt subtracts. C: crop.\n"
               "U / Shift+U: shapes. T: text. S / J: clone / heal; Alt-click sets the source.\n"
               "Wheel: zoom. Space / middle button: pan. Esc: cancel.\n"
               "Layer menu: masks, clipping, merge. Image menu: adjustment layers.\n"
               "Ctrl+S: save .psproj. Ctrl+Shift+E: export PNG / JPEG.\n"
               "PSD import: 8-bit RGB with a compatibility report. RAW and AI selection are unavailable.\n"
               "Limits: 16 million canvas pixels, 32 million source pixels, 256 layers."));
    }));
    help->addAction(action("Open demo document", {}, [this]() { demo(); }));
    help->addAction(action("About", {}, [this]() {
        QMessageBox::about(this, "PhotoShip 0.2.1",
                           "A cross-platform, layer-based image editor built with Qt 6 and C++.\nCompositor "
                           "algorithms used under the MIT license.\nQt is dynamically linked; see "
                           "THIRD_PARTY_NOTICES.md.\nThis application is independent of Adobe Photoshop.");
    }));
}
void Window::refresh() {
    if (syncing)
        return;
    syncing = true;
    for (int i = 0; i < tabs->count(); ++i) {
        auto *c = qobject_cast<Canvas *>(tabs->widget(i));
        auto *d = c->document;
        QString title = d->filePath.isEmpty() ? ui("Untitled") : QFileInfo(d->filePath).completeBaseName();
        tabs->setTabText(i, title + (d->dirty() ? " *" : "") + (d->saving ? ui(" [saving]") : QString()));
    }
    auto *d = currentDocument();
    layers->clear();
    parentGroup->clear();
    parentGroup->addItem(ui("None"), QString());
    QMap<QString, QTreeWidgetItem *> items;
    if (d) {
        for (const auto &l : d->state.layers) {
            auto *item = new QTreeWidgetItem;
            item->setData(0, Qt::UserRole, l.id);
            item->setData(0, Qt::UserRole + 1, l.isGroup());
            item->setText(0, l.name);
            item->setToolTip(0, ui(l.kind) + (l.clipped ? ui(" • clipped to base alpha") : QString()));
            item->setFlags(item->flags() | Qt::ItemIsEditable | Qt::ItemIsUserCheckable);
            item->setCheckState(0, l.visible ? Qt::Checked : Qt::Unchecked);
            if (!l.parent.isEmpty() && items.contains(l.parent))
                items[l.parent]->insertChild(0, item);
            else
                layers->insertTopLevelItem(0, item);
            items[l.id] = item;
            if (l.isGroup())
                item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            else if (l.kind == "adjustment")
                item->setIcon(0, style()->standardIcon(QStyle::SP_FileDialogDetailedView));
            else {
                QImage thumb = l.pixels().scaled(38, 30, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                item->setIcon(0, QIcon(QPixmap::fromImage(thumb)));
            }
            item->setExpanded(true);
        }
        if (items.contains(d->state.active))
            layers->setCurrentItem(items[d->state.active], 0, QItemSelectionModel::NoUpdate);
        auto selected = d->selectedRoots();
        for (auto it = items.begin(); it != items.end(); ++it)
            it.value()->setSelected(selected.contains(it.key()));
    }
    Layer *l = d ? d->active() : nullptr;
    bool enabled = l != nullptr;
    for (QWidget *w : {static_cast<QWidget *>(opacity), static_cast<QWidget *>(x), static_cast<QWidget *>(y),
                       static_cast<QWidget *>(layerWidth), static_cast<QWidget *>(layerHeight),
                       static_cast<QWidget *>(rotation), static_cast<QWidget *>(parentGroup)})
        w->setEnabled(enabled);
    blend->setEnabled(enabled && !l->isGroup() && l->kind != "adjustment");
    maskTarget->setEnabled(enabled && !l->isGroup());
    maskEnabled->setEnabled(enabled && !l->mask.isNull());
    if (l) {
        opacity->setValue(l->opacity * 100);
        blend->setCurrentIndex(blend->findData(l->blend));
        x->setValue(l->position.x());
        y->setValue(l->position.y());
        layerWidth->setValue(l->size.width());
        layerHeight->setValue(l->size.height());
        rotation->setValue(l->rotation);
        layerWidth->setEnabled(!l->isGroup());
        layerHeight->setEnabled(!l->isGroup());
        rotation->setEnabled(!l->isGroup());
        maskEnabled->setChecked(!l->mask.isNull() && l->maskEnabled);
        for (const auto &g : d->state.layers)
            if (g.isGroup() && g.id != l->id && !d->descendant(g, l->id))
                parentGroup->addItem(g.name, g.id);
        parentGroup->setCurrentIndex(qMax(0, parentGroup->findData(l->parent)));
    }
    maskTarget->setChecked(currentCanvas() && currentCanvas()->maskTarget);
    if (d) {
        details->setText(ui("%1 × %2 px  •  8-bit sRGB\n%3 layers  •  %4 MP source pixels\n%5\n\nSelect "
                            "a layer to edit its properties.")
                             .arg(d->state.size.width())
                             .arg(d->state.size.height())
                             .arg(d->state.layers.size())
                             .arg(d->pixelCount() / 1000000.0, 0, 'f', 1)
                             .arg(ui(l ? l->kind : "No active layer")));
        setWindowTitle(tabs->tabText(tabs->currentIndex()) + " — PhotoShip");
    } else {
        details->setText(ui("Create or open a document to begin."));
        setWindowTitle("PhotoShip");
    }
    undoAction->setEnabled(d && !d->past.isEmpty());
    redoAction->setEnabled(d && !d->future.isEmpty());
    undoAction->setText(d && !d->past.isEmpty() ? ui("Undo %1").arg(ui(d->past.last().label)) : ui("Undo"));
    redoAction->setText(d && !d->future.isEmpty() ? ui("Redo %1").arg(ui(d->future.last().label))
                                                  : ui("Redo"));
    syncing = false;
}
void Window::transformChanged() {
    if (syncing)
        return;
    auto *d = currentDocument();
    if (!d || !d->active())
        return;
    auto *l = d->active();
    QPointF position(x->value(), y->value());
    QSizeF size(layerWidth->value(), layerHeight->value());
    double angle = rotation->value();
    if (l->position == position && l->size == size && l->rotation == angle)
        return;
    if (l->kind == "text" || l->kind == "shape") {
        qint64 old = qint64(qRound(l->size.width())) * qRound(l->size.height());
        qint64 pixels = qint64(qRound(size.width())) * qRound(size.height());
        if (!Document::validSize(size.toSize(), MaxSourcePixels) ||
            d->pixelCount() - old + pixels > MaxSourcePixels) {
            error("Layer size exceeds the source pixel budget.");
            refresh();
            return;
        }
    }
    d->edit("Transform layer", [&]() {
        l = d->active();
        QPointF delta = position - l->position;
        if (l->isGroup()) {
            auto ids = d->descendants(l->id);
            for (auto &member : d->state.layers)
                if (ids.contains(member.id))
                    member.position += delta;
        } else {
            l->position = position;
            if (l->size != size && !l->mask.isNull() && l->image.isNull())
                l->mask = l->mask.scaled(size.toSize());
            l->size = size;
            l->rotation = angle;
        }
    });
}
void Window::newDialog() {
    QDialog dialog(this);
    dialog.setWindowTitle("New document");
    QFormLayout layout(&dialog);
    QSpinBox w, h;
    w.setRange(1, MaxSide);
    h.setRange(1, MaxSide);
    w.setValue(1280);
    h.setValue(800);
    layout.addRow("Width (px)", &w);
    layout.addRow("Height (px)", &h);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    translateWidgets(&dialog);
    if (dialog.exec() == QDialog::Accepted)
        newDocument(QSize(w.value(), h.value()));
}
bool Window::openPath(const QString &path) {
    QString errorMessage;
    QFileInfo info(path);
    if (info.isDir() || info.fileName() == "manifest.json") {
        State state;
        if (!loadProject(path, state, errorMessage)) {
            error(errorMessage);
            return false;
        }
        Document d;
        d.state = state;
        QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
        QFile f(QDir(folder).filePath("manifest.json"));
        f.open(QIODevice::ReadOnly);
        if (QJsonDocument::fromJson(f.readAll()).object()["format"].toString() == "org.pixelstudio.project")
            d.filePath = folder;
        addDocument(std::move(d));
        if (currentDocument()->filePath.isEmpty()) {
            currentDocument()->revision = 1;
            currentDocument()->notify();
            statusBar()->showMessage(ui("Compositor project imported. Save as a new .psproj project."), 8000);
        }
        return true;
    }
    if (info.suffix().compare("psd", Qt::CaseInsensitive) == 0) {
        State state;
        QStringList report;
        if (!readPsd(path, state, report, errorMessage)) {
            error(errorMessage);
            return false;
        }
        QDialog dialog(this);
        dialog.setWindowTitle("PSD compatibility report");
        dialog.resize(640, 460);
        QVBoxLayout layout(&dialog);
        QLabel summary(ui(report.value(0)));
        summary.setWordWrap(true);
        layout.addWidget(&summary);
        QTextEdit details;
        details.setReadOnly(true);
        details.setPlainText(
            report.mid(1).isEmpty()
                ? ui("All layer features recognized by this importer. Save the imported document as .psproj.")
                : [&report] {
                      QStringList translated;
                      for (const auto &line : report.mid(1))
                          translated << ui(line);
                      return translated.join("\n\n");
                  }());
        layout.addWidget(&details);
        QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons.button(QDialogButtonBox::Ok)->setText("Import");
        layout.addWidget(&buttons);
        connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        translateWidgets(&dialog);
        if (dialog.exec() != QDialog::Accepted)
            return false;
        Document d;
        d.state = std::move(state);
        d.revision = 1;
        addDocument(std::move(d));
        return true;
    }
    QImage image;
    if (!readImage(path, image, errorMessage)) {
        error(errorMessage);
        return false;
    }
    if (!Document::validSize(image.size())) {
        error("Image is too large for a new canvas. Create a smaller canvas and import it as a layer.");
        return false;
    }
    Document d = Document::create(image.size());
    d.state.layers[0].image = image;
    d.state.layers[0].name = info.completeBaseName();
    d.revision = 1;
    addDocument(std::move(d));
    return true;
}
void Window::importImages(const QStringList &provided) {
    QStringList paths = provided;
    if (paths.isEmpty())
        paths = QFileDialog::getOpenFileNames(
            this, ui("Import images"), {},
            ui("Images / PSD (*.png *.jpg *.jpeg *.bmp *.webp *.psd);;All files (*)"), nullptr,
            QFileDialog::DontUseNativeDialog);
    for (const auto &path : paths) {
        if (QFileInfo(path).isDir() || QFileInfo(path).fileName() == "manifest.json" ||
            QFileInfo(path).suffix().compare("psd", Qt::CaseInsensitive) == 0) {
            openPath(path);
            continue;
        }
        if (!currentDocument()) {
            openPath(path);
            continue;
        }
        QImage image;
        QString message;
        if (!readImage(path, image, message)) {
            error(message);
            continue;
        }
        run([&](Document *d, QString *e) {
            return d->addImage(image, QFileInfo(path).completeBaseName(), e);
        });
    }
}
bool Window::save(bool saveAs) {
    auto *d = currentDocument();
    if (!d)
        return false;
    QString path = d->filePath;
    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this, ui("Save project directory"), path.isEmpty() ? "Untitled.psproj" : path,
            ui("PhotoShip project (*.psproj)"), nullptr, QFileDialog::DontUseNativeDialog);
        if (path.isEmpty())
            return false;
        if (!path.endsWith(".psproj", Qt::CaseInsensitive))
            path += ".psproj";
    }
    if (d->saving) {
        statusBar()->showMessage(ui("This document is already saving. Editing can continue."), 5000);
        return false;
    }
    const State snapshot = d->state;
    const quint64 revision = d->revision;
    const QString recovery = d->recoveryPath;
    d->saving = true;
    statusBar()->showMessage(ui("Saving project in background…"));
    refresh();
    persistence->submit(
        [path, snapshot] {
            QString error;
            bool ok = saveProject(path, snapshot, error);
            return JobResult{ok, error};
        },
        [this, path, revision, recovery](JobResult result) {
            Document *d = nullptr;
            for (const auto &entry : documents)
                if (entry->recoveryPath == recovery)
                    d = entry.get();
            if (!d)
                return;
            d->saving = false;
            if (!result.ok) {
                closeAfterSave.remove(d);
                closeWindowAfterSave = false;
                error(result.error);
                refresh();
                return;
            }
            d->filePath = QFileInfo(path).absoluteFilePath();
            d->lastChange = {};
            d->markSaved(revision);
            if (!d->dirty())
                clearRecovery(d);
            statusBar()->showMessage(
                ui(d->dirty() ? "Snapshot saved; newer edits still need saving." : "Project saved"), 6000);
            if (closeAfterSave.remove(d) && !d->dirty())
                for (int i = 0; i < tabs->count(); ++i)
                    if (qobject_cast<Canvas *>(tabs->widget(i))->document == d) {
                        closeTab(i);
                        break;
                    }
            if (closeWindowAfterSave) {
                closeWindowAfterSave = false;
                close();
            }
        });
    return true;
}

void Window::exportDialog() {
    auto *d = currentDocument();
    if (!d)
        return;
    QString selected;
    QString path = QFileDialog::getSaveFileName(this, ui("Export flattened image"), "Untitled.png",
                                                ui("PNG image (*.png);;JPEG image (*.jpg)"), &selected,
                                                QFileDialog::DontUseNativeDialog);
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += selected.contains("*.jpg") ? ".jpg" : ".png";
    int quality = 95;
    if (QStringList{"jpg", "jpeg"}.contains(QFileInfo(path).suffix().toLower())) {
        bool ok = false;
        quality = QInputDialog::getInt(this, "JPEG quality",
                                       "Quality (transparency is composited onto white)", 95, 1, 100, 1, &ok);
        if (!ok)
            return;
    }
    State snapshot = d->state;
    statusBar()->showMessage(ui("Exporting image in background…"));
    persistence->submit(
        [path, quality, snapshot] {
            Document copy;
            copy.state = snapshot;
            QString error;
            bool ok = exportImage(path, copy.composite(), quality, error);
            return JobResult{ok, error};
        },
        [this](JobResult result) {
            if (!result.ok)
                error(result.error);
            else
                statusBar()->showMessage(ui("Image exported. Project save status is unchanged."), 6000);
        });
}

bool Window::confirmClose(Document *d) {
    if (d->saving) {
        statusBar()->showMessage(ui("Saving is still running; this document will close after saving."), 5000);
        closeAfterSave.insert(d);
        return false;
    }
    if (!d->dirty())
        return true;
    auto choice = QMessageBox::question(this, "Unsaved changes", "Save changes before closing this document?",
                                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
                                        QMessageBox::Save);
    if (choice == QMessageBox::Cancel)
        return false;
    if (choice == QMessageBox::Save) {
        if (save())
            closeAfterSave.insert(d);
        return false;
    }
    clearRecovery(d);
    return true;
}
void Window::closeTab(int i) {
    if (i < 0 || i >= tabs->count())
        return;
    tabs->setCurrentIndex(i);
    auto *c = qobject_cast<Canvas *>(tabs->widget(i));
    c->abortGesture();
    auto *d = c->document;
    if (!confirmClose(d))
        return;
    clearRecovery(d);
    d->changed = {};
    d->regionChanged = {};
    closeAfterSave.remove(d);
    tabs->removeTab(i);
    delete c;
    documents.erase(
        std::remove_if(documents.begin(), documents.end(), [&](const auto &p) { return p.get() == d; }),
        documents.end());
    refresh();
}
void Window::closeEvent(QCloseEvent *e) {
    for (int i = 0; i < tabs->count(); ++i) {
        tabs->setCurrentIndex(i);
        currentCanvas()->abortGesture();
        if (!confirmClose(currentDocument())) {
            closeWindowAfterSave = currentDocument()->saving;
            e->ignore();
            return;
        }
    }
    for (const auto &d : documents)
        clearRecovery(d.get());
    persistence->wait();
    e->accept();
}
void Window::editText(QPointF at) {
    auto *d = currentDocument();
    if (!d)
        return;
    const bool editing = at.x() < 0 && d->active() && d->active()->kind == "text";
    if (!editing && at.x() < 0)
        return;
    QDialog dialog(this);
    dialog.setWindowTitle(editing ? "Edit text" : "Add text");
    dialog.resize(440, 330);
    QVBoxLayout layout(&dialog);
    QTextEdit text;
    text.setAcceptRichText(false);
    text.setPlainText(editing ? d->active()->text : ui("Your text"));
    layout.addWidget(&text);
    QFontComboBox family;
    family.setCurrentFont(editing ? d->active()->font : QFont("Sans Serif"));
    QSpinBox size;
    size.setRange(1, 1000);
    size.setValue(editing ? qRound(d->active()->font.pointSizeF()) : 36);
    QCheckBox bold("Bold");
    bold.setChecked(editing && d->active()->font.bold());
    QFormLayout form;
    form.addRow("Font", &family);
    form.addRow("Size (pt)", &size);
    form.addRow(&bold);
    layout.addLayout(&form);
    QColor color = editing ? d->active()->color : foreground;
    QPushButton colorEdit("Text color…");
    layout.addWidget(&colorEdit);
    connect(&colorEdit, &QPushButton::clicked, &dialog, [&]() {
        QColor picked =
            QColorDialog::getColor(color, &dialog, ui("Text color"), QColorDialog::DontUseNativeDialog);
        if (picked.isValid())
            color = picked;
    });
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    translateWidgets(&dialog);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (text.toPlainText().size() > 100000) {
        error("Text must contain at most 100,000 characters.");
        return;
    }
    QFont font = family.currentFont();
    font.setPointSize(size.value());
    font.setBold(bold.isChecked());
    if (editing)
        d->edit("Edit text", [&]() {
            auto *l = d->active();
            l->text = text.toPlainText();
            l->font = font;
            l->color = color;
            l->name = l->text.left(30);
        });
    else
        run([&](Document *doc, QString *error) {
            QSizeF box(qMin(600.0, qMax(1.0, doc->state.size.width() - at.x())),
                       qMin(240.0, qMax(1.0, doc->state.size.height() - at.y())));
            return doc->addText(text.toPlainText(), font, color, QRectF(at, box), error);
        });
}
void Window::maskAction(const QString &name) {
    auto *d = currentDocument();
    if (!d || !d->active() || d->active()->isGroup()) {
        statusBar()->showMessage(ui("Select a non-group layer for a mask."), 5000);
        return;
    }
    auto *l = d->active();
    if (name == "Invert mask" && l->mask.isNull())
        return;
    if (name == "Remove mask" && l->mask.isNull())
        return;
    d->edit(name, [&]() {
        l = d->active();
        if (name == "Remove mask") {
            l->mask = {};
            currentCanvas()->maskTarget = false;
            return;
        }
        QSize size = l->image.isNull() ? l->size.toSize() : l->image.size();
        if (name == "Invert mask") {
            QImage gray = l->mask.convertToFormat(QImage::Format_Grayscale8);
            gray.invertPixels();
            l->mask = gray.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        } else {
            l->mask = QImage(size, QImage::Format_ARGB32_Premultiplied);
            l->mask.fill(name == "Mask from selection" && !d->state.selection.isEmpty() ? Qt::black
                                                                                        : Qt::white);
            if (name == "Mask from selection" && !d->state.selection.isEmpty()) {
                QPainter p(&l->mask);
                p.setRenderHint(QPainter::Antialiasing);
                p.setTransform(l->transform().inverted());
                p.setClipPath(d->state.selection);
                p.fillRect(QRect(QPoint(), d->state.size), Qt::white);
            }
        }
        l->maskEnabled = true;
        currentCanvas()->maskTarget = true;
    });
}
void Window::adjustDialog(const QString &kind) {
    auto *d = currentDocument();
    if (!d || !d->active()) {
        statusBar()->showMessage(
            ui("Select a raster layer. Rasterize text or shapes before pixel adjustments."), 6000);
        return;
    }
    bool editing = d->active()->kind == "adjustment";
    if (kind == "Invert") {
        if (!editing)
            run([&](Document *d, QString *e) { return d->addAdjustment(kind, 0, 0, 0, {}, e); });
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(kind);
    QFormLayout layout(&dialog);
    QDoubleSpinBox first, second, third;
    QTextEdit curve;
    curve.setMaximumHeight(100);
    curve.setPlainText("0,0\n128,128\n255,255");
    if (kind == "Levels") {
        first.setRange(0, 254);
        first.setValue(0);
        second.setRange(.1, 10);
        second.setValue(1);
        third.setRange(1, 255);
        third.setValue(255);
        layout.addRow("Black point", &first);
        layout.addRow("Gamma", &second);
        layout.addRow("White point", &third);
    } else if (kind == "Hue / Saturation") {
        first.setRange(-180, 180);
        second.setRange(-100, 100);
        third.setRange(-100, 100);
        layout.addRow("Hue", &first);
        layout.addRow("Saturation (%)", &second);
        layout.addRow("Brightness (%)", &third);
    } else {
        layout.addRow("RGB curve points: input,output (0–255)", &curve);
        QLabel note("Linear interpolation between points. Input values must increase.");
        note.setWordWrap(true);
        layout.addRow(&note);
    }
    if (editing) {
        first.setValue(d->active()->first);
        second.setValue(d->active()->second);
        third.setValue(d->active()->third);
        QStringList points;
        for (auto p : d->active()->curve)
            points << QString("%1,%2").arg(p.x() * 255).arg(p.y() * 255);
        if (!points.isEmpty())
            curve.setPlainText(points.join("\n"));
    }
    QCheckBox destructive("Apply directly to raster pixels");
    destructive.setEnabled(!editing && d->active()->kind == "raster");
    layout.addRow(&destructive);
    QLabel note(
        editing
            ? "Edit this adjustment layer's parameters."
            : "Creates an editable adjustment layer above the active layer. A selection becomes its mask.");
    note.setWordWrap(true);
    layout.addRow(&note);
    QDialogButtonBox buttons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addRow(&buttons);
    connect(&buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    translateWidgets(&dialog);
    if (dialog.exec() != QDialog::Accepted)
        return;
    if (kind == "Levels" && first.value() >= third.value()) {
        error("Black point must be lower than white point.");
        return;
    }
    QVector<QPointF> points;
    if (kind == "Curves") {
        for (const QString &line : curve.toPlainText().split('\n', Qt::SkipEmptyParts)) {
            auto parts = line.split(',');
            bool okX = false, okY = false;
            double x = parts.value(0).trimmed().toDouble(&okX), y = parts.value(1).trimmed().toDouble(&okY);
            if (parts.size() != 2 || !okX || !okY || !std::isfinite(x) || !std::isfinite(y) || x < 0 ||
                x > 255 || y < 0 || y > 255 || (!points.isEmpty() && x / 255 <= points.last().x())) {
                error("Each point needs two values from 0 to 255, with increasing input values.");
                return;
            }
            points << QPointF(x / 255, y / 255);
        }
        if (points.size() < 2 || points.size() > 256) {
            error("Enter 2–256 curve points.");
            return;
        }
    }
    if (editing)
        d->edit("Edit adjustment", [&] {
            auto *l = d->active();
            l->first = first.value();
            l->second = second.value();
            l->third = third.value();
            l->curve = points;
        });
    else if (destructive.isChecked())
        d->adjust(kind, first.value(), second.value(), third.value(), points);
    else
        run([&](Document *d, QString *e) {
            return d->addAdjustment(kind, first.value(), second.value(), third.value(), points, e);
        });
}
void Window::demo() {
    Document d = Document::create(QSize(1200, 800));
    d.state.layers[0].image.fill(QColor("#151f32"));
    d.state.layers[0].name = "Midnight background";
    d.addShape("ellipse", QColor("#526cff"), QRectF(560, 80, 550, 550));
    d.active()->name = "Blue orbit";
    d.addShape("ellipse", QColor("#f09d85"), QRectF(750, 385, 170, 170));
    d.active()->name = "Warm accent";
    d.addShape("rectangle", QColor("#2a3c58"), QRectF(85, 585, 1010, 110));
    d.active()->name = "Footer panel";
    QFont title("Sans Serif", 62);
    title.setBold(true);
    d.addText("Make room\nfor your ideas.", title, QColor("#f3f5fc"), QRectF(90, 140, 730, 290));
    d.active()->name = "Headline";
    QFont small("Sans Serif", 17);
    d.addText("LAYERS  /  COLOR  /  POSSIBILITY", small, QColor("#b5c8e8"), QRectF(92, 505, 680, 65));
    d.active()->name = "Caption";
    d.addText("Built to create. Saved to keep.", small, QColor("#ffffff"), QRectF(110, 615, 870, 60));
    d.active()->name = "Footer text";
    d.past.clear();
    d.future.clear();
    addDocument(std::move(d));
    currentCanvas()->fit();
}
bool Window::writeScreenshot(const QString &path) {
    return grab().save(path, "PNG");
}
} // namespace ps

namespace ps {
void Window::snapshotRecovery() {
    for (const auto &entry : documents) {
        auto *d = entry.get();
        QString path = d->recoveryPath;
        if (!d->dirty() || d->isEditing() || recovering.contains(path) ||
            (recoveredRevision.contains(path) && recoveredRevision[path] == d->revision))
            continue;
        State snapshot = d->state;
        QString original = d->filePath;
        quint64 revision = d->revision;
        recovering.insert(path);
        persistence->submit(
            [path, snapshot, original, revision] {
                QString error;
                bool ok = saveRecovery(path, snapshot, original, revision, error);
                return JobResult{ok, error};
            },
            [this, path, revision](JobResult result) {
                recovering.remove(path);
                bool open = false;
                for (const auto &d : documents)
                    if (d->recoveryPath == path)
                        open = true;
                if (!open)
                    return;
                if (result.ok)
                    recoveredRevision[path] = revision;
                else
                    statusBar()->showMessage(ui("Recovery save failed: " + result.error), 10000);
            });
    }
}
void Window::clearRecovery(Document *d) {
    QString path = d->recoveryPath;
    recoveredRevision.remove(path);
    persistence->submit([path] { return JobResult{removeRecovery(path), {}}; });
}
void Window::recoverDocuments() {
    QDir root(recoveryDirectory());
    auto paths = root.entryList({"*.psproj"}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);
    if (paths.isEmpty())
        return;
    QDialog dialog(this);
    dialog.setWindowTitle("Recover unsaved documents");
    dialog.resize(560, 360);
    QVBoxLayout layout(&dialog);
    QLabel note("Recovery snapshots were found. Restore opens separate unsaved documents and preserves the "
                "original projects.");
    note.setWordWrap(true);
    layout.addWidget(&note);
    QTreeWidget list;
    list.setHeaderLabels({"Recovery snapshot", "Original project"});
    layout.addWidget(&list);
    for (auto name : paths) {
        QString path = root.filePath(name);
        if (QFileInfo(path).isSymLink())
            continue;
        QFile metadata(QDir(path).filePath("recovery.json"));
        QJsonObject obj;
        if (metadata.open(QIODevice::ReadOnly) && metadata.size() < 16384)
            obj = QJsonDocument::fromJson(metadata.readAll()).object();
        auto *item = new QTreeWidgetItem(
            &list, {obj["savedAt"].toString(name), obj["originalPath"].toString("Untitled")});
        item->setData(0, Qt::UserRole, path);
        item->setCheckState(0, Qt::Checked);
    }
    auto *buttons = new QDialogButtonBox;
    auto *restore = buttons->addButton("Restore checked", QDialogButtonBox::AcceptRole);
    auto *discard = buttons->addButton("Discard checked", QDialogButtonBox::DestructiveRole);
    buttons->addButton("Keep for later", QDialogButtonBox::RejectRole);
    layout.addWidget(buttons);
    int choice = 0;
    connect(restore, &QPushButton::clicked, &dialog, [&] {
        choice = 1;
        dialog.accept();
    });
    connect(discard, &QPushButton::clicked, &dialog, [&] {
        choice = 2;
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    translateWidgets(&dialog);
    if (dialog.exec() != QDialog::Accepted)
        return;
    for (int i = 0; i < list.topLevelItemCount(); ++i) {
        auto *item = list.topLevelItem(i);
        if (item->checkState(0) != Qt::Checked)
            continue;
        QString path = item->data(0, Qt::UserRole).toString();
        if (choice == 2) {
            removeRecovery(path);
            continue;
        }
        State state;
        QString original, message;
        if (!loadRecovery(path, state, original, message)) {
            error(message);
            continue;
        }
        Document d;
        d.state = std::move(state);
        d.recoveryPath = path;
        d.revision = 1;
        addDocument(std::move(d));
        statusBar()->showMessage(ui("Recovered snapshot. Save As to preserve it."), 10000);
    }
}
} // namespace ps
