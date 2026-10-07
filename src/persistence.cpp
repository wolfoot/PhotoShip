#include "persistence.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>
namespace ps {
Persistence::Persistence(QObject *parent) : QObject(parent) {
    pool.setMaxThreadCount(1);
}
Persistence::~Persistence() {
    pool.waitForDone();
}
void Persistence::wait() {
    pool.waitForDone();
}
void Persistence::submit(std::function<JobResult()> work, std::function<void(JobResult)> done) {
    auto *watcher = new QFutureWatcher<JobResult>(this);
    connect(watcher, &QFutureWatcher<JobResult>::finished, this, [watcher, done] {
        auto result = watcher->result();
        watcher->deleteLater();
        if (done)
            done(result);
    });
    watcher->setFuture(QtConcurrent::run(&pool, [work] {
        try {
            return work();
        } catch (const std::exception &e) {
            return JobResult{false, QString::fromUtf8(e.what())};
        } catch (...) {
            return JobResult{false, "Background operation failed."};
        }
    }));
}
QString recoveryDirectory() {
    QString override = qEnvironmentVariable("PHOTOSHIP_RECOVERY_DIR");
    if (override.isEmpty())
        override = qEnvironmentVariable("PIXELSTUDIO_RECOVERY_DIR");
    if (!override.isEmpty())
        return override;
    QString root = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    // Existing recovery snapshots must remain discoverable after the application rename.
    if (QCoreApplication::applicationName() == "PhotoShip" &&
        QCoreApplication::organizationName() == "PixelStudio")
        root = QFileInfo(root).absolutePath() + "/Pixel Studio";
    return root + "/recovery";
}
bool saveRecovery(const QString &path, const State &state, const QString &original, quint64 revision,
                  QString &error) {
    if (!saveProject(path, state, error))
        return false;
    QSaveFile file(QDir(path).filePath("recovery.json"));
    QByteArray data =
        QJsonDocument(QJsonObject{{"originalPath", original},
                                  {"revision", QString::number(revision)},
                                  {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}})
            .toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        error = "Recovery pixels were saved, but recovery metadata could not be committed.";
        return false;
    }
    return true;
}
bool loadRecovery(const QString &path, State &state, QString &original, QString &error) {
    if (!loadProject(path, state, error))
        return false;
    QFile file(QDir(path).filePath("recovery.json"));
    original.clear();
    if (file.open(QIODevice::ReadOnly) && file.size() < 16384)
        original = QJsonDocument::fromJson(file.readAll()).object()["originalPath"].toString();
    return true;
}
bool removeRecovery(const QString &path) {
    QFileInfo info(path);
    QDir root(recoveryDirectory());
    if (info.isSymLink() || info.absolutePath() != root.absolutePath() ||
        !info.fileName().endsWith(".psproj") || QUuid(info.completeBaseName()).isNull())
        return false;
    return !info.exists() || QDir(path).removeRecursively();
}
} // namespace ps
