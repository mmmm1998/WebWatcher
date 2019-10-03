#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QStandardItemModel>
#include <QRect>

#include "webwatcher.h"

#include "ui_mainwindow.h"


class QCloseEvent;
class Notificator;

class MainWindow : public QMainWindow
{
  Q_OBJECT
  public:
    MainWindow(QWidget *parent = nullptr);

  public slots:
    void handleTrayActivation(QSystemTrayIcon::ActivationReason reason);
    void handleAddSiteButton();
    void handleRemoveSiteButton();
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
    void addSite(const QUrl& url, QString xmlQuery = QString(), std::int64_t updateIntervalMs = 10000);
    void updateProbesModel(const WatchedSite& site);
    void increaseChangeCount();
    void decreaseChangeCount();
    QDomElement toXml(QDomDocument& doc);
    void fromXml(const QDomElement& content);

  private:
    Ui::MainWindow ui;
    QSystemTrayIcon tray;
    QStandardItemModel subsModel;
    QStandardItemModel probesModel;
    WebWatcher watcher;
    int changesCount{0};
    Notificator* notify{nullptr};

  private:
    static const int ID = Qt::UserRole + 1;
};

#endif // MAINWINDOW_H
