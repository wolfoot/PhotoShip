#pragma once
#include "document.h"
#include <QObject>
#include <QThreadPool>
namespace ps {
struct JobResult {
    bool ok = false;
    QString error;
};
class Persistence : public QObject {
  public:
    explicit Persistence(QObject *parent = nullptr);
    ~Persistence() override;
    void submit(std::function<JobResult()> work, std::function<void(JobResult)> done = {});
    void wait();

  private:
    QThreadPool pool;
};
QString recoveryDirectory();
bool saveRecovery(const QString &path, const State &state, const QString &original, quint64 revision,
                  QString &error);
bool loadRecovery(const QString &path, State &state, QString &original, QString &error);
bool removeRecovery(const QString &path);
} // namespace ps
