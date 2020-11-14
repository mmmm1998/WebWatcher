#ifndef WEBWATCHER_H
#define WEBWATCHER_H

#include <vector>
#include <optional>

#include <QObject>
#include <QUrl>
#include <QIcon>
#include <QMap>
#include <QSet>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QDomDocument>
#include <QDomElement>
#include <QThread>

#include "cefenginewrapper.hpp"

class QWebEnginePage;

/// Result of acessing to the certain watched site
struct WatchedSiteProbe
{
    /// Time of the acess in ms from epoch, see @c QDateTime::currentMSecsSinceEpoch
    std::int64_t accessTime;
    /// Result of JavaScript query for the watched site
    QString text;
    /// Hash of the @c text
    QByteArray hash;
};

/// Description of watched site
struct WatchedSiteDescription
{
    /// URL of the watched site. For example "https://google.com"
    QUrl url;
    /// If is it true, then @c WebWatcher won't watch for this target (but will store previous results)
    bool isDisabled;
    /// JavaScript code, wich will perform on loaded page by @c url
    QString jsQuery;
    /// Minimal time of wating between site acessing in ms.
    /// @note actual time between can be more, that this, because the aplication, which use @c WebWatcher class need to work, when the time of next update will come.
    std::int64_t updateIntervalMs;
};

// This structure contains description of watching process and all collected data during observation
struct WatchedSite
{
    // ID for watching site. Unique for all stored WatchedSite of certain class instance.
    std::int64_t id;
    /// Description of site watching parameter
    WatchedSiteDescription info;

    /// Data loaded during site observation
    QString page_title;
    std::vector<WatchedSiteProbe> probes;
};

/**
 * @class WebWatcher
 *
 * This class designed for watching a multiple sites. "Watching" means, that the class will loads
 * site (@c WatchedSiteDescription::url) with with a given periodicity (@c WatchedSiteDescription::updateIntervalMs)
 * and run certain JavaScript code (@c WatchedSiteDescription::jsQuery) for loaded page. Result of this JavaScript will
 * be saved as string with timestamp of the site access time (but if result of this access is equal to result of the
 * previous access, then the class just update timestamp of previous access). And all this data (not only last access
 * result, but each access for the site) are stored and acessable for class consumers.
 * Class also supports enabling/disabling of the active watching for site (@c WatchedSiteDescription::isDisabled), serialization,
 * manipulation with loaded data and qt signal notifications
 */
class WebWatcher: public QObject
{
  Q_OBJECT
  public:
    // Сreates a class instance, which is not watching over anyone
    explicit WebWatcher();
    WebWatcher(const WebWatcher& other);
    WebWatcher(WebWatcher&& other);
    ~WebWatcher();

    /**
     * Add new target for watching
     * @param siteDescription Description of the site, which will be watched
     * @return Id of the watching entry, which will used in other class methods or @c -1, if @p site data is invalid (eg invalid url)
     */
    std::int64_t addSite(const WatchedSiteDescription& siteDescription);

    /**
     * Remove site from watching with all data loaded during observation
     * @param id ID of the removed watching entry (should be valid or nothing happens)
     */
    void removeSite(std::int64_t id);

    /**
     * Request full data (watched site description and loaded data) for certain watched entry.
     * @param id ID of the requested watching entry (should be valid or nothing happens)
     * @return @c std::optional, which contains data for valid (existed) ID and std::nullopt if the ID is invalid
     */
    std::optional<WatchedSite> siteById(std::int64_t id);

    /**
     * Updates parameters of certain watched entry.
     * @param id ID of the watched entry, which will be changed (should be valid or nothing happens)
     * @param siteDescription New description of the watched entry, which will replace old data
     * @param allowResetLoadedData Optional parameter (@c true by default). If it is @c false, then already loaded won't be deleted, else all loaded data will be cleaned (in certain cases)
     * @note When @p allowResetLoadedData is @c true the loaded data will be automatically reseted, but only if url or JavaScript query of new description (@p siteDescription) is differ from previous stored url or query (i.e. inverval update won't reseted the loaded data)
     */
    bool updateSite(std::int64_t id, const WatchedSiteDescription& siteDescription, bool allowResetLoadedData = true);

    /**
     * This method allow to remove sertain piece of the loaded data for certain watched entry.
     * Usefull for situation, when the query result is invalid or wrong due some user error in JavaScript code
     * @param site_id ID of the watched entry, which data will be changed (should be valid or nothing happens)
     * @param probe_number Sequence index in @c WatchedSite::probes of the watched entry (the numbers starts from 0)
     */
    void removeSiteProbe(std::int64_t site_id, size_t probe_number);

    /**
     * List of watched entries
     * @return list with ID of all stored watched entry (so regardless of @c WatchedSiteDescription::isDisabled)
     */
    QList<std::int64_t> ids();

    /**
     * Serialize class instance to XML form
     * @return XML representation of the instance
     */
    QDomElement toXml(QDomDocument& doc);

    /**
     * Set all inner data from xml form (deserialization)
     * @note handling of the deserialization errors don't added. Any XML structure error will cause assert fail
     */
    void fromXml(const QDomElement& content);

  signals:
    /// Emit, when previous request for watched entry wit @p id haven't finished until new request time. Time from previous request also added (@p requestOutdateMs)
    void requestOutdated(std::int64_t, std::int64_t requestOutdateMs);
    /// Emit, then page loading for watched entry with @p id have failed. The failed address also attached (@p url)
    void failToLoadPage(std::int64_t id, QUrl url);
    /// Emit on each access of watched entry with this @p id
    void siteAcessed(std::int64_t id);
    /// Emit only, if result of query for this access is different from result of the previous access of watched entry with this @p id
    void siteChanged(std::int64_t id);
    /// Emit only, then running jq query for watched site with this @p id will finished with Javascript exception with text @p exceptionText
    void exceptionOccurred(std::int64_t id, QString exceptionText);
    /// Emit only, then running jq query via qt web engine for watched site with this @p id will finished empty string\
    (qt don't report exceptiona and errors, but return empty string as javascript result)
    void possibleQtExceptionOccurred(std::int64_t id);

  /* ==================================================================================================================
   * =                                           Private class members                                                   =
   * ==================================================================================================================
   */
  private slots:
    void handlePageLoad(std::int64_t id, bool success, QWebEnginePage* page);
    void handleJsCallbackFromQtWebEngine(std::int64_t id, QVariant callbackResult, QWebEnginePage* page);
    void updateSite(std::int64_t id);
    void handleJsCallbackFromChromeEngine(std::int64_t id, const QString& scriptResult, bool haveException, const QString& pageTitle);

  private:
    void handleJsCallback(std::int64_t id, const QString& callbackResult, const QString& pageTitle, bool haveException, QString exceptionText);
    void doSiteUpdate(std::int64_t id, const WatchedSiteDescription& info, const QString& page_title);
    void addNamedTextNode(QDomElement& elem, QDomDocument& doc, QString name, QString text);

  private:
    ChromiumWebEngineWrapper chromeEngine;
    std::vector<WatchedSite> sites;
    QMap<int, std::int64_t> processed;
    QSet<std::int64_t> needCloudflareBypass;
    std::int64_t id_count{0};

    static QString kCloudflareProtectedPageTitle;
    static QString kCloudflareProtectedPageTextMark1;
    static QString kCloudflareProtectedPageTextMark2;
    static QString kTrue;
    static QString kFalse;
};

#endif // WEBWATCHER_H
