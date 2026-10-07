#include "i18n.h"
#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QTimer>
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("PhotoShip");
    app.setApplicationVersion("0.2.1");
    app.setOrganizationName("PixelStudio");
    ps::setLanguage(ps::preferredLanguage(), false);
    app.setWindowIcon(ps::applicationIcon());
    app.setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#2b2f37"));
    palette.setColor(QPalette::WindowText, QColor("#e5e9f2"));
    palette.setColor(QPalette::Base, QColor("#23262d"));
    palette.setColor(QPalette::AlternateBase, QColor("#30353f"));
    palette.setColor(QPalette::Text, QColor("#e5e9f2"));
    palette.setColor(QPalette::Button, QColor("#343944"));
    palette.setColor(QPalette::ButtonText, QColor("#e5e9f2"));
    palette.setColor(QPalette::Highlight, QColor("#5373cc"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#777f8f"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#777f8f"));
    app.setPalette(palette);
    app.setStyleSheet(
        "QMainWindow { background:#202329; } QToolBar { spacing:6px; padding:7px; border:0; "
        "background:#2b2f37; } QToolButton { padding:8px 7px; border-radius:4px; } QToolButton:checked { "
        "background:#4965ab; } QDockWidget::title { padding:9px; background:#30353f; font-weight:bold; } "
        "QTreeWidget { border:0; } QTreeWidget::item { padding:5px; } QTabBar::tab { padding:10px 24px; } "
        "QTabBar::tab:selected { background:#3b424f; } QPushButton { padding:6px; } QMenu { padding:5px; } "
        "QStatusBar { color:#bac3d5; }");
    QCommandLineParser parser;
    parser.setApplicationDescription("PhotoShip — layer-based image editor");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"demo", "Open a built-in demo document."});
    parser.addOption(
        {"screenshot", "Write a demo UI screenshot and exit (for visual verification).", "path"});
    parser.addPositionalArgument("files", "Images, .psproj/.comp folders, or project manifests.", "[files…]");
    parser.addOption(
        {"language", "Interface language: en, zh_CN, ja, ko, fr, de, es (one launch only).", "code"});
    parser.process(app);
    if (parser.isSet("language") && !ps::setLanguage(parser.value("language"), false))
        parser.showHelp(1);
    ps::Window window;
    window.show();
    for (const auto &file : parser.positionalArguments())
        window.openPath(file);
    if (parser.isSet("demo") || parser.isSet("screenshot"))
        window.demo();
    if (parser.isSet("screenshot")) {
        QString path = parser.value("screenshot");
        QTimer::singleShot(250, &app,
                           [&app, &window, path]() { app.exit(window.writeScreenshot(path) ? 0 : 1); });
    }
    return app.exec();
}
