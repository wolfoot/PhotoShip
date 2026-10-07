#pragma once
#include <QIcon>
#include <QStringList>
class QWidget;
namespace ps {
QStringList languageCodes();
QString languageName(const QString &code);
QString currentLanguage();
QString preferredLanguage();
bool setLanguage(const QString &code, bool persist = true);
QString ui(const QString &source);
void translateWidgets(QWidget *root);
QIcon applicationIcon();
} // namespace ps
