#include "webwatcher.h"

#include <cassert>

#include <QDateTime>
#include <QCryptographicHash>
#include <QTimer>
#include <QWebEnginePage>

using namespace std;

static_assert(sizeof(qint64) == sizeof(int64_t));

int64_t WebWatcher::addSite(QUrl url, QString title, QString xmlQuery, std::int64_t updateIntervalMs)
{
    if (!url.isValid())
        return -1;

    WatchedSite site{id_count++, url, title, title.isEmpty() == false, false, xmlQuery, updateIntervalMs};
    qDebug() << "WebWatcher new site" << site.id;
    sites.push_back(site);

    updateSite(site.id);
    return site.id;
}

bool WebWatcher::updateSite(const WatchedSite& site, bool resetProbes)
{
    int64_t id = site.id;
    auto iter = std::find_if(sites.begin(), sites.end(), [id](const WatchedSite& iter_site){return iter_site.id == id;});
    if (iter != sites.end())
    {
        bool needResetProbes = resetProbes && (iter->url != site.url || iter->jsQuery != site.jsQuery);

        *iter = site;
         if (needResetProbes)
            iter->probes.clear();
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
            int64_t nextUpdateDate = lastUpdateTimeMs + iter->updateIntervalMs;
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
        if (success && iter->url == page->requestedUrl())
        {
            page->runJavaScript(iter->jsQuery, [this, page, id](QVariant result){
                this->handleJsCallback(id, result, page);
            });
        }
        else
        {
            //TODO
            qDebug() << "failed to load" << iter->url;
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
        qDebug() << "callbackResult" << iter->title << newData << QDateTime::currentMSecsSinceEpoch() - (iter->probes.size() == 0 ? 0 : iter->probes.back().accessTime);
        const QByteArray& hash = QCryptographicHash::hash(newData.toUtf8(), QCryptographicHash::Md5);

        // Update stored data
        if (!iter->isManualTitle)
            iter->title = page->title();

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
    root.setAttribute(QLatin1String("sites_count"), QString::number(sites.size()));

    for(const WatchedSite& site: sites)
    {
        QDomElement siteElem = doc.createElement(QLatin1String("WatchedSite"));


        siteElem.setAttribute(QLatin1String("id"), QString::number(site.id));
        siteElem.setAttribute(QLatin1String("probes_count"), QString::number(site.probes.size()));
        siteElem.setAttribute(QLatin1String("disabled"), site.isDisabled ? "true" : "false");

        addNamedTextNode(siteElem, doc, QLatin1String("Url"), site.url.toString());

        QDomElement titleElem = doc.createElement(QLatin1String("Title"));
        titleElem.setAttribute(QLatin1String("manual"), site.isManualTitle ? "true" : "false");
        titleElem.appendChild(doc.createTextNode(site.title));
        siteElem.appendChild(titleElem);

        addNamedTextNode(siteElem, doc, QLatin1String("Query"), site.jsQuery);
        addNamedTextNode(siteElem, doc, QLatin1String("Interval"), QString::number(site.updateIntervalMs));
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
    assert(content.attributes().contains(QLatin1String("sites_count")));

    id_count = content.attribute(QLatin1String("id_counter")).toLongLong();
    size_t count = content.attribute(QLatin1String("sites_count")).toLongLong();

    sites.reserve(count);
    const QDomNodeList& sitesElems = content.elementsByTagName(QLatin1String("WatchedSite"));
    for (size_t i = 0; i < count; i++)
    {
        const QDomElement& siteElem = sitesElems.item(i).toElement();

        WatchedSite site;

        assert(siteElem.attributes().contains(QLatin1String("id")));
        assert(siteElem.attributes().contains(QLatin1String("probes_count")));
        assert(siteElem.attributes().contains(QLatin1String("disabled")));

        site.id = siteElem.attribute(QLatin1String("id")).toLongLong();
        size_t probesCount = (size_t)siteElem.attribute(QLatin1String("probes_count")).toLongLong();
        site.isDisabled = siteElem.attribute(QLatin1String("disabled")) == QLatin1String("true");

        assert(siteElem.firstChildElement(QLatin1String("Url")).isNull() == false);
        site.url = QUrl(siteElem.firstChildElement(QLatin1String("Url")).text());

        assert(siteElem.firstChildElement(QLatin1String("Title")).isNull() == false);
        const QDomElement& titleEl = siteElem.firstChildElement(QLatin1String("Title"));
        assert(titleEl.attributes().contains(QLatin1String("manual")));
        site.title = titleEl.text();
        site.isManualTitle = titleEl.attribute(QLatin1String("manual")) == QLatin1String("true");

        assert(siteElem.firstChildElement(QLatin1String("Query")).isNull() == false);
        site.jsQuery = siteElem.firstChildElement(QLatin1String("Query")).text();

        assert(siteElem.firstChildElement(QLatin1String("Interval")).isNull() == false);
        site.updateIntervalMs = siteElem.firstChildElement(QLatin1String("Interval")).text().toLongLong();

        const QDomNodeList& probesElems = siteElem.elementsByTagName(QLatin1String("WatchedSiteProbe"));
        for (size_t j = 0; j < probesCount; j++)
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
    if (!iter->isDisabled)
    {
        qDebug() << "request web update for" << id << iter->title;
        QWebEnginePage* page = new QWebEnginePage(this);
        page->load(iter->url);
        connect(page, &QWebEnginePage::loadFinished, [this, id, page](bool ok){this->handlePageLoad(id, ok, page);});

        processed[id] = QDateTime::currentMSecsSinceEpoch();
    }
    QTimer::singleShot(iter->updateIntervalMs, this, [id, this](){this->updateSite(id);});
}

void WebWatcher::addNamedTextNode(QDomElement& elem, QDomDocument& doc, QString name, QString text)
{
    QDomElement tmp = doc.createElement(name);
    tmp.appendChild(doc.createTextNode(text));
    elem.appendChild(tmp);
}

void WebWatcher::reorder(QList<std::int64_t> ids)
{
    if (sites.size() == 0)
        return;

    size_t order_index = 0;
    for (int64_t id : ids)
    {
        for (size_t i = order_index; i < sites.size(); i++)
        {
            if (sites[i].id == id)
            {
                if (order_index != i)
                    swap(sites[order_index], sites[i]);
                order_index++;
                break;
            }
        }
    }
}

void WebWatcher::removeSiteProbe(std::int64_t site_id, std::int64_t probe_number)
{
    auto iter = std::find_if(sites.begin(), sites.end(), [site_id](const WatchedSite& site){return site.id == site_id;});
    if (iter != sites.end())
    {
        if (iter->probes.size() > probe_number)
            iter->probes.erase(iter->probes.begin() + probe_number);
    }
}
