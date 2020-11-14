#include "cefenginewrapper.hpp"

#include <QDateTime>
#include <QTimer>

#include "logging.hpp"

#include <cefengine.hpp>

void ChromiumWebEngineWrapper::enqueueWebwatcherTask(int64_t id, const QUrl& url, const QString& script)
{
    m_tasks.enqueue({id, url, script});
    if (m_tasks.size() == 1)
        runFirstTask();
}

void ChromiumWebEngineWrapper::runFirstTask()
{
    int64_t id = std::get<0>(m_tasks.head());
    Log::info("[watcher chromium] Start working on task #%ld", id);
    const QUrl& url = std::get<1>(m_tasks.head());
    qint64 currectTime = QDateTime::currentMSecsSinceEpoch();
    m_timelimiter = loadingMsWait + currectTime;

    Log::info("[watcher chromium] Start load watched site #%ld (%s) with Cloudflare protection bypass", id, url.toString().toLocal8Bit().data());
    ChromiumWebEngine::loadPage(url.toString().toStdString());
    checkPageLoading();
}

void ChromiumWebEngineWrapper::checkPageLoading()
{
    quint64 currectTime = QDateTime::currentMSecsSinceEpoch();
    // Forse wait some seconds, because Cloudflare DDoS protection takes ~5 seconds
    if (m_timelimiter > currectTime)
    {
        ChromiumWebEngine::handleEvents();
        QTimer::singleShot(sleepIntervalMs, this, &ChromiumWebEngineWrapper::checkPageLoading);
        return;
    }

    if (!ChromiumWebEngine::isLoadingFinished())
    {
        ChromiumWebEngine::handleEvents();
        QTimer::singleShot(sleepIntervalMs, this, &ChromiumWebEngineWrapper::checkPageLoading);
        return;
    }

    int64_t id = std::get<0>(m_tasks.head());
    Log::info("[watcher chromium] Loading page for watched site #%ld finished, run JS query script ", id);
    const QString& script = std::get<2>(m_tasks.head());
    ChromiumWebEngine::runScript(script.toStdString());
    checkScriptEnding();
}

void ChromiumWebEngineWrapper::checkScriptEnding()
{
    if (!ChromiumWebEngine::isScriptResultReady())
    {
        ChromiumWebEngine::handleEvents();
        QTimer::singleShot(sleepIntervalMs, this, &ChromiumWebEngineWrapper::checkScriptEnding);
        return;
    }

    int64_t id = std::get<0>(m_tasks.head());
    const std::string& scriptResult = ChromiumWebEngine::scriptResult();
    const std::string& pageTitle = ChromiumWebEngine::getPageTitle();
    bool haveException = ChromiumWebEngine::haveScriptException();
    Log::info("[watcher chromium] Script for watched site #%ld finished, go to next task", id);
    emit scriptFinished(id, QString::fromStdString(scriptResult), haveException, QString::fromStdString(pageTitle));

    m_tasks.dequeue();
    Log::info("[watcher chromium] Finished task (#%ld) have been dequeue, currect queue size is %lu", id, m_tasks.size());
    if (m_tasks.size() != 0)
        runFirstTask();
}
