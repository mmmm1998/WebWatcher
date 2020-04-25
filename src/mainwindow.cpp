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
    ui.subsView->setMovement(QListView::Free);
    ui.subsView->setDragEnabled(true);
    ui.subsView->setDefaultDropAction(Qt::MoveAction);
    ui.subsView->setDragDropMode(QAbstractItemView::InternalMove);
    ui.subsView->setDropIndicatorShown(true);
    ui.subsView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    connect(ui.subsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::handleSubsClick);

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
        save();
        event->accept();
    }
    else
    {
        tray.show();
        hide();
        event->ignore();
        save();
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
    if (!ui.subsView->selectionModel()->hasSelection())
        return;

    QModelIndexList selection;
    while (!(selection = ui.subsView->selectionModel()->selectedIndexes()).isEmpty())
    {
        QModelIndex index = selection[0];
        QStandardItem *item = subsModel.itemFromIndex(index);

        bool updated = item->data(ST_Updated).toBool();
        if (updated)
            decreaseChangeCount();

        int64_t id = item->data(ID).toLongLong();
        watcher.removeSite(id);

        subsModel.removeRow(index.row());
    }

    // Clear field for updating watched site information - because we have removed selected element(s) just now
    ui.siteEdit->setText(QString());
    ui.queryEdit->setText(QString());
    ui.titleEdit->setText(QString());
    ui.intervalUnitsCombobox->setCurrentIndex(0);
    ui.intervalEdit->setText(QString());
    probesModel.removeRows(0, probesModel.rowCount());
}

void MainWindow::handleIgnoreSiteUpdateButton()
{
    if (!ui.subsView->selectionModel()->hasSelection())
        return;

    QModelIndexList selection = ui.subsView->selectionModel()->selectedIndexes();
    for (QModelIndex index : selection)
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
    if (!ui.subsView->selectionModel()->hasSelection())
        return;

    QModelIndexList selection = ui.subsView->selectionModel()->selectedIndexes();
    for (QModelIndex index : selection)
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
    if (!ui.subsView->selectionModel()->hasSelection())
        return;

    QModelIndexList selection = ui.subsView->selectionModel()->selectedIndexes();
    for (QModelIndex index : selection)
    {
        QStandardItem *item = subsModel.itemFromIndex(index);

        int64_t id = item->data(ID).toLongLong();
        optional<WatchedSite> site = watcher.siteById(id);
        if (site)
        {
            //change disabled value
            site->isDisabled = !site->isDisabled;
            watcher.updateSite(*site);

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
    entry->setDropEnabled(false); // Disable overwrite items by another items


    subsModel.appendRow(entry);
}

void MainWindow::handleSiteChanged(int64_t id)
{
    qDebug() << "handleSiteChanged" << id;
    QStandardItem* siteItem = findItemById(id);
    if (siteItem)
    {
        optional<WatchedSite> site = watcher.siteById(id);
        if (site)
        {
            siteItem->setText(itemName(*site));
            qDebug() << "handle probe change from the site" << site->probes.size();

            if (!siteItem->data(ST_Ignorable).toBool())
            {
                setUpdated(siteItem, true);
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

        QStandardItem* siteItem = findItemById(id);
        if (siteItem)
            siteItem->setText(itemName(*site));
    }
}

void MainWindow::save()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    const QString& swapFilename = path + QLatin1String("/webwatcher.xml.swap");
    const QString& saveFilename = path + QLatin1String("/webwatcher.xml");
    const QString& previousSaveFilename = path + QLatin1String("/webwatcher.xml.old");

    QFile file(swapFilename);
    if (file.open(QIODevice::WriteOnly))
    {
        QDomDocument doc;
        doc.appendChild(toXml(doc));
        file.write(doc.toByteArray(4)); // so, xml indent == 4
        file.close();
    }

    // remove old, move save file to old, move swap to save
    QFile::remove(previousSaveFilename);
    QFile::rename(saveFilename, previousSaveFilename);
    QFile::rename(swapFilename, saveFilename);
}

void MainWindow::load()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString& saveFilename = path + QLatin1String("/webwatcher.xml");
    const QString& previousSaveFilename = path + QLatin1String("/webwatcher.xml.old");

    bool usedOldFile = false;
    QFile file(saveFilename);

    if (!QFile::exists(saveFilename))
        if (QFile::exists(previousSaveFilename))
        {
            usedOldFile = true;
            file.setFileName(previousSaveFilename);
        }

    if (file.open(QIODevice::ReadOnly))
    {
        QDomDocument doc;
        doc.setContent(file.readAll());
        file.close();

        fromXml(doc.firstChildElement(QLatin1String("WebWatcherApplication")));

        if (usedOldFile)
            QMessageBox::warning(this, tr("Warning - Savefile"), tr("Actual save file haven't found, but program content was restored from previous save file. Some changes can be lost, sorry for that."));

        qDebug() << usedOldFile << file.fileName() << watcher.ids().size();
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
        site->url = QUrl(ui.siteEdit->text());
        if (site->isManualTitle)
            site->isManualTitle = !ui.titleEdit->text().isEmpty();
        else
            site->isManualTitle = ui.titleEdit->text() != site->title;
        site->title = ui.titleEdit->text();
        site->jsQuery = ui.queryEdit->text();

        const QString& unitName = ui.intervalUnitsCombobox->currentText();
        ReadableDuration::TimeUnit timeType = ReadableDuration::unitType(unitName);
        assert(timeType != ReadableDuration::TimeUnit::Unknown);
        site->updateIntervalMs = ReadableDuration::toMs(ui.intervalEdit->text().toLongLong(), timeType);

        watcher.updateSite(*site);

        item->setText(itemName(*site));
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


    QList<std::int64_t> ids;
    for (int i = 0; i < subsModel.rowCount(); i++)
    {
        QStandardItem* item = subsModel.item(i);
        if (item)
            ids.append(item->data(ID).toLongLong());
    }

    watcher.reorder(ids);

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
        entry->setDropEnabled(false); // Disable overwrite items by another items

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

QStandardItem * MainWindow::findItemById(int64_t id)
{
    for (int i = 0; i < subsModel.rowCount(); i++)
    {
        QStandardItem* item = subsModel.item(i);
        if (item && item->data(ID).toLongLong() == id)
            return item;
    }
    return nullptr;
}
