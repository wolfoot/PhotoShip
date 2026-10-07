#include "i18n.h"
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QEvent>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QReadWriteLock>
#include <QRegularExpression>
#include <QSettings>
#include <QToolBar>
#include <QTranslator>
#include <QTreeWidget>

// Referencing the initializer ensures the static library's resources are linked.
static void initializeResources() {
    Q_INIT_RESOURCE(resources);
}
namespace ps {
namespace {
class Catalog : public QTranslator {
  public:
    QJsonObject strings;
    QString code = "en";
    mutable QReadWriteLock lock;
    QString translate(const char *, const char *source, const char *, int) const override {
        const QString original = QString::fromUtf8(source);
        const QString result = ui(original);
        return result == original ? QString() : result;
    }
    bool isEmpty() const override {
        QReadLocker guard(&lock);
        return strings.isEmpty();
    }
    bool eventFilter(QObject *object, QEvent *event) override {
        // Dialogs are constructed on demand, after the language was selected.
        if (event->type() == QEvent::Show)
            if (auto *dialog = qobject_cast<QDialog *>(object))
                translateWidgets(dialog);
        return false;
    }
};
Catalog *catalog() {
    static Catalog *instance = nullptr;
    if (!instance) {
        initializeResources();
        instance = new Catalog;
        instance->setParent(qApp);
        qApp->installEventFilter(instance);
    }
    return instance;
}
// Keep preferences from the former name without changing the user-facing application name.
QSettings interfaceSettings() {
    const QString name = QCoreApplication::applicationName();
    return QSettings(QSettings::defaultFormat(), QSettings::UserScope, QCoreApplication::organizationName(),
                     name == "PhotoShip" && QCoreApplication::organizationName() == "PixelStudio"
                         ? QString("Pixel Studio")
                         : name);
}
QString remembered(QObject *object, const char *property, const QString &text) {
    auto saved = object->property(property);
    if (!saved.isValid()) {
        if (text.isEmpty())
            return text;
        object->setProperty(property, text);
        saved = text;
    }
    return ui(saved.toString());
}
} // namespace
QStringList languageCodes() {
    return {"en", "zh_CN", "ja", "ko", "fr", "de", "es"};
}
QString languageName(const QString &code) {
    const QStringList names = {"English", "简体中文", "日本語", "한국어", "Français", "Deutsch", "Español"};
    int index = languageCodes().indexOf(code);
    return index < 0 ? code : names.at(index);
}
QString currentLanguage() {
    auto *translator = catalog();
    QReadLocker guard(&translator->lock);
    return translator->code;
}
QString preferredLanguage() {
    const auto saved = interfaceSettings().value("interface/language").toString();
    if (languageCodes().contains(saved))
        return saved;
    QString code = QLocale::system().name();
    if (code.startsWith("zh"))
        return "zh_CN";
    code = code.section('_', 0, 0);
    return languageCodes().contains(code) ? code : "en";
}
bool setLanguage(const QString &code, bool persist) {
    if (!languageCodes().contains(code))
        return false;
    auto *translator = catalog();
    QFile file(":/i18n/" + code + ".json");
    if (!file.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return false;
    qApp->removeTranslator(translator);
    {
        QWriteLocker guard(&translator->lock);
        translator->strings = document.object();
        translator->code = code;
    }
    QLocale::setDefault(QLocale(code));
    if (persist)
        interfaceSettings().setValue("interface/language", code);
    qApp->installTranslator(translator);
    return true;
}
QString ui(const QString &source) {
    auto *translator = catalog();
    QReadLocker guard(&translator->lock);
    if (translator->code == "en")
        return source;
    const auto direct = translator->strings.value(source);
    if (direct.isString())
        return direct.toString();
    QString normalized = source.trimmed();
    const int accelerator = normalized.indexOf('&');
    const QString key = accelerator >= 0 ? normalized.mid(accelerator + 1, 1) : QString();
    normalized.remove('&');
    QString suffix;
    if (normalized.endsWith("…")) {
        suffix = "…";
        normalized.chop(1);
    } else if (normalized.endsWith("...")) {
        suffix = "...";
        normalized.chop(3);
    }
    const auto simple = translator->strings.value(normalized);
    if (simple.isString() && translator->code != "en") {
        const int start = source.indexOf(source.trimmed());
        return source.left(start) + simple.toString() + suffix +
               (key.isEmpty() ? QString() : " (&" + key.toUpper() + ")") +
               source.mid(start + source.trimmed().size());
    }
    // Translate already formatted status/error messages without touching their arguments.
    for (auto it = translator->strings.begin(); it != translator->strings.end(); ++it) {
        const QString key = it.key();
        if (!key.contains("%1"))
            continue;
        QString expression;
        int count = 0;
        int keyOffset = 0;
        while (key.indexOf("%" + QString::number(count + 1), keyOffset) >= 0) {
            ++count;
            const QString placeholder = "%" + QString::number(count);
            const int at = key.indexOf(placeholder, keyOffset);
            expression += QRegularExpression::escape(key.mid(keyOffset, at - keyOffset));
            expression += "([\\s\\S]*?)";
            keyOffset = at + placeholder.size();
        }
        expression += QRegularExpression::escape(key.mid(keyOffset));
        auto match = QRegularExpression("\\A" + expression + "\\z").match(source);
        if (match.hasMatch()) {
            const QString translated = it.value().toString();
            QString result;
            int offset = 0;
            auto placeholders = QRegularExpression("%([1-9][0-9]*)").globalMatch(translated);
            while (placeholders.hasNext()) {
                const auto placeholder = placeholders.next();
                result += translated.mid(offset, placeholder.capturedStart() - offset);
                result += match.captured(placeholder.captured(1).toInt());
                offset = placeholder.capturedEnd();
            }
            return result + translated.mid(offset);
        }
    }
    return source;
}
void translateWidgets(QWidget *root) {
    root->setLocale(QLocale(currentLanguage()));
    if (qobject_cast<QDialog *>(root))
        root->setWindowTitle(remembered(root, "i18nTitle", root->windowTitle()));
    for (auto *label : root->findChildren<QLabel *>())
        label->setText(remembered(label, "i18nText", label->text()));
    for (auto *button : root->findChildren<QAbstractButton *>())
        button->setText(remembered(button, "i18nText", button->text()));
    for (auto *action : root->findChildren<QAction *>()) {
        if (!action->property("languageCode").isValid())
            action->setText(remembered(action, "i18nText", action->text()));
    }
    for (auto *dock : root->findChildren<QDockWidget *>())
        dock->setWindowTitle(remembered(dock, "i18nTitle", dock->windowTitle()));
    for (auto *toolbar : root->findChildren<QToolBar *>())
        toolbar->setWindowTitle(remembered(toolbar, "i18nTitle", toolbar->windowTitle()));
    // Only column headings, never editable layer names or recovery file names.
    for (auto *tree : root->findChildren<QTreeWidget *>())
        for (int i = 0; i < tree->columnCount(); ++i) {
            auto property = QByteArray("i18nHeader") + QByteArray::number(i);
            tree->headerItem()->setText(i,
                                        remembered(tree, property.constData(), tree->headerItem()->text(i)));
        }
}
QIcon applicationIcon() {
    catalog();
    QIcon icon;
    for (int size : {16, 32, 48, 64, 128, 256, 512})
        icon.addFile(QString(":/icons/photoship-%1.png").arg(size), QSize(size, size));
    return icon;
}
} // namespace ps
