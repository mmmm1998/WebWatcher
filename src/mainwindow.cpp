#include "mainwindow.h"

#include <cassert>

#include <QCloseEvent>
#include <QObject>
#include <QMenu>
#include <QInputDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QWebEnginePage>
#include <QDir>
#include <QListView>
#include <QMap>
#include <QSystemTrayIcon>
#include <QDomDocument>
#include <QDomElement>

#include <QDebug>

#include "timeunit.h"
#include "watcherinputdialog.h"
#include "notificator.h"

using namespace std;

MainWindow::MainWindow (QWidget *parent) : QMainWindow (parent)
    , tray(QIcon(QLatin1String(":/icons/watch.png")), parent)
{
    ui.setupUi(this);

    // Create tray and menu for the tray
    // Tray allows real close the application, because on close action the app just go to traybar
    connect(&tray, &QSystemTrayIcon::activated, this, &MainWindow::handleTrayActivation);
    QMenu* menu = new QMenu(this);
    menu->addAction(tr("Exit"), this, &QMainWindow::close);
    menu->addAction(tr("Show"), this, &QMainWindow::show);
    tray.setContextMenu(menu);
    tray.show();

    ui.subsView->setModel(&subsModel);
    ui.subsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    
    ui.requestHistoryView->setModel(&probesModel);
    ui.requestHistoryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    probesModel.setHorizontalHeaderLabels({QObject::tr("Time"), QObject::tr("Value")});
    ui.requestHistoryView->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    connect(&watcher, &WebWatcher::siteAcessed, this, &MainWindow::handleSiteAcessed);
    connect(&watcher, &WebWatcher::siteChanged, this, &MainWindow::handleSiteChanged);

    const QStringList& units = ReadableDuration::supportedUnits();
    for (const QString& unit: units)
        ui.intervalUnitsCombobox->addItem(unit);

    load();
}

void MainWindow::handleTrayActivation(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        if (isVisible())
            hide();
        else
            show();
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (isHidden())
    {
        event->accept();
        save();
    }
    else
    {
        tray.show();
        hide();
        event->ignore();
    }
}

void MainWindow::handleAddSiteButton()
{
    WatcherInputDialog dialog;

    if (dialog.exec())
    {
        QUrl url(dialog.site());
        if (url.isValid() && (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")))
        {
            addSite(url, dialog.jsQuery(), dialog.updateInterval());
        }
        else
            QMessageBox::warning(this, tr("WebWatcher"), tr("%1 url is invalid, only web sites (http, https) are supported").arg(url.toString()));
    }
}

void MainWindow::handleSubsDoubleClick(const QModelIndex& index)
{
    QStandardItem *item = subsModel.itemFromIndex(index);

    int64_t id = item->data(ID).toLongLong();
    optional<WatchedSite> site = watcher.siteById(id);
    const QUrl& url = site->url;

    QFont font = item->font();

    if (font.bold() == true)
        decreaseChangeCount();

    font.setBold(false);
    item->setFont(font);

    QDesktopServices::openUrl(url);
}

void MainWindow::handleRemoveSiteButton()
{
    QModelIndex index = ui.subsView->currentIndex();
    if (index.isValid())
    {
        QStandardItem *item = subsModel.itemFromIndex(index);

        QFont font = item->font();
        qDebug() << "font" << font.bold();
        if (font.bold() == true)
            decreaseChangeCount();

        int64_t id = item->data(ID).toLongLong();
        watcher.removeSite(id);

        subsModel.removeRow(index.row());

        // Clear field for updating watched site information - because we have removed selected element just now
        ui.siteEdit->setText(QString());
        ui.queryEdit->setText(QString());
        ui.intervalUnitsCombobox->setCurrentIndex(0);
        ui.intervalEdit->setText(QString());
    }
}

void MainWindow::addSite(const QUrl& url, QString xmlQuery, int64_t updateIntervalMs)
{
    int id = watcher.addSite(url, xmlQuery, updateIntervalMs);

    QStandardItem* entry = new QStandardItem(url.toString());
    entry->setData(id);

    subsModel.appendRow(entry);
}

void MainWindow::handleSiteChanged(int64_t id)
{
    qDebug() << "handleSiteChanged" << id;
    for (int i = 0; i < subsModel.rowCount(); i++)
    {
        QStandardItem* item = subsModel.item(i);
        if (item && item->data(ID).toLongLong() == id)
        {
            optional<WatchedSite> site = watcher.siteById(id);
            if (site)
            {
                item->setText(site->title);
                qDebug() << "hande proble change from handel site" << site->probes.size();
                updateProbesModel(*site);


                QFont font = item->font();

                bool isNewChange = font.bold() == false;
                font.setBold(true);
                item->setFont(font);

                if (isNewChange)
                    increaseChangeCount();

                if (notify)
                    notify->deleteLater();

                /*
                notify = new Notificator(*site);
                notify->show();
                */
            }
            return;
        }
    }

}

void MainWindow::handleSiteAcessed(std::int64_t id)
{
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
        updateProbesModel(*site);
}

void MainWindow::save()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(path + QLatin1String("/webwatcher.xml"));
    if (file.open(QIODevice::WriteOnly))
    {
        QDomDocument doc;
        const QDomElement& watcherElem = watcher.toXml(doc);
        doc.appendChild(watcherElem);
        file.write(doc.toByteArray(4)); // xml indent == 4
        file.close();
    }
}

void MainWindow::load()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(path + QLatin1String("/webwatcher.xml"));

    if (file.open(QIODevice::ReadOnly))
    {
        QDomDocument doc;
        doc.setContent(file.readAll());
        watcher.fromXml(doc.firstChildElement(QLatin1String("WebWatcher")));
        file.close();

        QList<int64_t> ids = watcher.ids();
        qDebug() << file.fileName() << ids;
        for (int64_t id : ids)
        {
            optional<WatchedSite> site = watcher.siteById(id);
            QStandardItem* entry = new QStandardItem((site->title.isEmpty() ? site->url.toString() : site->title));
            entry->setData((qint64)id, ID);

            subsModel.appendRow(entry);
        }

    }
}

void MainWindow::handleSubsClick(const QModelIndex& index)
{
    QStandardItem *item = subsModel.itemFromIndex(index);

    int64_t id = item->data(ID).toLongLong();
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        ui.siteEdit->setText(site->url.toString());
        ui.queryEdit->setText(site->jsQuery);

        pair<int64_t, ReadableDuration::TimeUnit> format = ReadableDuration::toHumanReadable(site->updateIntervalMs);
        ui.intervalUnitsCombobox->setCurrentText(ReadableDuration::unitName(format.second));
        ui.intervalEdit->setText(QString::number(format.first));
        
        updateProbesModel(*site);
    }
}

void MainWindow::handleSubsEdit()
{
    QStandardItem *item = subsModel.itemFromIndex(ui.subsView->currentIndex());
    
    // Exit, if no model selection
    if (item == nullptr)
        return;

    int64_t id = item->data(ID).toLongLong();
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        const QUrl& url = QUrl(ui.siteEdit->text());
        const QString& jsQuery = ui.queryEdit->text();

        const QString& unitName = ui.intervalUnitsCombobox->currentText();
        ReadableDuration::TimeUnit timeType = ReadableDuration::unitType(unitName);
        const int64_t updateIntervalMs =  ReadableDuration::toMs(ui.intervalEdit->text().toLongLong(), timeType);

        watcher.setSite(id, url, jsQuery, updateIntervalMs);
    }
}

void MainWindow::updateProbesModel(const WatchedSite& site)
{
    probesModel.removeRows(0, probesModel.rowCount());
    for (const WatchedSiteProbe& probe: site.probes)
    {
        QString time = QDateTime::fromMSecsSinceEpoch(probe.accessTime).toString();
        probesModel.appendRow({new QStandardItem(time), new QStandardItem(probe.text)});
    }
}

void MainWindow::decreaseChangeCount()
{
    changesCount--;

    if (changesCount == 0)
        tray.setIcon(QIcon(QLatin1String(":/icons/watch.png")));
}

void MainWindow::increaseChangeCount()
{
    changesCount++;

    // No need set icon each time, when changeCount was increased
    // because we can do it only when it is really needed
    if (changesCount == 1)
        tray.setIcon(QIcon(QLatin1String(":/icons/updated.png")));
}
