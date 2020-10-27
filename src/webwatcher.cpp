#include "webwatcher.h"

#include <cassert>

#include <QDateTime>
#include <QCryptographicHash>
#include <QTimer>
#include <QWebEnginePage>

using namespace std;

static_assert(sizeof(qint64) == sizeof(int64_t));

int64_t WebWatcher::addSite(const WatchedSiteDescription& siteDescription)
{
    if (!siteDescription.url.isValid())
        return -1;

    WatchedSite site{id_count++, siteDescription};
    qDebug() << "WebWatcher new site" << site.id;
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
        sites.erase(iter);
}

void WebWatcher::updateSite(int64_t id)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
    {
        int64_t currentMs = QDateTime::currentMSecsSinceEpoch();
        if (processed.keys().contains(id))
        {
            qDebug() << "requrest timeout" << iter->id << "time sinse request" << currentMs - processed[id];
            //TODO
        }
        else
        {
            int64_t lastUpdateTimeMs = iter->probes.size() != 0 ? iter->probes.back().accessTime : 0;
            int64_t nextUpdateDate = lastUpdateTimeMs + iter->info.updateIntervalMs;
            if (currentMs > nextUpdateDate)
                doSiteUpdate(iter);
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
            page->runJavaScript(iter->info.jsQuery, [this, page, id](QVariant result){
                this->handleJsCallback(id, result, page);
            });
        }
        else
        {
            //TODO
            qDebug() << "failed to load" << iter->info.url;
            processed.remove(id);
            page->deleteLater();
        }
    }
}

void WebWatcher::handleJsCallback(int64_t id, QVariant callbackResult, QWebEnginePage* page)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& site){return site.id == id;});
    if (iter != sites.end())
    {
        const QString& newData = callbackResult.toString();
        qDebug() << "callbackResult" << iter->page_title << newData << QDateTime::currentMSecsSinceEpoch() - (iter->probes.size() == 0 ? 0 : iter->probes.back().accessTime);
        const QByteArray& hash = QCryptographicHash::hash(newData.toUtf8(), QCryptographicHash::Md5);

        // Update stored data
        iter->page_title = page->title();

        int64_t currentMs = QDateTime::currentMSecsSinceEpoch();
        WatchedSiteProbe newProbe{currentMs, newData, hash};
        QByteArray oldHash = iter->probes.size() != 0 ? iter->probes.back().hash : QByteArray();
        assert(hash != QByteArray());
        if (oldHash != hash)
        {
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
    processed.remove(id);
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
        siteElem.setAttribute(QLatin1String("disabled"), site.info.isDisabled ? "true" : "false");

        addNamedTextNode(siteElem, doc, QLatin1String("Url"), site.info.url.toString());

        QDomElement titleElem = doc.createElement(QLatin1String("PageTitle"));
        titleElem.appendChild(doc.createTextNode(site.page_title));
        siteElem.appendChild(titleElem);

        addNamedTextNode(siteElem, doc, QLatin1String("Query"), site.info.jsQuery);
        addNamedTextNode(siteElem, doc, QLatin1String("Interval"), QString::number(site.info.updateIntervalMs));
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
    for (size_t i = 0; i < sitesElems.length(); i++)
    {
        const QDomElement& siteElem = sitesElems.item(i).toElement();

        WatchedSite site;

        assert(siteElem.attributes().contains(QLatin1String("id")));
        assert(siteElem.attributes().contains(QLatin1String("disabled")));

        site.id = siteElem.attribute(QLatin1String("id")).toLongLong();
        site.info.isDisabled = siteElem.attribute(QLatin1String("disabled")) == QLatin1String("true");

        assert(siteElem.firstChildElement(QLatin1String("Url")).isNull() == false);
        site.info.url = QUrl(siteElem.firstChildElement(QLatin1String("Url")).text());

        assert(siteElem.firstChildElement(QLatin1String("PageTitle")).isNull() == false);
        const QDomElement& titleEl = siteElem.firstChildElement(QLatin1String("PageTitle"));
        site.page_title = titleEl.text();

        assert(siteElem.firstChildElement(QLatin1String("Query")).isNull() == false);
        site.info.jsQuery = siteElem.firstChildElement(QLatin1String("Query")).text();

        assert(siteElem.firstChildElement(QLatin1String("Interval")).isNull() == false);
        site.info.updateIntervalMs = siteElem.firstChildElement(QLatin1String("Interval")).text().toLongLong();

        const QDomNodeList& probesElems = siteElem.elementsByTagName(QLatin1String("WatchedSiteProbe"));
        for (size_t j = 0; j < probesElems.length(); j++)
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

void WebWatcher::doSiteUpdate(std::vector<WatchedSite>::iterator iter)
{
    const int64_t id = iter->id;
    if (!iter->info.isDisabled)
    {
        qDebug() << "request web update for" << id << iter->page_title;
        QWebEnginePage* page = new QWebEnginePage(this);
        page->load(iter->info.url);
        connect(page, &QWebEnginePage::loadFinished, [this, id, page](bool ok){this->handlePageLoad(id, ok, page);});

        processed[id] = QDateTime::currentMSecsSinceEpoch();
    }
    QTimer::singleShot(iter->info.updateIntervalMs, this, [id, this](){this->updateSite(id);});
}

void WebWatcher::addNamedTextNode(QDomElement& elem, QDomDocument& doc, QString name, QString text)
{
    QDomElement tmp = doc.createElement(name);
    tmp.appendChild(doc.createTextNode(text));
    elem.appendChild(tmp);
}

void WebWatcher::removeSiteProbe(std::int64_t site_id, std::int64_t probe_number)
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
            doSiteUpdate(iter);
        else
            QTimer::singleShot(nextUpdateDate-currentMs, this, [site_id, this](){this->updateSite(site_id);});
    }
}
