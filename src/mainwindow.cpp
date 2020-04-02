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
    ui.requestHistoryView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui.requestHistoryView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);

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
            addSite(url, dialog.title(), dialog.jsQuery(), dialog.updateInterval());
        }
        else
            QMessageBox::warning(this, tr("WebWatcher"), tr("%1 url is invalid, only web sites (http, https) are supported").arg(url.toString()));
    }
}

void MainWindow::handleSubsDoubleClick(const QModelIndex& index)
{
    QStandardItem *item = subsModel.itemFromIndex(index);

    // Check, if marked as changed, and disable marking, if it is true
    bool updated = item->data(ST_Updated).toBool();
    if (updated)
    {
        setUpdated(item, false);
    }

    int64_t id = item->data(ID).toLongLong();
    optional<WatchedSite> site = watcher.siteById(id);
    const QUrl& url = site->url;
    QDesktopServices::openUrl(url);
}

void MainWindow::handleRemoveSiteButton()
{
    QModelIndex index = ui.subsView->currentIndex();
    if (index.isValid())
    {
        QStandardItem *item = subsModel.itemFromIndex(index);

        bool updated = item->data(ST_Updated).toBool();
        if (updated)
            decreaseChangeCount();

        int64_t id = item->data(ID).toLongLong();
        watcher.removeSite(id);

        subsModel.removeRow(index.row());

        // Clear field for updating watched site information - because we have removed selected element just now
        ui.siteEdit->setText(QString());
        ui.queryEdit->setText(QString());
        ui.titleEdit->setText(QString());
        ui.intervalUnitsCombobox->setCurrentIndex(0);
        ui.intervalEdit->setText(QString());
        probesModel.removeRows(0, probesModel.rowCount());

        ui.subsView->clearSelection();
    }
}

void MainWindow::handleIgnoreSiteUpdateButton()
{
    QModelIndex index = ui.subsView->currentIndex();
    if (index.isValid())
    {
        QStandardItem *item = subsModel.itemFromIndex(index);

        bool updated = item->data(ST_Updated).toBool();
        if (updated)
        {
            setUpdated(item, false);
        }
    }
}

void MainWindow::handleToggleSiteIgnorableButton()
{
    QModelIndex index = ui.subsView->currentIndex();
    if (index.isValid())
    {
        QStandardItem *item = subsModel.itemFromIndex(index);

        bool ignorable = !item->data(ST_Ignorable).toBool();
        setIgnorable(item, ignorable);

        if (ignorable)
        {
            bool updated = item->data(ST_Updated).toBool();
            if (updated)
            {
                setUpdated(item, false);
            }
        }
    }
}

void MainWindow::handleOnOffUpdateButton()
{
    QModelIndex index = ui.subsView->currentIndex();
    if (index.isValid())
    {
        QStandardItem *item = subsModel.itemFromIndex(index);

        int64_t id = item->data(ID).toLongLong();
        optional<WatchedSite> site = watcher.siteById(id);
        if (site)
        {
            //change disabled value
            site->isDisabled = !site->isDisabled;
            watcher.updateSite(*site);

            qDebug() << "site->isDisabled" << site->isDisabled;

            setDisabled(item, site->isDisabled);

            if (site->isDisabled)
            {
                bool updated = item->data(ST_Updated).toBool();
                if (updated)
                {
                    setUpdated(item, false);
                }
            }
        }
    }
}

void MainWindow::addSite(const QUrl& url, QString title, QString xmlQuery, int64_t updateIntervalMs)
{
    int64_t id = watcher.addSite(url, title, xmlQuery, updateIntervalMs);

    QStandardItem* entry = new QStandardItem(url.toString());
    entry->setData(QVariant((qlonglong)id), ID);
    setUpdated(entry, false);
    setIgnorable(entry, false);


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
                item->setText(itemName(*site));
                qDebug() << "hande proble change from handel site" << site->probes.size();

                if (!item->data(ST_Ignorable).toBool())
                {
                    setUpdated(item, true);
                }

                QStandardItem *selectedItem = subsModel.itemFromIndex(ui.subsView->currentIndex());
                if (selectedItem)
                {
                    int64_t view_id = selectedItem->data(ID).toLongLong();
                    if (id == view_id)
                    {
                        updateProbesModel(*site);

                        // handle situation, when
                        // 1. we change url of the entry
                        // 2. we see edit ui element for the entry in this moment
                        // 3. our title isn't manual and changed with the url
                        if (!site->isManualTitle)
                            ui.titleEdit->setText(site->title);
                    }
                }
            }
            return;
        }
    }

}

void MainWindow::handleSiteAcessed(std::int64_t id)
{
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        QStandardItem *selectedItem = subsModel.itemFromIndex(ui.subsView->currentIndex());
        if (selectedItem)
        {
            int64_t view_id = selectedItem->data(ID).toLongLong();
            if (id == view_id)
                updateProbesModel(*site);
        }
    }
}

void MainWindow::save()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(path + QLatin1String("/webwatcher.xml"));
    if (file.open(QIODevice::WriteOnly))
    {
        QDomDocument doc;
        doc.appendChild(toXml(doc));
        file.write(doc.toByteArray(4)); // so, xml indent == 4
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
        file.close();

        fromXml(doc.firstChildElement(QLatin1String("WebWatcherApplication")));
        qDebug() << file.fileName() << watcher.ids().size();
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
        ui.titleEdit->setText(site->title);

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
        bool isManualTitle = ui.titleEdit->text() != site->title;
        const QString& title = ui.titleEdit->text();

        const QString& unitName = ui.intervalUnitsCombobox->currentText();
        ReadableDuration::TimeUnit timeType = ReadableDuration::unitType(unitName);
        const int64_t updateIntervalMs =  ReadableDuration::toMs(ui.intervalEdit->text().toLongLong(), timeType);

        bool isDisabled = site->isDisabled;

        watcher.setSite(id, url, title, isManualTitle, isDisabled, jsQuery, updateIntervalMs);

        item->setText(title);
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
    ui.requestHistoryView->resizeColumnsToContents();
    ui.requestHistoryView->resizeRowsToContents();
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

QDomElement MainWindow::toXml(QDomDocument& doc)
{
    QDomElement root = doc.createElement(QLatin1String("WebWatcherApplication"));

    root.setAttribute(QLatin1String("changesCount"), changesCount);

    for (int i = 0; i < subsModel.rowCount(); i++)
    {
        QStandardItem* item = subsModel.item(i);
        if (item && item->data(ST_Updated).toBool()) // Item marked as changed
        {
            QDomElement idEl = doc.createElement(QLatin1String("WithChanges"));
            idEl.appendChild(doc.createTextNode(QString::number(item->data(ID).toLongLong())));
            root.appendChild(idEl);
        }
        if (item && item->data(ST_Ignorable).toBool())
        {
            QDomElement idEl = doc.createElement(QLatin1String("Ignorable"));
            idEl.appendChild(doc.createTextNode(QString::number(item->data(ID).toLongLong())));
            root.appendChild(idEl);
        }

    }

    root.appendChild(watcher.toXml(doc));

    return root;
}

void MainWindow::fromXml(const QDomElement& content)
{
    assert(content.tagName() == QLatin1String("WebWatcherApplication"));

    assert(content.attributes().contains(QLatin1String("changesCount")));
    changesCount = content.attribute(QLatin1String("changesCount")).toInt();
    if (changesCount > 0)
        tray.setIcon(QIcon(QLatin1String(":/icons/updated.png")));

    assert(content.firstChildElement(QLatin1String("WebWatcher")).isNull() == false);
    watcher.fromXml(content.firstChildElement(QLatin1String("WebWatcher")));

    QList<int64_t> ids = watcher.ids();
    for (int64_t id : ids)
    {
        optional<WatchedSite> site = watcher.siteById(id);
        QStandardItem* entry = new QStandardItem((site->title.isEmpty() ? site->url.toString() : site->title));
        entry->setData((qint64)id, ID);
        setUpdated(entry, false);
        setIgnorable(entry, false);
        setDisabled(entry, site->isDisabled);

        subsModel.appendRow(entry);
    }



    for (auto [tagname, actor]: {tuple{"WithChanges", &MainWindow::setUpdated}, tuple{"Ignorable", &MainWindow::setIgnorable}})
    {
        const QDomNodeList& idsElems = content.elementsByTagName(QLatin1String(tagname));
        for (size_t i = 0; i < idsElems.length(); i++)
        {
            const QDomElement& siteElem = idsElems.item(i).toElement();
            int64_t saveId = siteElem.text().toLongLong();
            for (int i = 0; i < subsModel.rowCount(); i++)
            {
                QStandardItem* item = subsModel.item(i);
                if (item && item->data(ID).toLongLong() == saveId)
                {
                    (this->*actor)(item, true);
                }
            }
        }
    }
}

void MainWindow::setUpdated(QStandardItem* item, bool value)
{
    if (value && item->data(ST_Updated).toBool() == false)
        increaseChangeCount();
    else if (value == false && item->data(ST_Updated).toBool())
        decreaseChangeCount();

    item->setData(value, ST_Updated);

    // Visual information about "Updated" state
    QFont font = item->font();
    font.setBold(value);
    item->setFont(font);
}

void MainWindow::setIgnorable(QStandardItem* item, bool value)
{
    item->setData(value, ST_Ignorable);

    // Visual information about "Ignorable" state
    QFont font = item->font();
    font.setItalic(value);
    item->setFont(font);
}

void MainWindow::setDisabled(QStandardItem* item, bool value)
{
    // Visual information about "Disabled" state
    QBrush back = item->background();
    if (value)
    {
        back.setColor(Qt::lightGray);
        back.setStyle(Qt::SolidPattern);
    }
    else
    {
        back.setColor(Qt::white);
        back.setStyle(Qt::NoBrush);
    }
    item->setBackground(back);
}

QString MainWindow::itemName(const WatchedSite& site)
{
    QString name = !site.title.isEmpty() ? site.title : site.url.toString();
    return name;
}
