#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QStandardItemModel>
#include <QRect>

#include "webwatcher.h"

#include "ui_mainwindow.h"


class QCloseEvent;
class QStandardItem;

class MainWindow : public QMainWindow
{
  Q_OBJECT
  public:
    MainWindow(QWidget *parent = nullptr);

  public slots:
    void handleTrayActivation(QSystemTrayIcon::ActivationReason reason);
    void handleAddSiteButton();
    void handleRemoveSiteButton();
    void handleIgnoreSiteUpdateButton();
    void handleToggleSiteIgnorableButton();
    void handleOnOffUpdateButton();
    void handleSubsClick(const QModelIndex &index);
    void handleSubsDoubleClick(const QModelIndex &index);
    void handleSubsEdit();
    void handleSiteChanged(std::int64_t id);
    void handleSiteAcessed(std::int64_t id);

    void save();
    void load();

  private slots:
    void closeEvent(QCloseEvent *event);

  private:
    void addSite(const QUrl& url, QString title, QString xmlQuery, std::int64_t updateIntervalMs);
    void updateProbesModel(const WatchedSite& site);
    void increaseChangeCount();
    void decreaseChangeCount();
    QDomElement toXml(QDomDocument& doc);
    void fromXml(const QDomElement& content);
    void setUpdated(QStandardItem* item, bool value);
    void setIgnorable(QStandardItem* item, bool value);
    void setDisabled(QStandardItem* item, bool value);

  private:
    static QString itemName(const WatchedSite& site);

  private:
    Ui::MainWindow ui;
    QSystemTrayIcon tray;
    QStandardItemModel subsModel;
    QStandardItemModel probesModel;
    WebWatcher watcher;
    int changesCount{0};

  private:
    static const int ID = Qt::UserRole + 1;
    static const int ST_Updated = Qt::UserRole + 2;
    static const int ST_Ignorable = Qt::UserRole + 3;
};

#endif // MAINWINDOW_H
