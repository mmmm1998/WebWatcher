#ifndef CEFENGINELOADER_HPP
#define CEFENGINELOADER_HPP

#include <tuple>

#include <QObject>
#include <QQueue>
#include <QUrl>
#include <QString>

class ChromiumWebEngineWrapper : public QObject
{
    Q_OBJECT

public slots:
    void enqueueWebwatcherTask(int64_t id, const QUrl& url, const QString& script);

signals:
    void scriptFinished(int64_t id, QString scripResult, bool haveException, QString pageTitle);

private slots:
    void checkPageLoading();
    void checkScriptEnding();

private:
    void runFirstTask();

private:
    QQueue<std::tuple<int64_t, QUrl, QString>> m_tasks;
    quint64 m_timelimiter;

    static constexpr int64_t loadingMsWait = 8000;
    static constexpr int64_t sleepIntervalMs = 100;
};

#endif // CEFENGINELOADER_HPP
