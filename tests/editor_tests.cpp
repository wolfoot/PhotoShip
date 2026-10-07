#include "document.h"
#include "window.h"
#include <QApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <iostream>
#include <stdexcept>
static int checks = 0;
static void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
static QImage solid(QSize size, QColor color) {
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(color);
    return image;
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    try {
        using namespace ps;
        Document d = Document::create(QSize(64, 48));
        check(d.composite().pixelColor(5, 5) == QColor(Qt::white), "new canvas");
        QString error;
        check(d.addImage(solid(QSize(12, 12), Qt::red), "Red", &error), "add layer");
        QString red = d.state.active;
        d.active()->position = {10, 10};
        check(d.composite().pixelColor(12, 12) == QColor(Qt::red), "layer placement");
        d.markSaved();
        d.edit("Opacity", [&]() { d.active()->opacity = .5; });
        check(d.dirty(), "dirty edit");
        QColor mixed = d.composite().pixelColor(12, 12);
        check(mixed.red() == 255 && qAbs(mixed.green() - 128) <= 1, "opacity composition");
        d.undo();
        check(!d.dirty() && d.active()->opacity == 1, "undo to save");
        d.redo();
        check(d.dirty() && d.active()->opacity == .5, "redo");
        d.undo();
        check(d.addGroup(&error), "group creation");
        QString group = d.state.active;
        check(d.addImage(solid(QSize(4, 4), Qt::blue), "Blue", &error), "group child");
        QString blue = d.state.active;
        check(d.active()->parent == group, "group parenting");
        d.active()->position = {10, 10};
        d.state.active = group;
        d.active()->opacity = .5;
        check(qAbs(d.composite().pixelColor(11, 11).blue() - 128) <= 1, "group opacity");
        d.active()->visible = false;
        check(d.composite().pixelColor(11, 11).red() == 255, "group visibility");
        d.active()->visible = true;
        d.active()->opacity = 1;
        check(d.duplicate(&error), "duplicate group");
        QString copy = d.state.active;
        check(d.descendants(copy).size() == 2, "duplicate subtree");
        d.remove();
        check(d.index(copy) < 0 && d.index(blue) >= 0, "remove copied subtree");
        d.undo();
        check(d.index(copy) >= 0, "undo group deletion");
        d.redo();
        d.state.active = blue;
        d.reparent(QString());
        check(d.active()->parent.isEmpty(), "ungroup layer");
        d.undo();
        check(d.active()->parent == group, "undo reparent");
        d.state.active = red;
        d.active()->mask = solid(QSize(12, 12), Qt::black);
        check(d.composite().pixelColor(20, 20) == QColor(Qt::white), "black mask");
        d.active()->mask.fill(Qt::white);
        d.begin("Mask stroke");
        d.paint({20, 20}, {20, 20}, 4, Qt::black, 1, false, true);
        d.commit();
        check(d.composite().pixelColor(20, 20) == QColor(Qt::white), "mask painting");
        d.undo();
        check(d.composite().pixelColor(20, 20) == QColor(Qt::red), "mask undo");
        d.active()->mask = {};
        d.state.selection = {};
        d.state.selection.addRect(QRectF(10, 10, 3, 3));
        d.begin("Paint");
        d.paint({10, 10}, {20, 20}, 10, Qt::green, 1, false, false);
        d.commit();
        check(d.active()->image.pixelColor(1, 1).green() > 100, "selected painting");
        check(d.active()->image.pixelColor(8, 8) == QColor(Qt::red), "selection clipping");
        d.undo();
        d.state.selection = {};
        d.active()->rotation = 90;
        d.active()->size = QSizeF(24, 24);
        d.active()->position = {20, 10};
        QPointF paintPoint = d.active()->transform().map(QPointF(2, 2));
        d.begin("Transformed paint");
        d.paint(paintPoint, paintPoint, 5, Qt::green, 1, false, false);
        d.commit();
        check(d.active()->image.pixelColor(2, 2).green() > 100, "rotated scaled painting");
        d.undo();
        Document blend = Document::create(QSize(8, 8));
        blend.state.layers[0].image.fill(QColor(100, 150, 200));
        blend.addImage(solid(QSize(8, 8), QColor(128, 128, 128)), "Multiply");
        blend.active()->blend = "Multiply";
        QColor product = blend.composite().pixelColor(1, 1);
        check(qAbs(product.red() - 50) <= 1 && qAbs(product.blue() - 100) <= 1, "multiply blend");
        Document wand = Document::create(QSize(10, 10));
        wand.state.layers[0].image.fill(Qt::white);
        {
            QPainter p(&wand.state.layers[0].image);
            p.fillRect(QRect(2, 2, 4, 4), Qt::red);
        }
        check(wand.magicWand({3, 3}, 0, false, false, &error), "wand algorithm");
        check(wand.state.selection.contains({3.5, 3.5}) && !wand.state.selection.contains({1.5, 1.5}),
              "wand exact boundary");
        check(wand.magicWand({0, 0}, 0, false, false, &error) && !wand.state.selection.contains({3.5, 3.5}),
              "wand hole trace");
        wand.state.selection = {};
        wand.adjust("Invert", 0, 0, 0);
        check(wand.active()->image.pixelColor(3, 3) == QColor(Qt::cyan), "invert adjustment");
        wand.undo();
        wand.adjust("Curves", 0, 0, 0, {{0, 1}, {1, 0}});
        check(wand.active()->image.pixelColor(3, 3) == QColor(Qt::cyan), "curve adjustment");
        Document paint = Document::create(QSize(8, 8));
        paint.active()->image.fill(Qt::transparent);
        paint.begin("Alpha stroke");
        paint.paint({3, 3}, {3, 3}, 4, Qt::red, .5, false, false);
        paint.commit();
        check(qAbs(paint.active()->image.pixelColor(3, 3).alpha() - 128) <= 1, "brush alpha");
        paint.begin("Erase");
        paint.paint({3, 3}, {3, 3}, 4, Qt::black, 1, true, false);
        paint.commit();
        check(paint.active()->image.pixelColor(3, 3).alpha() == 0, "eraser");
        paint.undo();
        check(paint.active()->image.pixelColor(3, 3).alpha() > 0, "undo eraser");
        QImage beforeFill = paint.active()->image;
        paint.fill(Qt::blue, false);
        check(paint.active()->image.pixelColor(3, 3) == QColor(Qt::blue), "fill pixels");
        paint.undo();
        check(paint.active()->image == beforeFill, "fill undo preserves prior storage");
        paint.begin("Canceled stroke");
        paint.paint({3, 3}, {3, 3}, 4, Qt::green, 1, false, false);
        paint.cancel();
        check(paint.active()->image == beforeFill, "cancel stroke restores pixels");
        Document imported = Document::create(QSize(8, 8));
        imported.revision = 1;
        imported.markSaved();
        imported.fill(Qt::blue, false);
        check(imported.dirty(), "editing imported revision marks dirty");
        imported.undo();
        check(!imported.dirty(), "import revision undo returns to saved state");
        Document text = Document::create(QSize(80, 60));
        check(text.addText("Hello", QFont("Sans Serif", 12), Qt::black, QRectF(1, 1, 50, 25), &error),
              "text layer");
        check(text.addShape("ellipse", Qt::blue, QRectF(10, 30, 20, 20), &error), "shape layer");
        text.active()->mask = solid(QSize(20, 20), Qt::white);
        text.active()->flipX = true;
        QTemporaryDir temp;
        check(temp.isValid(), "temporary directory");
        QString path = temp.path() + "/test.psproj";
        check(saveProject(path, d.state, error), qPrintable(error));
        State loaded;
        check(loadProject(path, loaded, error), qPrintable(error));
        Document roundtrip;
        roundtrip.state = loaded;
        check(roundtrip.composite() == d.composite(), "project image roundtrip");
        check(saveProject(path, d.state, error), "atomic overwrite");
        check(loadProject(path, loaded, error), "overwrite load");
        QString tp = temp.path() + "/text.psproj";
        check(saveProject(tp, text.state, error) && loadProject(tp, loaded, error),
              "text shape mask roundtrip");
        check(loaded.layers.last().kind == "shape" && loaded.layers.last().flipX, "editable shape roundtrip");
        State invalid = d.state;
        invalid.layers[0].opacity = 2;
        QFile original(path + "/manifest.json");
        original.open(QIODevice::ReadOnly);
        QByteArray before = original.readAll();
        original.close();
        check(!saveProject(path, invalid, error), "invalid save rejected");
        original.open(QIODevice::ReadOnly);
        check(original.readAll() == before, "failed save preserves manifest");
        original.close();
        QJsonObject manifest = QJsonDocument::fromJson(before).object();
        QJsonArray records = manifest["layers"].toArray();
        QJsonObject record = records[0].toObject();
        record["imageFile"] = "../outside.png";
        records[0] = record;
        manifest["layers"] = records;
        QFile bad(path + "/manifest.json");
        bad.open(QIODevice::WriteOnly);
        bad.write(QJsonDocument(manifest).toJson());
        bad.close();
        State unchanged = loaded;
        check(!loadProject(path, loaded, error), "path traversal rejected");
        check(loaded.id == unchanged.id && loaded.layers.size() == unchanged.layers.size(),
              "failed load preserves destination");
        manifest = QJsonDocument::fromJson(before).object();
        records = manifest["layers"].toArray();
        record = records[0].toObject();
        record["parentID"] = record["id"];
        records[0] = record;
        manifest["layers"] = records;
        bad.open(QIODevice::WriteOnly);
        bad.write(QJsonDocument(manifest).toJson());
        bad.close();
        check(!loadProject(path, loaded, error), "cyclic group rejected");
        auto writeManifest = [&](const QJsonObject &object) {
            QFile file(path + "/manifest.json");
            check(file.open(QIODevice::WriteOnly), "write corrupt fixture");
            file.write(QJsonDocument(object).toJson());
        };
        manifest = QJsonDocument::fromJson(before).object();
        records = manifest["layers"].toArray();
        record = records[0].toObject();
        record["opacity"] = "not-a-number";
        records[0] = record;
        manifest["layers"] = records;
        writeManifest(manifest);
        check(!loadProject(path, loaded, error), "invalid numeric type rejected");
        record.remove("opacity");
        record.remove("imageFile");
        records[0] = record;
        manifest["layers"] = records;
        writeManifest(manifest);
        check(!loadProject(path, loaded, error), "missing raster reference rejected");
        QString emptyProject = temp.path() + "/empty.psproj";
        QDir().mkpath(emptyProject);
        check(saveProject(emptyProject, d.state, error), "save into empty selected directory");
        check(!saveProject(emptyProject, text.state, error), "other document overwrite rejected");
        QString comp = temp.path() + "/simple.comp";
        QDir().mkpath(comp + "/images");
        QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        solid(QSize(8, 8), Qt::red).save(comp + "/images/" + id + ".png");
        QJsonObject cr{{"id", id},
                       {"name", "Layer"},
                       {"imageFile", id + ".png"},
                       {"isVisible", true},
                       {"transform", QJsonObject{{"origin", QJsonArray{0, 0}}, {"size", QJsonArray{8, 8}}}}};
        QJsonObject cm{{"format", "com.compositor.project"},
                       {"version", 11},
                       {"colorSpace", "sRGB"},
                       {"documentID", QUuid::createUuid().toString(QUuid::WithoutBraces)},
                       {"width", 8},
                       {"height", 8},
                       {"activeLayerID", id},
                       {"layers", QJsonArray{cr}}};
        QFile cf(comp + "/manifest.json");
        cf.open(QIODevice::WriteOnly);
        cf.write(QJsonDocument(cm).toJson());
        cf.close();
        check(loadProject(comp, loaded, error), "basic Compositor import");
        cr["effects"] = QJsonObject{{"stroke", QJsonObject{{"size", 5}}}};
        cm["layers"] = QJsonArray{cr};
        cf.open(QIODevice::WriteOnly);
        cf.write(QJsonDocument(cm).toJson());
        cf.close();
        check(!loadProject(comp, loaded, error) && error.contains("effects"),
              "unsupported Compositor feature rejected");
        QString png = temp.path() + "/out.png", jpg = temp.path() + "/out.jpg";
        check(exportImage(png, d.composite(), 95, error), "PNG export");
        QImage exported(png);
        check(exported.convertToFormat(QImage::Format_ARGB32_Premultiplied) == d.composite(),
              "PNG export pixel match");
        check(exportImage(jpg, paint.composite(), 100, error), "JPEG export");
        QImage jpeg(jpg);
        check(jpeg.pixelColor(0, 0).red() > 245, "JPEG white transparency background");
        QSize oldSize = d.state.size;
        d.crop(QRect(5, 5, 20, 20));
        check(d.state.size == QSize(20, 20), "crop size");
        d.undo();
        check(d.state.size == oldSize, "undo crop");
        Document limits = Document::create(QSize(8, 8));
        limits.state.layers.resize(MaxLayers);
        check(!limits.addBlank(&error), "layer limit");
        check(!Document::validSize(QSize(8192, 8192)), "canvas pixel budget");
        Window window;
        window.show();
        app.processEvents();
        check(window.currentDocument() != nullptr, "window initial document");
        window.demo();
        app.processEvents();
        check(window.findChild<QTreeWidget *>("layerTree")->topLevelItemCount() == 7,
              "UI layers reflect document");
        Canvas *canvas = window.currentCanvas();
        canvas->setTool(Tool::RectangleSelect);
        QPointF p1 = canvas->widgetPoint({10, 10}), p2 = canvas->widgetPoint({40, 40});
        QMouseEvent press(QEvent::MouseButtonPress, p1, p1, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &press);
        QMouseEvent move(QEvent::MouseMove, p2, p2, Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &move);
        QMouseEvent release(QEvent::MouseButtonRelease, p2, p2, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(canvas, &release);
        check(window.currentDocument()->state.selection.contains({20, 20}), "canvas selection gesture");
        auto *tree = window.findChild<QTreeWidget *>("layerTree");
        QString selectedID = window.currentDocument()->state.active;
        tree->currentItem()->setText(0, "Renamed in UI");
        check(window.currentDocument()->active()->name == "Renamed in UI", "inline rename UI");
        window.currentDocument()->undo();
        check(window.currentDocument()->active()->name == "Footer text", "inline rename undo");
        tree->currentItem()->setCheckState(0, Qt::Unchecked);
        check(!window.currentDocument()->active()->visible, "visibility checkbox UI");
        window.currentDocument()->undo();
        check(window.currentDocument()->state.active == selectedID &&
                  window.currentDocument()->active()->visible,
              "visibility undo UI");
        std::cout << "Passed " << checks << " checks\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
