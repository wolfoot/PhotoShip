#include "i18n.h"
#include "persistence.h"
#include "window.h"
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <iostream>
#include <stdexcept>
using namespace ps;
static int checks = 0;
static void check(bool condition, const char *message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
static QStringList placeholders(const QString &text) {
    QStringList tokens;
    auto matches = QRegularExpression("%[1-9][0-9]*").globalMatch(text);
    while (matches.hasNext())
        tokens << matches.next().captured();
    tokens.sort();
    return tokens;
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setOrganizationName("PixelStudioTests");
    app.setApplicationName("Localization");
    QTemporaryDir temporary;
    check(temporary.isValid(), "temporary settings directory");
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, temporary.path());
    qputenv("PHOTOSHIP_TESTING", "1");
    qputenv("PHOTOSHIP_RECOVERY_DIR", temporary.filePath("recovery").toUtf8());
    try {
        check(setLanguage("en", false), "initialize English resources");
        QFile englishFile(":/i18n/en.json");
        check(englishFile.open(QIODevice::ReadOnly), "embedded reference catalog");
        auto reference = QJsonDocument::fromJson(englishFile.readAll()).object();
        Window window;
        auto *document = window.currentDocument();
        document->active()->name = "File"; // A user name matching a translation key must stay intact.
        document->edit("Fill", [&] { document->active()->image.fill(QColor("#ff8040")); });
        auto revision = document->revision;
        const auto rendered = document->composite();
        auto *blend = window.findChild<QComboBox *>("blendMode");
        auto *tabs = window.findChild<QTabWidget *>("documents");
        for (const auto &code : languageCodes()) {
            QFile file(":/i18n/" + code + ".json");
            check(file.open(QIODevice::ReadOnly), "embedded catalog exists");
            QJsonParseError error;
            auto catalog = QJsonDocument::fromJson(file.readAll(), &error).object();
            check(error.error == QJsonParseError::NoError && catalog.keys() == reference.keys(),
                  "all languages have the same complete key set");
            for (auto it = catalog.begin(); it != catalog.end(); ++it) {
                check(it.value().isString() && !it.value().toString().isEmpty(), "nonempty translation");
                check(placeholders(it.key()) == placeholders(it.value().toString()),
                      "translation preserves format placeholders");
            }
            check(setLanguage(code, false), "switch language");
            app.processEvents();
            check(currentLanguage() == code, "active language");
            check(window.currentDocument() == document && document->revision == revision &&
                      document->composite() == rendered && document->active()->name == "File",
                  "language switch preserves document, revision, pixels and user names");
            check(blend->currentData().toString() == "Normal" && blend->currentText() == ui("Normal"),
                  "blend identifiers remain canonical");
            check(tabs->tabText(0) == ui("Untitled") + " *", "live tab title translation");
            check(window.findChild<QDockWidget *>("layersDock")->windowTitle() == ui("Layers"),
                  "live panel translation");
            auto *menu = window.findChild<QMenu *>("languageMenu");
            int checked = 0;
            for (auto *choice : menu->actions()) {
                checked += choice->isChecked();
                check(choice->text() == languageName(choice->property("languageCode").toString()),
                      "language picker keeps native names");
            }
            check(checked == 1, "exclusive language selection");
            QAction *undo = nullptr;
            for (auto *action : window.findChildren<QAction *>())
                if (action->property("i18nText").toString() == "Undo")
                    undo = action;
            check(undo && undo->text() == ui("Undo %1").arg(ui("Fill")), "localized undo history");
            QDialog dialog;
            dialog.setWindowTitle("New document");
            QLabel label("Width", &dialog);
            QPushButton button("Restore checked", &dialog);
            translateWidgets(&dialog);
            check(dialog.windowTitle() == ui("New document") && label.text() == ui("Width") &&
                      button.text() == ui("Restore checked"),
                  "dialog title, label and custom button");
            check(ui("Zoom 123%") == ui("Zoom %1%").arg(123), "formatted status translation");
            check(ui("Cannot decode image: abc%1.png") == ui("Cannot decode image: %1").arg("abc%1.png"),
                  "formatted messages preserve placeholders in user arguments");
            check(ui("not a translation key") == "not a translation key", "unknown message fallback");
        }
        check(setLanguage("fr", true), "persist language");
        check(preferredLanguage() == "fr" && QSettings().value("interface/language") == "fr",
              "saved preference reloads");
        check(!setLanguage("invalid", true) && currentLanguage() == "fr" && preferredLanguage() == "fr",
              "invalid language leaves preference unchanged");
        // Localized controls must still write the English identifiers expected by the project format.
        blend->setCurrentIndex(blend->findData("Multiply"));
        check(document->active()->blend == "Multiply", "localized blend editing");
        for (auto *action : window.findChildren<QAction *>())
            if (action->property("i18nText").toString() == "Add white mask")
                action->trigger();
        check(!document->active()->mask.isNull(), "localized mask command executes original operation");
        auto icon = applicationIcon();
        for (int size : {16, 32, 48, 64, 128, 256, 512}) {
            auto pixmap = icon.pixmap(size, size);
            check(!pixmap.isNull() && pixmap.size() == QSize(size, size), "embedded icon size available");
            check(pixmap.toImage().pixelColor(0, 0).alpha() == 0, "rounded icon has transparent corners");
        }
        check(!window.windowIcon().isNull(), "window icon applied");
        // A branding change must keep preferences and recovery snapshots from the old name.
        app.setOrganizationName("PixelStudio");
        app.setApplicationName("PhotoShip");
        QSettings legacy(QSettings::defaultFormat(), QSettings::UserScope, "PixelStudio", "Pixel Studio");
        legacy.setValue("interface/language", "ja");
        legacy.sync();
        check(preferredLanguage() == "ja", "renamed app reads legacy language preference");
        check(setLanguage("de", true) && legacy.value("interface/language") == "de",
              "renamed app writes compatible language preference");
        qunsetenv("PHOTOSHIP_RECOVERY_DIR");
        qunsetenv("PIXELSTUDIO_RECOVERY_DIR");
        check(recoveryDirectory() ==
                  QFileInfo(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                          .absolutePath() +
                      "/Pixel Studio/recovery",
              "renamed app keeps the legacy recovery directory");
        qputenv("PIXELSTUDIO_RECOVERY_DIR", temporary.filePath("legacy").toUtf8());
        check(recoveryDirectory() == temporary.filePath("legacy"), "legacy recovery environment alias");
        qputenv("PHOTOSHIP_RECOVERY_DIR", temporary.filePath("recovery").toUtf8());
        check(recoveryDirectory() == temporary.filePath("recovery"),
              "new recovery environment takes priority");
        app.setOrganizationName("PixelStudioTests");
        app.setApplicationName("Localization");
        setLanguage("en", false);
        std::cout << checks << " localization and icon checks passed\n";
    } catch (const std::exception &error) {
        std::cerr << "Check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
