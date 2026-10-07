#include "document.h"
#include "persistence.h"
#include "psd.h"
#include "window.h"
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointingDevice>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QTabletEvent>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
using namespace ps;
static int checks = 0;
static void check(bool value, const char *message) {
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
static QImage image(QSize size, QColor color) {
    QImage out(size, QImage::Format_ARGB32_Premultiplied);
    out.fill(color);
    return out;
}
static void write(QString path, QByteArray data) {
    QFile file(path);
    check(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "fixture write");
}
static bool waitUntil(std::function<bool()> ready, int timeout = 10000) {
    QElapsedTimer timer;
    timer.start();
    while (!ready() && timer.elapsed() < timeout) {
        QApplication::processEvents();
        QThread::msleep(1);
    }
    return ready();
}
static void u16(QByteArray &b, int n) {
    b += char(n >> 8);
    b += char(n);
}
static void u32(QByteArray &b, qint64 n) {
    for (int i = 3; i >= 0; --i)
        b += char((quint64(n) >> (8 * i)) & 255);
}
static void section(QByteArray &b, QByteArray part) {
    u32(b, part.size());
    b += part;
}
struct FixtureLayer {
    QString name;
    QColor color = Qt::red;
    int folder = 0, compression = 0;
    bool mask = false, clipped = false, unsupported = false;
};
static QByteArray psd(QVector<FixtureLayer> layers, int depth = 8, int mode = 3) {
    std::reverse(layers.begin(), layers.end()); // Fixture API is top to bottom; PSD bytes are bottom to top.
    QByteArray b = "8BPS";
    u16(b, 1);
    b += QByteArray(6, 0);
    u16(b, 3);
    u32(b, 2);
    u32(b, 2);
    u16(b, depth);
    u16(b, mode);
    u32(b, 0);
    u32(b, 0);
    QByteArray info, records, payload;
    u16(info, layers.size());
    for (const auto &l : layers) {
        bool empty = l.folder != 0;
        u32(records, 0);
        u32(records, 0);
        u32(records, empty ? 0 : 2);
        u32(records, empty ? 0 : 2);
        u16(records, empty ? 0 : l.mask ? 5 : 4);
        if (!empty)
            for (int c : QList<int>{0, 1, 2, -1, -2}) {
                if (c == -2 && !l.mask)
                    continue;
                QByteArray plane(4, char(c == 0    ? l.color.red()
                                         : c == 1  ? l.color.green()
                                         : c == 2  ? l.color.blue()
                                         : c == -1 ? l.color.alpha()
                                                   : 128));
                QByteArray channel;
                u16(channel, l.compression);
                if (l.compression == 0)
                    channel += plane;
                else if (l.compression == 1) {
                    u16(channel, 3);
                    u16(channel, 3);
                    for (int row = 0; row < 2; ++row) {
                        channel += char(1);
                        channel += plane.mid(row * 2, 2);
                    }
                } else {
                    if (l.compression == 3) {
                        plane[1] = char(quint8(plane[1]) - quint8(plane[0]));
                        plane[3] = char(quint8(plane[3]) - quint8(plane[2]));
                    }
                    channel += qCompress(plane).mid(4);
                }
                u16(records, c);
                u32(records, channel.size());
                payload += channel;
            }
        records += "8BIMnorm";
        records += char(255);
        records += char(l.clipped);
        records += char(0);
        records += char(0);
        QByteArray extra, mask;
        if (l.mask) {
            u32(mask, 0);
            u32(mask, 0);
            u32(mask, 2);
            u32(mask, 2);
            mask += char(255);
            mask += char(0);
            mask += QByteArray(2, 0);
        }
        section(extra, mask);
        u32(extra, 0);
        QByteArray name = l.name.toLatin1();
        extra += char(name.size());
        extra += name;
        extra += QByteArray((4 - (name.size() + 1) % 4) % 4, 0);
        QByteArray unicode;
        u32(unicode, l.name.size());
        for (auto c : l.name)
            u16(unicode, c.unicode());
        extra += "8BIMluni";
        section(extra, unicode);
        if (l.folder) {
            QByteArray data;
            u32(data, l.folder);
            if (l.folder != 3)
                data += "8BIMpass";
            extra += "8BIMlsct";
            section(extra, data);
        }
        if (l.unsupported) {
            extra += "8BIMTySh";
            u32(extra, 0);
        }
        section(records, extra);
    }
    info += records;
    info += payload;
    if (info.size() % 2)
        info += char(0);
    QByteArray lm;
    section(lm, info);
    u32(lm, 0);
    section(b, lm);
    u16(b, 0);
    b += QByteArray(4, char(255));
    b += QByteArray(8, char(0));
    return b;
}
int main(int argc, char **argv) {
    qputenv("PHOTOSHIP_TESTING", "1");
    QApplication app(argc, argv);
    if (argc == 3 && QString::fromLocal8Bit(argv[1]) == "--crash-writer") {
        Document crash = Document::create({16, 16});
        crash.active()->image.fill(Qt::green);
        QString error;
        bool ok = saveRecovery(QString::fromLocal8Bit(argv[2]), crash.state, {}, 1, error);
        std::_Exit(ok ? 99 : 98);
    }
    QTemporaryDir temp;
    qputenv("PHOTOSHIP_RECOVERY_DIR", (temp.path() + "/recovery").toUtf8());
    try {
        QString error;
        Document d = Document::create({1024, 1024});
        d.past.clear();
        QImage original = d.active()->image;
        d.beginStroke("Small stroke", false);
        d.paint({30, 30}, {40, 40}, 8, Qt::red, 1, false, false);
        d.commit();
        check(d.past.last().stroke && d.historyBytes() <= 2 * 256 * 256 * 4,
              "small stroke history bounded to touched tile");
        QImage painted = d.active()->image;
        d.undo();
        check(d.active()->image == original, "delta undo exact");
        d.redo();
        check(d.active()->image == painted, "delta redo exact");
        d.edit("Rename", [&] { d.active()->name = "Renamed"; });
        d.undo();
        d.undo();
        check(d.active()->image == original, "mixed structural and delta undo");
        d.redo();
        d.redo();
        check(d.active()->name == "Renamed" && d.active()->image == painted, "mixed redo");
        d.beginStroke("Mask", true);
        d.paint({10, 10}, {10, 10}, 10, Qt::black, 1, false, true);
        d.cancel();
        check(d.active()->mask.isNull(), "cancel removes newly created mask");
        d.beginStroke("Mask", true);
        d.paint({10, 10}, {10, 10}, 10, Qt::black, 1, false, true);
        d.commit();
        QImage mask = d.active()->mask;
        d.undo();
        check(d.active()->mask.isNull(), "undo new mask");
        d.redo();
        check(d.active()->mask == mask, "redo mask exact");
        d.active()->rotation = 30;
        d.active()->position = {12, 9};
        d.active()->size = {512, 700};
        QImage beforeTransform = d.active()->image;
        d.beginStroke("Rotated", false);
        d.paint({50, 70}, {80, 100}, 32, Qt::green, 1, false, false);
        d.commit();
        d.undo();
        check(d.active()->image == beforeTransform, "rotated brush delta bounds cover changed pixels");
        // Viewport cache does not flatten the entire image and does not repaint all tiles after a dab.
        Document tiled = Document::create({2048, 2048});
        Canvas canvas(&tiled);
        canvas.resize(520, 520);
        canvas.actualSize();
        canvas.show();
        canvas.actualSize();
        QApplication::processEvents();
        int rendered = canvas.tilesRendered;
        check(rendered > 0 && rendered < 64, "only viewport tiles rendered");
        canvas.repaint();
        check(canvas.tilesRendered == rendered, "viewport cache reused");
        tiled.regionChanged = [&](QRect r) { canvas.invalidateRegion(r); };
        tiled.beginStroke("Dab", false);
        tiled.paint({1000, 1000}, {1000, 1000}, 4, Qt::red, 1, false, false);
        tiled.commit();
        canvas.repaint();
        check(canvas.tilesRendered - rendered <= 4, "local invalidation reuses unaffected tiles");
        Document uniform = Document::create({768, 768});
        QColor uniformColor("#162233");
        uniform.active()->image.fill(uniformColor);
        Canvas smooth(&uniform);
        smooth.resize(600, 600);
        smooth.tool = Tool::Hand;
        smooth.show();
        smooth.actualSize();
        smooth.zoomBy(.8);
        QApplication::processEvents();
        QImage screen = smooth.grab().toImage();
        bool seamless = true;
        for (int y = 5; y < screen.height() - 5; ++y)
            for (int x = 5; x < screen.width() - 5; ++x) {
                QColor pixel = screen.pixelColor(x, y);
                if (std::abs(pixel.red() - uniformColor.red()) > 1 ||
                    std::abs(pixel.green() - uniformColor.green()) > 1 ||
                    std::abs(pixel.blue() - uniformColor.blue()) > 1)
                    seamless = false;
            }
        check(seamless, "zoomed tile boundaries have no checkerboard seams");
        Document layers = Document::create({16, 16});
        QString base = layers.state.active;
        layers.addImage(image({16, 16}, Qt::red), "Red");
        QString red = layers.state.active;
        layers.addImage(image({16, 16}, QColor(0, 0, 255, 128)), "Blue");
        QString blue = layers.state.active;
        layers.setSelected({red, blue}, blue);
        QImage mergedBefore = layers.composite();
        check(layers.mergeSelected(&error), "merge adjacent layers");
        check(layers.composite() == mergedBefore, "merge keeps composite");
        layers.undo();
        check(layers.state.layers.size() == 3, "merge undo");
        layers.addGroup();
        QString group = layers.state.active;
        check(layers.dropLayers({red, blue}, group, {}, &error), "drop selected into group");
        check(layers.state.layers[layers.index(red)].parent == group &&
                  layers.state.layers[layers.index(blue)].parent == group,
              "group assignment");
        check(!layers.dropLayers({group}, group, {}, &error), "reject cycle");
        layers.setSelected({group, red}, group);
        check(layers.selectedRoots() == QStringList{group} && layers.selectedMembers().size() == 3,
              "selected descendants deduplicated");
        check(layers.duplicate(&error), "duplicate selected group");
        check(layers.state.layers.size() == 7, "duplicate hierarchy count");
        QString copyGroup = layers.state.active;
        check(layers.descendants(copyGroup).size() == 3, "duplicate hierarchy preserved");
        layers.undo();
        layers.setSelected({red, blue}, blue);
        layers.remove();
        check(layers.index(red) < 0 && layers.index(blue) < 0, "delete multiple");
        layers.undo();
        check(layers.index(red) >= 0, "delete undo");
        layers.setSelected({base, red}, red);
        check(!layers.mergeSelected(&error), "reject non-sibling merge");
        Document pixels = Document::create({32, 32});
        pixels.active()->image.fill(Qt::transparent);
        pixels.active()->image.setPixelColor(4, 4, Qt::red);
        pixels.state.selection.addRect(3, 3, 3, 3);
        check(pixels.moveSelection({5, 0}, false, &error), "move selected pixels");
        check(qAlpha(pixels.active()->image.pixel(4, 4)) == 0 &&
                  pixels.active()->image.pixelColor(9, 4) == QColor(Qt::red),
              "pixel source cleared destination moved");
        pixels.undo();
        check(pixels.active()->image.pixelColor(4, 4) == QColor(Qt::red), "selection movement undo");
        check(pixels.moveSelection({5, 0}, true, &error), "copy selected pixels");
        check(pixels.active()->image.pixelColor(4, 4) == QColor(Qt::red) &&
                  pixels.active()->image.pixelColor(9, 4) == QColor(Qt::red),
              "copy retains source");
        pixels.undo();
        pixels.active()->position = {2, 2};
        pixels.active()->rotation = 90;
        pixels.state.selection = {};
        pixels.state.selection.addRect(pixels.active()->transform().mapRect(QRectF(3, 3, 3, 3)));
        check(pixels.moveSelection({0, 5}, false, &error), "move rotated selected pixels");
        check(pixels.active()->image.pixelColor(9, 4) == QColor(Qt::red),
              "document displacement mapped to rotated source");
        Document expandedPixels = Document::create({32, 32});
        expandedPixels.active()->image = image({4, 4}, Qt::red);
        expandedPixels.active()->size = {4, 4};
        expandedPixels.state.selection.addRect(0, 0, 4, 4);
        check(expandedPixels.moveSelection({8, 0}, false, &error),
              "selection moves beyond original layer source");
        check(expandedPixels.active()->image.width() == 12 &&
                  expandedPixels.composite().pixelColor(9, 1) == QColor(Qt::red),
              "expanded source retains moved pixels");
        expandedPixels.undo();
        check(expandedPixels.active()->image.size() == QSize(4, 4),
              "expanded source undo restores dimensions");
        Document adjust = Document::create({16, 16});
        adjust.active()->image.fill(QColor(60, 100, 200));
        QImage unchanged = adjust.active()->image;
        QString sourceID = adjust.state.active;
        check(adjust.addAdjustment("Invert", 0, 0, 0, {}, &error), "add non-destructive adjustment");
        check(adjust.composite().pixelColor(5, 5) == QColor(195, 155, 55), "invert composite");
        check(adjust.state.layers[adjust.index(sourceID)].image == unchanged,
              "adjustment retains source pixels");
        adjust.active()->visible = false;
        check(adjust.composite().pixelColor(5, 5) == QColor(60, 100, 200), "toggle adjustment visibility");
        adjust.active()->visible = true;
        adjust.active()->opacity = .5;
        QColor middle = adjust.composite().pixelColor(5, 5);
        check(std::abs(middle.red() - 128) <= 1, "adjustment opacity interpolation");
        adjust.active()->mask = image({16, 16}, Qt::black);
        check(adjust.composite().pixelColor(5, 5) == QColor(60, 100, 200), "adjustment black mask");
        adjust.active()->mask.fill(Qt::white);
        adjust.active()->first = 10;
        QString project = temp.path() + "/adjust.psproj";
        check(saveProject(project, adjust.state, error), "save v2 adjustment");
        State loaded;
        check(loadProject(project, loaded, error), "load v2 adjustment");
        check(loaded.layers.last().kind == "adjustment" && loaded.layers.last().first == 10,
              "parameters round trip");
        Document clipping = Document::create({16, 16});
        clipping.active()->image.fill(Qt::transparent);
        clipping.active()->image.setPixelColor(5, 5, Qt::red);
        clipping.addImage(image({16, 16}, Qt::blue), "Clipped");
        clipping.active()->clipped = true;
        QImage clipped = clipping.composite();
        check(clipped.pixelColor(5, 5) == QColor(Qt::blue) && qAlpha(clipped.pixel(4, 4)) == 0,
              "clipping uses base alpha");
        check(saveProject(temp.path() + "/clip.psproj", clipping.state, error) &&
                  loadProject(temp.path() + "/clip.psproj", loaded, error) && loaded.layers.last().clipped,
              "clipping round trip");
        Document groupBase = Document::create({16, 16});
        groupBase.addGroup();
        groupBase.addImage(image({16, 16}, Qt::red), "Group child");
        groupBase.state.active.clear();
        groupBase.addImage(image({16, 16}, Qt::blue), "Clip above group");
        groupBase.active()->clipped = true;
        check(groupBase.composite().pixelColor(5, 5) == QColor(Qt::red),
              "unsupported group clipping base does not latch onto older sibling");
        Document repair = Document::create({32, 32});
        repair.active()->image.fill(Qt::transparent);
        {
            QPainter p(&repair.active()->image);
            p.fillRect(0, 0, 16, 32, Qt::red);
            p.fillRect(16, 0, 16, 32, Qt::blue);
        }
        QImage repairBefore = repair.active()->image;
        repair.beginStroke("Clone", false);
        repair.repair({24, 16}, {24, 16}, {-16, 0}, 8, 1, false, repairBefore);
        repair.commit();
        check(repair.active()->image.pixelColor(24, 16) == QColor(Qt::red), "clone sampled source");
        repair.undo();
        check(repair.active()->image == repairBefore, "repair delta undo");
        repair.beginStroke("Heal", false);
        repair.repair({24, 16}, {24, 16}, {-16, 0}, 8, 1, true, repairBefore);
        repair.commit();
        check(repair.active()->image.pixelColor(24, 16) == QColor(Qt::blue),
              "heal adapts sampled tone to target");
        // Tablet pressure uses a synthetic Qt event; real hardware remains a manual verification.
        Document pen = Document::create({64, 64});
        pen.active()->image.fill(Qt::transparent);
        Canvas penCanvas(&pen);
        penCanvas.resize(300, 300);
        penCanvas.actualSize();
        penCanvas.tool = Tool::Brush;
        penCanvas.foreground = Qt::red;
        penCanvas.brushSize = 20;
        QPointF point = penCanvas.widgetPoint({30, 30});
        QPointingDevice device("Test pen", 1, QInputDevice::DeviceType::Stylus,
                               QPointingDevice::PointerType::Pen,
                               QInputDevice::Capability::Position | QInputDevice::Capability::Pressure, 1, 1);
        QTabletEvent press(QEvent::TabletPress, &device, point, point, .25, 0, 0, 0, 0, 0, Qt::NoModifier,
                           Qt::LeftButton, Qt::LeftButton);
        QApplication::sendEvent(&penCanvas, &press);
        QTabletEvent release(QEvent::TabletRelease, &device, point, point, 0, 0, 0, 0, 0, 0, Qt::NoModifier,
                             Qt::LeftButton, Qt::NoButton);
        QApplication::sendEvent(&penCanvas, &release);
        check(qAlpha(pen.active()->image.pixel(30, 30)) >= 60 &&
                  qAlpha(pen.active()->image.pixel(30, 30)) <= 65,
              "pressure opacity applied");
        check(qAlpha(pen.active()->image.pixel(34, 30)) == 0, "pressure size applied");
        check(pen.past.size() == 1, "tablet event creates single stroke history");
        QStringList report;
        State psdState;
        for (int compression = 0; compression <= 3; ++compression) {
            QString path = temp.path() + QString("/c%1.psd").arg(compression);
            write(path, psd({{"顶层", QColor(100, 150, 200, 128), 0, compression, true, false, true},
                             {"Bottom", Qt::white}},
                            8, 3));
            check(readPsd(path, psdState, report, error), "PSD channel compression import");
            check(psdState.layers.size() == 2 && psdState.layers.last().name == "顶层",
                  "PSD stacking and unicode names");
            check(psdState.layers.last().image.pixelColor(0, 0).red() >= 99 &&
                      psdState.layers.last().image.pixelColor(0, 0).alpha() == 128,
                  "PSD RGB/alpha");
            check(qGray(psdState.layers.last().mask.pixel(0, 0)) == 128, "PSD mask");
            check(report.join(' ').contains("TySh"), "unsupported PSD text appears in compatibility report");
        }
        write(
            temp.path() + "/group.psd",
            psd({{"Folder", Qt::white, 1}, {"Child", Qt::red}, {"End", Qt::white, 3}, {"Bottom", Qt::blue}}));
        check(readPsd(temp.path() + "/group.psd", psdState, report, error), "PSD group import");
        check(psdState.layers.size() == 3 && psdState.layers[1].isGroup() &&
                  psdState.layers[2].parent == psdState.layers[1].id,
              "PSD group hierarchy bottom to top");
        check(saveProject(temp.path() + "/psd.psproj", psdState, error),
              "imported PSD validates as editable project");
        QByteArray valid = psd({{"A", Qt::red}});
        for (int cut : QList<int>{0, 7, 25, 60, int(valid.size() / 2), int(valid.size() - 16)}) {
            write(temp.path() + "/bad.psd", valid.left(cut));
            check(!readPsd(temp.path() + "/bad.psd", psdState, report, error), "reject truncated PSD");
        }
        write(temp.path() + "/bad.psd", psd({{"A", Qt::red}}, 16, 3));
        check(!readPsd(temp.path() + "/bad.psd", psdState, report, error), "reject 16-bit PSD");
        write(temp.path() + "/bad.psd", psd({{"A", Qt::red}}, 8, 4));
        check(!readPsd(temp.path() + "/bad.psd", psdState, report, error), "reject CMYK PSD");
        write(temp.path() + "/flat.psd", psd({}));
        check(readPsd(temp.path() + "/flat.psd", psdState, report, error) &&
                  psdState.layers[0].image.pixelColor(0, 0) == QColor(Qt::red),
              "flattened PSD fallback");
        // A saved snapshot must never claim later edits were saved; UI event processing remains live.
        Persistence persistence;
        Document background = Document::create({512, 512});
        background.edit("Before save", [&] { background.active()->image.fill(Qt::red); });
        State snapshot = background.state;
        quint64 revision = background.revision;
        bool done = false;
        QString async = temp.path() + "/async.psproj";
        int ticks = 0;
        QTimer timer;
        timer.setInterval(0);
        QObject::connect(&timer, &QTimer::timeout, [&] { ++ticks; });
        timer.start();
        persistence.submit(
            [async, snapshot] {
                QString error;
                bool ok = saveProject(async, snapshot, error);
                return JobResult{ok, error};
            },
            [&](JobResult r) {
                check(r.ok, "background save succeeds");
                background.markSaved(revision);
                done = true;
            });
        background.edit("New edit", [&] { background.active()->image.fill(Qt::blue); });
        check(waitUntil([&] { return done; }), "async save completes");
        timer.stop();
        check(ticks > 0, "main event loop responds while saving");
        check(background.dirty(), "newer edits remain dirty");
        check(loadProject(async, loaded, error) && loaded.layers[0].image.pixelColor(0, 0) == QColor(Qt::red),
              "saved snapshot is consistent");
        QString recovery = QDir(recoveryDirectory())
                               .filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".psproj");
        check(saveRecovery(recovery, background.state, async, background.revision, error), "save recovery");
        QString originalPath;
        check(loadRecovery(recovery, loaded, originalPath, error) && originalPath == async &&
                  loaded.layers[0].image.pixelColor(0, 0) == QColor(Qt::blue),
              "recovery contains newer edits and original path");
        State savedOriginal;
        check(loadProject(async, savedOriginal, error) &&
                  savedOriginal.layers[0].image.pixelColor(0, 0) == QColor(Qt::red),
              "recovery preserves original project");
        check(!removeRecovery(async), "recovery deletion cannot erase original project");
        bool cleaned = false;
        persistence.submit([recovery, state = background.state] {
            QString error;
            return JobResult{saveRecovery(recovery, state, {}, 1, error), error};
        });
        persistence.submit(
            [recovery] {
                return JobResult{removeRecovery(recovery), {}};
            },
            [&](JobResult r) { cleaned = r.ok; });
        check(waitUntil([&] { return cleaned; }) && !QFileInfo::exists(recovery),
              "queued recovery cleanup wins over in-flight writes");
        Window window;
        window.newDocument({64, 64});
        window.currentDocument()->addBlank();
        auto *tree = window.findChild<QTreeWidget *>("layerTree");
        check(tree->selectionMode() == QAbstractItemView::ExtendedSelection,
              "UI layer multi-selection enabled");
        check(tree->topLevelItemCount() == 2, "UI refresh shows both layers");
        tree->topLevelItem(1)->setSelected(true);
        QApplication::processEvents();
        check(window.currentDocument()->selectedRoots().size() == 2, "UI multi-selection updates model");
        auto findAction = [&](QString text) {
            for (auto *action : window.findChildren<QAction *>())
                if (action->text() == text)
                    return action;
            return static_cast<QAction *>(nullptr);
        };
        auto *saveAction = findAction("&Save project");
        auto *closeAction = findAction("Close document");
        check(saveAction && closeAction, "save and close UI actions");
        auto *uiDocument = window.currentDocument();
        uiDocument->filePath = temp.path() + "/ui.psproj";
        saveAction->trigger();
        check(uiDocument->saving, "UI save starts asynchronously");
        uiDocument->edit("Edit during UI save", [&] { uiDocument->active()->image.fill(Qt::green); });
        closeAction->trigger();
        check(waitUntil([&] { return !uiDocument->saving; }), "UI background save completes");
        check(window.currentDocument() == uiDocument && uiDocument->dirty(),
              "close during save preserves newer edits");
        check(waitUntil([&] {
                  return QFileInfo::exists(QDir(uiDocument->recoveryPath).filePath("manifest.json"));
              }),
              "GUI autosave creates recovery snapshot");
        State uiRecovery;
        QString originalUI;
        check(loadRecovery(uiDocument->recoveryPath, uiRecovery, originalUI, error) &&
                  uiRecovery.layers.last().image.pixelColor(0, 0) == QColor(Qt::green),
              "GUI recovery contains latest edits");
        QString uiRecoveryPath = uiDocument->recoveryPath;
        int beforeClose = window.findChild<QTabWidget *>("documents")->count();
        saveAction->trigger();
        closeAction->trigger();
        check(waitUntil(
                  [&] { return window.findChild<QTabWidget *>("documents")->count() == beforeClose - 1; }),
              "clean saved document closes after async save");
        check(waitUntil([&] { return !QFileInfo::exists(uiRecoveryPath); }),
              "saved document recovery removed after pending jobs");
        QTimer::singleShot(0, &window, [&] {
            for (auto *dialog : window.findChildren<QDialog *>())
                if (dialog->windowTitle() == "PSD compatibility report")
                    dialog->accept();
        });
        check(window.openPath(temp.path() + "/c0.psd"), "PSD import UI compatibility confirmation");
        check(window.currentDocument()->state.layers.size() == 2 && window.currentDocument()->dirty() &&
                  window.currentDocument()->filePath.isEmpty(),
              "PSD UI opens as separate unsaved project");
        QString crashedPath = QDir(recoveryDirectory())
                                  .filePath(QUuid::createUuid().toString(QUuid::WithoutBraces) + ".psproj");
        QProcess crashWriter;
        crashWriter.start(QCoreApplication::applicationFilePath(), {"--crash-writer", crashedPath});
        check(crashWriter.waitForFinished(10000) && crashWriter.exitCode() == 99,
              "recovery writer exits abruptly after snapshot commit");
        check(QFileInfo::exists(QDir(crashedPath).filePath("manifest.json")),
              "committed recovery survives abrupt process exit");
        qunsetenv("PHOTOSHIP_TESTING");
        {
            Window restarted;
            QTimer::singleShot(0, &restarted, [&] {
                for (auto *button : restarted.findChildren<QPushButton *>())
                    if (button->text() == "Restore checked")
                        button->click();
            });
            QApplication::processEvents();
            check(restarted.currentDocument()->dirty() && restarted.currentDocument()->filePath.isEmpty() &&
                      restarted.currentDocument()->active()->image.pixelColor(0, 0) == QColor(Qt::green),
                  "startup recovery dialog restores crashed snapshot as unsaved document");
        }
        qputenv("PHOTOSHIP_TESTING", "1");
        std::cout << "Passed " << checks << " v2 checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
}
