#include "webwatcher.hpp"

#include <thread>
#include <cassert>

#include <QDateTime>
#include <QCryptographicHash>
#include <QTimer>
#include <QWebEnginePage>

#include "logging.hpp"


using namespace std;

static_assert(sizeof(qint64) == sizeof(int64_t));

QString WebWatcher::kCloudflareProtectedPageTitle = QString::fromLatin1("Just a moment...");
QString WebWatcher::kCloudflareProtectedPageTextMark1 = QString::fromLatin1("DDoS protection by <a rel=\"noopener noreferrer\" href=\"https://www.cloudflare.com/5xx-error-landing/\" target=\"_blank\">Cloudflare</a>");
QString WebWatcher::kCloudflareProtectedPageTextMark2 = QString::fromLatin1("<span class=\"ray_id\">Ray ID");

QString WebWatcher::kTrue = QString::fromLatin1("true");
QString WebWatcher::kFalse = QString::fromLatin1("false");

WebWatcher::WebWatcher(): QObject()
{
    connect(&chromeEngine, &ChromiumWebEngineWrapper::scriptFinished, this, &WebWatcher::handleJsCallbackFromChromeEngine);
}

WebWatcher::~WebWatcher()
{
}

int64_t WebWatcher::addSite(const WatchedSiteDescription& siteDescription)
{
    if (!siteDescription.url.isValid())
        return -1;

    WatchedSite site{id_count++, siteDescription};
    Log::info("[watcher] Add new watched site #%ld", site.id);
    sites.push_back(site);

    updateSite(site.id);
    return site.id;
}

bool WebWatcher::updateSite(std::int64_t id, const WatchedSiteDescription& siteDescription, bool allowResetLoadedData)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& iter_site){return iter_site.id == id;});
    if (iter != sites.end())
    {
        bool needResetProbes = allowResetLoadedData && (iter->info.url != siteDescription.url || iter->info.jsQuery != siteDescription.jsQuery);

        //With changing site maybe we don't need bypass for this id
        if (iter->info.url != siteDescription.url && needCloudflareBypass.contains(id))
            needCloudflareBypass.remove(iter->id);

        iter->info = siteDescription;
        if (needResetProbes)
        {
            iter->probes.clear();
            iter->page_title.clear();
        }
        updateSite(id);
        return true;
    }
    else
        return false;
}

void WebWatcher::removeSite(int64_t id)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
    {
        Log::info("[watcher] Remove watched site #%ld", id);
        sites.erase(iter);
    }

    if (needCloudflareBypass.contains(id))
        needCloudflareBypass.remove(id);
}

void WebWatcher::updateSite(int64_t id)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
    {
        int64_t currentMs = QDateTime::currentMSecsSinceEpoch();
        int64_t lastUpdateTimeMs = iter->probes.size() != 0 ? iter->probes.back().accessTime : 0;
        int64_t nextUpdateDate = lastUpdateTimeMs + iter->info.updateIntervalMs;
        if (processed.keys().contains(id))
        {
            Log::error("[watcher] Request for #%ld have timeouted by %d seconds, so something wrong", id, currentMs - processed[id]);
            emit requestOutdated(id, currentMs - processed[id]);
        }
        else
        {
            if (currentMs > nextUpdateDate)
                doSiteUpdate(iter->id, iter->info, iter->page_title);
            else
                QTimer::singleShot(nextUpdateDate-currentMs, this, [id, this](){this->updateSite(id);});
        }
    }
}

std::optional<WatchedSite> WebWatcher::siteById(int64_t id)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
        return *iter;
    else
        return std::nullopt;
}

void WebWatcher::handlePageLoad(int64_t id, bool success, QWebEnginePage* page)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
    {
        if (success && iter->info.url == page->requestedUrl())
        {
            Log::info("[watcher] Requested webpage for watched site #%ld have loaded successfully, run JS query", id);
            if (page->title() == kCloudflareProtectedPageTitle)
            {
                Log::info("[watcher] Suspect that watched site #%ld protected by Cloudflare DDoS protection (matched page's titles), need additional invastigation", id);
                page->toHtml([this, page, siteInfo=iter->info, pageTitle=iter->page_title, id](const QString& html) {
                    if (html.contains(kCloudflareProtectedPageTextMark1) && html.contains(kCloudflareProtectedPageTextMark2))
                    {
                        Log::info("[watcher] Found that watched site #%ld is protected by Cloudflare, enable Cloudflare bypass", id);
                        needCloudflareBypass.insert(id);
                        page->deleteLater();
                        this->doSiteUpdate(id, siteInfo, pageTitle);
                    }
                    else
                    {
                        Log::info("[watcher] Watched site #%ld don't protected by Cloudflare, go to standart scrapping process", id);
                        page->runJavaScript(siteInfo.jsQuery, [this, page, id](QVariant result){
                            this->handleJsCallbackFromQtWebEngine(id, result, page);
                        });
                    }
                });
            }
            else
                page->runJavaScript(iter->info.jsQuery, [this, page, id](QVariant result){
                    this->handleJsCallbackFromQtWebEngine(id, result, page);
                });
        }
        else
        {
            Log::error("[watcher] Fail to load webpage for watched site #%ld, so something wrong", id);
            emit failToLoadPage(id, iter->info.url);
            processed.remove(id);
            page->deleteLater();
        }
    }
}

void WebWatcher::handleJsCallbackFromQtWebEngine(int64_t id, QVariant callbackResult, QWebEnginePage* page)
{
    //Qt don't allow getting information about exception - if error apears, then the callback result will be just empty string
    handleJsCallback(id, callbackResult.toString(), page->title(), false, QString());
    if (callbackResult.toString().isEmpty())
    {
        //Error possiblity
        Log::warn("[watcher] Javascript query execution for warched site #%ld finished with empty string. With Qt webengine it can means exception", id);
        emit possibleQtExceptionOccurred(id);
    }

    page->deleteLater();
}

QDomElement WebWatcher::toXml(QDomDocument& doc)
{
    QDomElement root = doc.createElement(QLatin1String("WebWatcher"));

    root.setAttribute(QLatin1String("id_counter"), QString::number(id_count));

    for(const WatchedSite& site: sites)
    {
        QDomElement siteElem = doc.createElement(QLatin1String("WatchedSite"));

        siteElem.setAttribute(QLatin1String("id"), QString::number(site.id));
        siteElem.setAttribute(QLatin1String("disabled"), site.info.isDisabled ? kTrue : kFalse);

        addNamedTextNode(siteElem, doc, QLatin1String("Url"), site.info.url.toString());

        QDomElement titleElem = doc.createElement(QLatin1String("PageTitle"));
        titleElem.appendChild(doc.createTextNode(site.page_title));
        siteElem.appendChild(titleElem);

        addNamedTextNode(siteElem, doc, QLatin1String("Query"), site.info.jsQuery);
        addNamedTextNode(siteElem, doc, QLatin1String("Interval"), QString::number(site.info.updateIntervalMs));
        addNamedTextNode(siteElem, doc, QLatin1String("NeedCloudflareBypass"), needCloudflareBypass.contains(site.id) ? kTrue : kFalse);

        for (const WatchedSiteProbe& probe: site.probes)
        {
            QDomElement probeElem  = doc.createElement(QLatin1String("WatchedSiteProbe"));
            addNamedTextNode(probeElem, doc, QLatin1String("AccessTime"), QString::number(probe.accessTime));
            addNamedTextNode(probeElem, doc, QLatin1String("Text"), probe.text);
            addNamedTextNode(probeElem, doc, QLatin1String("Hash"), QString::fromLatin1(probe.hash.toBase64()));
            siteElem.appendChild(probeElem);
        }
        root.appendChild(siteElem);
    }

    return root;
}

void WebWatcher::fromXml(const QDomElement& content)
{
    assert(content.tagName() == QLatin1String("WebWatcher"));

    assert(content.attributes().contains(QLatin1String("id_counter")));

    id_count = content.attribute(QLatin1String("id_counter")).toLongLong();

    const QDomNodeList& sitesElems = content.elementsByTagName(QLatin1String("WatchedSite"));
    for (int i = 0; i < sitesElems.length(); i++)
    {
        const QDomElement& siteElem = sitesElems.item(i).toElement();

        WatchedSite site;

        assert(siteElem.attributes().contains(QLatin1String("id")));
        assert(siteElem.attributes().contains(QLatin1String("disabled")));

        site.id = siteElem.attribute(QLatin1String("id")).toLongLong();
        site.info.isDisabled = siteElem.attribute(QLatin1String("disabled")) == kTrue;

        assert(siteElem.firstChildElement(QLatin1String("Url")).isNull() == false);
        site.info.url = QUrl(siteElem.firstChildElement(QLatin1String("Url")).text());

        assert(siteElem.firstChildElement(QLatin1String("PageTitle")).isNull() == false);
        const QDomElement& titleEl = siteElem.firstChildElement(QLatin1String("PageTitle"));
        site.page_title = titleEl.text();

        assert(siteElem.firstChildElement(QLatin1String("Query")).isNull() == false);
        site.info.jsQuery = siteElem.firstChildElement(QLatin1String("Query")).text();

        assert(siteElem.firstChildElement(QLatin1String("Interval")).isNull() == false);
        site.info.updateIntervalMs = siteElem.firstChildElement(QLatin1String("Interval")).text().toLongLong();

        //NOTE: Backward compatibility, so not a strict check
        const QDomElement& bypassElem = siteElem.firstChildElement(QLatin1String("NeedCloudflareBypass"));
        if (!bypassElem.isNull())
        {
            if (bypassElem.text() == kTrue)
                needCloudflareBypass.insert(site.id);
        }

        const QDomNodeList& probesElems = siteElem.elementsByTagName(QLatin1String("WatchedSiteProbe"));
        for (int j = 0; j < probesElems.length(); j++)
        {
            const QDomElement& probeElem = probesElems.item(j).toElement();

            WatchedSiteProbe probe;

            assert(probeElem.firstChildElement(QLatin1String("AccessTime")).isNull() == false);
            probe.accessTime = probeElem.firstChildElement(QLatin1String("AccessTime")).text().toLongLong();

            assert(probeElem.firstChildElement(QLatin1String("Text")).isNull() == false);
            probe.text = probeElem.firstChildElement(QLatin1String("Text")).text();

            assert(probeElem.firstChildElement(QLatin1String("Hash")).isNull() == false);
            probe.hash = QByteArray::fromBase64(probeElem.firstChildElement(QLatin1String("Hash")).text().toLatin1());

            site.probes.push_back(probe);
        }

        sites.push_back(site);
    }

    // Check need to update after loading
    for (const WatchedSite& site: sites)
        updateSite(site.id);

}

QList<int64_t> WebWatcher::ids()
{
    QList<int64_t> list;
    for (const WatchedSite& site: sites)
        list.append(site.id);
    return list;
}

void WebWatcher::doSiteUpdate(std::int64_t id, const WatchedSiteDescription& info, const QString& page_title)
{
    if (!info.isDisabled)
    {
        Log::info("[watcher] Request update for watched site #%ld (%s)", id, page_title.toLocal8Bit().data());
        if (needCloudflareBypass.contains(id))
        {
            Log::info("[watcher] Use Cloudflare bypass for loading watched site #%ld", id);
            chromeEngine.enqueueWebwatcherTask(id, info.url, info.jsQuery);
        }
        else
        {
            QWebEnginePage* page = new QWebEnginePage(this);
            connect(page, &QWebEnginePage::loadFinished, [this, id, page](bool ok){this->handlePageLoad(id, ok, page);});
            if (Log::willBeLogged(Log::Level::Debug))
            {
                connect(page, &QWebEnginePage::loadProgress, [id](int progress){
                    Log::debug("[watcher] watched site #%ld request progress %d%", id, progress);
                });
            }
            page->load(info.url);
        }

        processed[id] = QDateTime::currentMSecsSinceEpoch();
    }
    Log::debug("[watcher] Plan to next update for #%ld in %f seconds", id, info.updateIntervalMs / 1000.0);
    QTimer::singleShot(info.updateIntervalMs, this, [id, this](){this->updateSite(id);});
}

void WebWatcher::addNamedTextNode(QDomElement& elem, QDomDocument& doc, QString name, QString text)
{
    QDomElement tmp = doc.createElement(name);
    tmp.appendChild(doc.createTextNode(text));
    elem.appendChild(tmp);
}

void WebWatcher::removeSiteProbe(std::int64_t site_id, size_t probe_number)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [site_id](const WatchedSite& site){return site.id == site_id;});
    if (iter != sites.end())
    {
        if (probe_number < iter->probes.size())
            iter->probes.erase(iter->probes.begin() + probe_number);

        //Check, need we to change next update time, or we even should run update right now
        int64_t currentMs = QDateTime::currentMSecsSinceEpoch();
        int64_t lastUpdateTimeMs = iter->probes.size() != 0 ? iter->probes.back().accessTime : 0;
        int64_t nextUpdateDate = lastUpdateTimeMs + iter->info.updateIntervalMs;
        if (currentMs > nextUpdateDate)
            doSiteUpdate(iter->id, iter->info, iter->page_title);
        else
            QTimer::singleShot(nextUpdateDate-currentMs, this, [site_id, this](){this->updateSite(site_id);});
    }
}

void WebWatcher::handleJsCallbackFromChromeEngine(std::int64_t id, const QString& scriptResult, bool haveException, const QString& pageTitle)
{
    handleJsCallback(id, scriptResult, pageTitle, haveException, scriptResult);
}

void WebWatcher::handleJsCallback(std::int64_t id, const QString& newData, const QString& pageTitle, bool haveException, QString exceptionText)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
    {
        Log::info("[watcher] Successfuly run JS query for watched site #%ld", id);
        Log::debug("[watcher] Probe result \"%s\"", newData.toLocal8Bit().data());
        Log::info("[watcher] %f seconds have passed since last probe", (QDateTime::currentMSecsSinceEpoch() - (iter->probes.size() == 0 ? 0 : iter->probes.back().accessTime))/1000.0);

        // Update stored data
        iter->page_title = pageTitle;

        if (haveException)
        {
            Log::info("[watcher] Execution JS query for watched site #%ld have finished with exception, so report it", id);
            emit exceptionOccurred(id, exceptionText);
        }
        else
        {
            const QByteArray& hash = QCryptographicHash::hash(newData.toUtf8(), QCryptographicHash::Md5);

            int64_t currentMs = QDateTime::currentMSecsSinceEpoch();
            WatchedSiteProbe newProbe{currentMs, newData, hash};
            QByteArray oldHash = iter->probes.size() != 0 ? iter->probes.back().hash : QByteArray();
            assert(hash != QByteArray());
            if (oldHash != hash)
            {
                Log::info("[watcher] Query result for watched site #%ld have changed, so emit signal", id);
                iter->probes.push_back(newProbe);
                emit siteAcessed(id);
                emit siteChanged(id);
            }
            else
            {
                iter->probes.back().accessTime = currentMs;
                emit siteAcessed(id);
            }
        }

    }
    processed.remove(id);
}
