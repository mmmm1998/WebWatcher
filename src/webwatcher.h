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

class QWebEnginePage;

struct WatchedSiteProbe
{
    std::int64_t accessTime;
    QString text;
    QByteArray hash;
};

struct WatchedSite
{
    std::int64_t id;
    QUrl url;
    QString title;
    QString jsQuery;
    std::int64_t updateIntervalMs;
    std::vector<WatchedSiteProbe> probes;
};

class WebWatcher: public QObject
{
  Q_OBJECT
  public:
    explicit WebWatcher() = default;

    std::int64_t addSite(QUrl url, QString jsQuery, std::int64_t updateIntervalMs);
    bool setSite(std::int64_t id, QUrl url, QString jsQuery, std::int64_t updateIntervalMs);
    void removeSite(std::int64_t id);
    std::optional<WatchedSite> siteById(std::int64_t id);
    QList<std::int64_t> ids();

    void save(QIODevice* device);
    void load(QIODevice* device);

  signals:
    void siteChanged(std::int64_t id);

  public slots:
    void updateSite(std::int64_t id);

  // private ====================================
  private slots:
    void handlePageLoad(std::int64_t id, bool success, QWebEnginePage* page);
    void handleJsCallback(std::int64_t id, QVariant callbackResult, QWebEnginePage* page);

  private:
    void doSiteUpdate(std::vector<WatchedSite>::iterator iter);

  private:
    std::vector<WatchedSite> sites;
    QMap<int, std::int64_t> processed;
    std::int64_t id_count{0};
};

#endif // WEBWATCHER_H