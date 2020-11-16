#include "mainwindow.hpp"

#include <cassert>
#include <optional>

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
#include <QFile>
#include <QDesktopServices>
#include <QApplication>

#include <QDebug>

#include "logging.hpp"
#include "timeunit.hpp"
#include "watcherinputdialog.hpp"
#include "applicationlanguage.hpp"

const int MainWindow::EXIT_CODE_REBOOT = 2020;

using namespace std;

struct WatchedEntryInfo
{
    int64_t id;
    bool withChanges;
    bool isIgnorable;
    bool isUnresettable;
    QString manualTitle;

    static const QString classXmlTagName;

    QDomElement toXml(QDomDocument& doc)
    {
        QDomElement root = doc.createElement(classXmlTagName);

        QDomElement idEl = doc.createElement(QLatin1String("ID"));
        idEl.appendChild(doc.createTextNode(QString::number(id)));
        root.appendChild(idEl);

        QDomElement titleEl = doc.createElement(QLatin1String("ManualTitle"));
        titleEl.appendChild(doc.createTextNode(manualTitle));
        root.appendChild(titleEl);

        if (withChanges)
            root.setAttribute(QLatin1String("with_changes"), QLatin1String("true"));
        if (isIgnorable)
            root.setAttribute(QLatin1String("ignorable"), QLatin1String("true"));
        if (isUnresettable)
            root.setAttribute(QLatin1String("unresettable"), QLatin1String("true"));

        return root;
    }

    void fromXml(const QDomElement& content)
    {
        assert(content.tagName() == classXmlTagName);

        id = content.firstChildElement(QLatin1String("ID")).text().toLongLong();
        manualTitle = content.firstChildElement(QLatin1String("ManualTitle")).text();
        withChanges = content.hasAttribute(QLatin1String("with_changes"));
        isIgnorable = content.hasAttribute(QLatin1String("ignorable"));
        isUnresettable = content.hasAttribute(QLatin1String("unresettable"));
    }
};
const QString WatchedEntryInfo::classXmlTagName = QLatin1String("WatchedEntryInfo");

const QString MainWindow::EDITOR_FILES_DIR = QString::fromLatin1("/webwatcher/");

MainWindow::MainWindow (WebWatcherApplSettings& settings, QWidget *parent) : QMainWindow (parent)
    , tray(new QSystemTrayIcon(QIcon(QLatin1String(":/icons/watch.png")), parent))
    , appSettings(settings)
{
    ui.setupUi(this);

    // Create tray and menu for the tray
    // Tray allows real close the application, because on close action the app just go to traybar
    connect(tray, &QSystemTrayIcon::activated, this, &MainWindow::handleTrayActivation);
    QMenu* menu = new QMenu(this);
    menu->addAction(tr("Exit"), this, &QMainWindow::close);
    menu->addAction(tr("Show"), this, &QMainWindow::show);
    tray->setContextMenu(menu);
    tray->show();

    ui.subsView->setModel(&subsModel);
    ui.subsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.subsView->setMovement(QListView::Free);
    ui.subsView->setDragEnabled(true);
    ui.subsView->setDefaultDropAction(Qt::MoveAction);
    ui.subsView->setDragDropMode(QAbstractItemView::InternalMove);
    ui.subsView->setDropIndicatorShown(true);
    ui.subsView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    initUiActions();

    connect(ui.subsView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::handleSubsClick);

    ui.requestHistoryView->setModel(&probesModel);
    ui.requestHistoryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    probesModel.setHorizontalHeaderLabels({QObject::tr("Time"), QObject::tr("Value")});
    ui.requestHistoryView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui.requestHistoryView->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui.requestHistoryView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui.requestHistoryView->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(&watcher, &WebWatcher::siteAcessed, this, &MainWindow::handleSiteAcessed);
    connect(&watcher, &WebWatcher::siteChanged, this, &MainWindow::handleSiteChanged);
    connect(&watcher, &WebWatcher::failToLoadPage, this, &MainWindow::handleFailToLoadPage);
    connect(&watcher, &WebWatcher::requestOutdated, this, &MainWindow::handleRequestOutdated);
    connect(&watcher, &WebWatcher::exceptionOccurred, this, &MainWindow::handleExceptionOccurred);
    connect(&watcher, &WebWatcher::possibleQtExceptionOccurred, this, &MainWindow::handlePossibleQtExceptionOccurred);

    const QStringList& units = ReadableDuration::supportedUnits();
    for (const QString& unit: units)
        ui.intervalUnitsCombobox->addItem(unit);

    const QString& filesDir = QDir::tempPath() + EDITOR_FILES_DIR;
    if (!QFile::exists(filesDir))
        QDir().mkpath(filesDir);
    load();
}

void MainWindow::initUiActions()
{
    QMenu* programMenu = ui.menuBar->addMenu(tr("Program"));
    QAction* programExitAction = new QAction(QIcon::fromTheme("application-exit"), tr("Exit"), ui.menuBar);
    programMenu->addAction(programExitAction);
    connect(programExitAction, &QAction::triggered, this, &MainWindow::closeProgram);

    QMenu* watchedSiteMenu = ui.menuBar->addMenu(tr("WatchedSite"));

    QAction* addEntryAction = new QAction(QIcon::fromTheme("list-add"), tr("Add"), ui.menuBar);
    ui.subsView->addAction(addEntryAction);
    watchedSiteMenu->addAction(addEntryAction);
    connect(addEntryAction, &QAction::triggered, this, &MainWindow::handleAddSiteButton);

    QAction* removeEntryAction = new QAction(QIcon::fromTheme("list-remove"), tr("Remove"), ui.menuBar);
    ui.subsView->addAction(removeEntryAction);
    watchedSiteMenu->addAction(removeEntryAction);
    connect(removeEntryAction, &QAction::triggered, this, &MainWindow::handleRemoveSiteButton);

    QAction* ignoreUpdateAction = new QAction(QIcon::fromTheme("mail-mark-read"), tr("Ignore Update"), ui.menuBar);
    ui.subsView->addAction(ignoreUpdateAction);
    watchedSiteMenu->addAction(ignoreUpdateAction);
    connect(ignoreUpdateAction, &QAction::triggered, this, &MainWindow::handleIgnoreSiteUpdateButton);

    QAction* toggleIgnoranceAction = new QAction(QIcon::fromTheme("mail-mark-junk"), tr("Toggle Ignoring"), ui.menuBar);
    ui.subsView->addAction(toggleIgnoranceAction);
    watchedSiteMenu->addAction(toggleIgnoranceAction);
    connect(toggleIgnoranceAction, &QAction::triggered, this, &MainWindow::handleToggleSiteIgnorableButton);

    QAction* toggleUpdatingAction = new QAction(QIcon::fromTheme("process-stop"), tr("Toggle Updating"), ui.menuBar);
    ui.subsView->addAction(toggleUpdatingAction);
    watchedSiteMenu->addAction(toggleUpdatingAction);
    connect(toggleUpdatingAction, &QAction::triggered, this, &MainWindow::handleOnOffUpdateButton);

    QMenu* settingsMenu = ui.menuBar->addMenu(tr("Settings"));

    QAction* changeLanguageAction = new QAction(QIcon::fromTheme("emblem-system"), tr("Change language"), ui.menuBar);
    settingsMenu->addAction(changeLanguageAction);
    connect(changeLanguageAction, &QAction::triggered, this, &MainWindow::handleLanguageSettings);

    ui.subsView->setContextMenuPolicy(Qt::ActionsContextMenu);
}


MainWindow::~MainWindow()
{
    const QString& filesDir = QDir::tempPath() + EDITOR_FILES_DIR;
    if (QFile::exists(filesDir))
        QDir().rmpath(filesDir);
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
        delete tray;
        event->accept();
    }
    else
    {
        tray->show();
        hide();
        event->ignore();
        save();
    }
}

void MainWindow::closeProgram()
{
    hide();
    close();
}

void MainWindow::restart()
{
    save();
    tray->hide();
    delete tray;
    qApp->exit(EXIT_CODE_REBOOT);
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

void MainWindow::handleLanguageSettings()
{
    LanguageDialog dialog(appSettings);

    if (dialog.exec())
    {
        const QLocale& previousLocale = appSettings.usedLanguage;
        appSettings.usedLanguage = dialog.choosenLanguage();

        if (LanguageDialog::isEqual(previousLocale, appSettings.usedLanguage))
        {
            QMessageBox::StandardButton retCode = QMessageBox::question(
                this, tr("WebWatcher"), tr("Changes will applied after the application restart. Restart now?")
                , QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
            );
            if (retCode == QMessageBox::Yes)
            {
                restart();
            }
        }
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
    const QUrl& url = site->info.url;
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
        Log::info("Remove watched site #%ld from UI and watching", id);
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
            WatchedSiteDescription info = site->info;
            info.isDisabled = !site->info.isDisabled;
            watcher.updateSite(id, info);

            setDisabled(item, info.isDisabled);

            if (info.isDisabled)
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

void MainWindow::handleToggleNotResetableCheckbox()
{
    QStandardItem *selectedItem = subsModel.itemFromIndex(ui.subsView->currentIndex());
    if (selectedItem)
    {
        bool isNotResetable = !ui.resetProbesCheckBox->isChecked();
        selectedItem->setData(isNotResetable, ST_NotResetable);
    }
}

void MainWindow::addSite(const QUrl& url, QString title, QString xmlQuery, int64_t updateIntervalMs)
{
    WatchedSiteDescription description;
    description.url = url;
    description.jsQuery = xmlQuery;
    description.updateIntervalMs = updateIntervalMs;

    int64_t id = watcher.addSite(description);
    assert(id != -1);
    Log::info("New watched site (#%ld) have added", id);

    QStandardItem* entry = new QStandardItem(title.isEmpty() ? url.toString(): title);
    entry->setData(QVariant((qlonglong)id), ID);
    setUpdated(entry, false);
    setIgnorable(entry, false);
    entry->setData(QVariant(title), ST_ManualTitle);
    entry->setDropEnabled(false); // Disable overwrite items by another items

    subsModel.appendRow(entry);
}

void MainWindow::handleSiteChanged(int64_t id)
{
    Log::info("Watched site #%ld have updated", id);
    QStandardItem* siteItem = findItemById(id);
    if (siteItem)
    {
        optional<WatchedSite> site = watcher.siteById(id);
        if (site)
        {
            const QString& storedManualTitle = siteItem->data(ST_ManualTitle).toString();
            siteItem->setText(itemName(*site, storedManualTitle));
            Log::info("Watched site #%ld now have %lu probes", id, site->probes.size());

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
                    if (storedManualTitle.isEmpty())
                        ui.titleEdit->setText(itemName(*site, storedManualTitle));
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
        {
            const QString& storedManualTitle = siteItem->data(ST_ManualTitle).toString();
            siteItem->setText(itemName(*site, storedManualTitle));
        }
    }
}

void MainWindow::handleFailToLoadPage(std::int64_t id, QUrl url)
{
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        QStandardItem* siteItem = findItemById(id);
        if (siteItem)
        {
            const QString& storedManualTitle = siteItem->data(ST_ManualTitle).toString();
            QMessageBox::StandardButton button = QMessageBox::warning(this, tr("Warning - Page Loading"),
                tr("WebWatcher have tried to load url \"%1\" for watched site \"%2\", but page loading have "\
                "failed. Check that the url is valid and not redirect to another url (which disabled in "\
                "the application for proper user experience). Do you want to retry the request?"
                ).arg(url.toDisplayString()).arg(itemName(*site, storedManualTitle)),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
            );
            if (button == QMessageBox::Yes)
            {
                watcher.updateNow(id);
            }
        }
    }
}

void MainWindow::handleRequestOutdated(std::int64_t id, std::int64_t requestOutdateMs)
{
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        QStandardItem* siteItem = findItemById(id);
        if (siteItem)
        {
            const QString& storedManualTitle = siteItem->data(ST_ManualTitle).toString();
            QMessageBox::critical(this, tr("Error - Request timeout"),
                tr("WebWatcher have tried to load url \"%1\" for watched site \"%2\", but page loading "\
                "haven't finished in proper time (before next update) and the loading tooks %3 seconds. "\
                "It may be possible some Internet problems or internal problem with Qt Browser, "\
                "which used for page loading. It is recommended to use another url for data collection"
                ).arg(site->info.url.toDisplayString()).arg(itemName(*site, storedManualTitle)).arg(requestOutdateMs/1000.0)
            );
        }
    }
}

void MainWindow::handleExceptionOccurred(std::int64_t id, QString exceptionText)
{
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        QStandardItem* siteItem = findItemById(id);
        if (siteItem)
        {
            const QString& storedManualTitle = siteItem->data(ST_ManualTitle).toString();
            QMessageBox::critical(this, tr("Error - JavaScript Exception"),
                tr("WebWatcher try to execute javascript query for watched site \"%1\", but the execution "\
                "have finished with javascript exception \"%2\". It is reccommended check the query script "\
                "and correct all error or add javascript exception handling"
                ).arg(itemName(*site, storedManualTitle)).arg(exceptionText)
            );
        }
    }
}

void MainWindow::handlePossibleQtExceptionOccurred(std::int64_t id)
{
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        QStandardItem* siteItem = findItemById(id);
        if (siteItem)
        {
            const QString& storedManualTitle = siteItem->data(ST_ManualTitle).toString();
            QMessageBox::warning(this, tr("Warning - Possible JavaScript Exception"),
                tr("WebWatcher have execute javascript query for watched site \"%1\", but the execution "\
                "have finished with empty string as result. And for Javascript execution have used Qt Web "\
                "engine and the engine returns empty string when javascript errors occurres.  And with this "\
                "in mind and the fact that returning empty strings is pretty uselessness, the program decided "\
                "to warn you about this situation. It is also recommended to change your js query for not "\
                "returning empty string in case of successful execution for preventing future false warnings. "\
                "And it is also recommended to check your javascript code in a browser console on errors"
                ).arg(itemName(*site, storedManualTitle))
            );
        }
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
    Log::info("Search for application data file (%s)", "webwatcher.xml");
    const QString& writableLocationDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString& saveFilename = writableLocationDir + QLatin1String("/webwatcher.xml");
    const QString& previousSaveFilename = writableLocationDir + QLatin1String("/webwatcher.xml.old");

    bool usedOldFile = false;
    bool saveFileFound = false;
    QFile file(saveFilename);

    if (QFile::exists(saveFilename))
    {
        saveFileFound = true;
        Log::info("Found application data file (%s)", saveFilename.toLocal8Bit().data());
    }
    else
    {
        Log::warn("Application data file not found, try to found previous data file (%s)", "webwatcher.xml.old");
        if (QFile::exists(previousSaveFilename))
        {
            Log::info("Previous application data file have found and will used");
            usedOldFile = true;
            file.setFileName(previousSaveFilename);
            saveFileFound = true;
        }
        else
            Log::info("Previous data file not found, so this run is considered as first application run");
    }

    if (file.open(QIODevice::ReadOnly))
    {
        QDomDocument doc;
        doc.setContent(file.readAll());
        file.close();

        fromXml(doc.firstChildElement(QLatin1String("WebWatcherApplication")));

        Log::info("Found %d watched sites after the data file loading", watcher.ids().size());
        Log::info("%d watched sites updates marked as unseen", changesCount);

        if (usedOldFile)
            QMessageBox::warning(this, tr("Warning - Savefile"), tr("Actual save file haven't found, but program content was restored from previous save file. Some changes can be lost, sorry for that."));
    }
    else if (saveFileFound)
    {
        Log::error("Can't open the data file \"%s\" for reading, must be fixed for proper application work", saveFilename.toLocal8Bit().data());
        QMessageBox::critical(this, tr("Error - Savefile"), tr("WebWatcher try to use directory \"%1\" as read/write application data directory, but the application can't read file \"%2\" from the directory, which must be fixed and the application should have read/write access for this directory for proper working").arg(writableLocationDir).arg(saveFilename));
    }
}

void MainWindow::handleSubsClick(const QModelIndex& index)
{
    QStandardItem *item = subsModel.itemFromIndex(index);

    int64_t id = item->data(ID).toLongLong();
    optional<WatchedSite> site = watcher.siteById(id);
    if (site)
    {
        const QString& storedManualTitle = item->data(ST_ManualTitle).toString();

        ui.siteEdit->setText(site->info.url.toString());
        ui.queryEdit->setText(site->info.jsQuery);
        ui.titleEdit->setText(storedManualTitle.isEmpty() ? site->page_title : storedManualTitle);

        ui.resetProbesCheckBox->setCheckState(item->data(ST_NotResetable).toBool() ? Qt::Unchecked : Qt::Checked);

        pair<int64_t, ReadableDuration::TimeUnit> format = ReadableDuration::toHumanReadable(site->info.updateIntervalMs);
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
        WatchedSiteDescription info;
        QString storedManualTitle = item->data(ST_ManualTitle).toString();
        bool isManualTitle = !storedManualTitle.isEmpty();

        info.url = QUrl(ui.siteEdit->text());
        if (isManualTitle)
            isManualTitle = !(ui.titleEdit->text().isEmpty());
        else
            isManualTitle = ui.titleEdit->text() != site->page_title;
        if (isManualTitle)
        {
            storedManualTitle = ui.titleEdit->text();
            item->setData(QVariant(storedManualTitle), ST_ManualTitle);
        }

        QString filename = editorFileName(id);
        if (QFile::exists(filename))
        {
            QFile fin(filename);
            if (fin.open(QFile::ReadOnly))
            {
                QString code = QString::fromLocal8Bit(fin.readAll());
                ui.queryEdit->setText(code);
                Log::info("Load data and remove the editor file (%s) for watched site #%ld", filename.toLocal8Bit().data(), id);
                QFile::remove(filename);
            }
            else
            {
                Log::error("Fail to load data from editor file (%s) for watched site #%ld, must be fixed for proper work", filename.toLocal8Bit().data(), id);
                QMessageBox::warning(this, tr("Warning - External Editor"), tr("WebWatcher try to load new site's query from external editing file \"%1\", but can't open it for reading, so update from external editing won't be applied").arg(filename));
            }
        }
        info.jsQuery = ui.queryEdit->text();
        info.isDisabled = site->info.isDisabled;

        bool resetProbes = ui.resetProbesCheckBox->isChecked();

        const QString& unitName = ui.intervalUnitsCombobox->currentText();
        ReadableDuration::TimeUnit timeType = ReadableDuration::unitType(unitName);
        assert(timeType != ReadableDuration::TimeUnit::Unknown);
        info.updateIntervalMs = ReadableDuration::toMs(ui.intervalEdit->text().toLongLong(), timeType);

        watcher.updateSite(id, info, resetProbes);

        item->setText(itemName(*site, storedManualTitle));
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
        tray->setIcon(QIcon(QLatin1String(":/icons/watch.png")));
}

void MainWindow::increaseChangeCount()
{
    changesCount++;

    // No need set icon each time, when changeCount was increased
    // because we can do it only when it is really needed
    if (changesCount == 1)
        tray->setIcon(QIcon(QLatin1String(":/icons/updated.png")));
}

QDomElement MainWindow::toXml(QDomDocument& doc)
{
    QDomElement root = doc.createElement(QLatin1String("WebWatcherApplication"));

    root.setAttribute(QLatin1String("changesCount"), changesCount);


    QDomElement sitesRoot = doc.createElement(QLatin1String("WatchedSiteEntries"));
    for (int i = 0; i < subsModel.rowCount(); i++)
    {
        QStandardItem* item = subsModel.item(i);
        if (item)
        {
            WatchedEntryInfo info;
            info.id = item->data(ID).toLongLong();
            info.withChanges = item->data(ST_Updated).toBool();
            info.isIgnorable = item->data(ST_Ignorable).toBool();
            info.isUnresettable = item->data(ST_NotResetable).toBool();
            info.manualTitle = item->data(ST_ManualTitle).toString();

            sitesRoot.appendChild(info.toXml(doc));
        }
    }
    root.appendChild(sitesRoot);

    root.appendChild(watcher.toXml(doc));

    return root;
}

void MainWindow::fromXml(const QDomElement& content)
{
    assert(content.tagName() == QLatin1String("WebWatcherApplication"));

    assert(content.attributes().contains(QLatin1String("changesCount")));
    changesCount = content.attribute(QLatin1String("changesCount")).toInt();
    if (changesCount > 0)
        tray->setIcon(QIcon(QLatin1String(":/icons/updated.png")));

    assert(content.firstChildElement(QLatin1String("WebWatcher")).isNull() == false);
    watcher.fromXml(content.firstChildElement(QLatin1String("WebWatcher")));

    assert(content.firstChildElement(QLatin1String("WatchedSiteEntries")).isNull() == false);
    QList<int64_t> subsOrder;
    const QDomNodeList& infoElems
        = content.firstChildElement(QLatin1String("WatchedSiteEntries")).elementsByTagName(WatchedEntryInfo::classXmlTagName);
    for (int i = 0; i < infoElems.length(); i++)
    {
        const QDomElement& infoElem = infoElems.item(i).toElement();
        WatchedEntryInfo info;
        info.fromXml(infoElem);

        optional<WatchedSite> site = watcher.siteById(info.id);
        QStandardItem* entry = new QStandardItem(itemName(*site, info.manualTitle));
        entry->setData((qint64)info.id, ID);
        setUpdatedNoCounter(entry, info.withChanges);
        setIgnorable(entry, info.isIgnorable);
        setNotResetable(entry, info.isUnresettable);
        setDisabled(entry, site->info.isDisabled);
        entry->setData(QVariant(info.manualTitle), ST_ManualTitle);
        entry->setDropEnabled(false); // Disable overwrite items by another items

        subsModel.appendRow(entry);
    }
}

void MainWindow::setUpdated(QStandardItem* item, bool value, bool updateChangeCount)
{
    if (value && item->data(ST_Updated).toBool() == false && updateChangeCount)
        increaseChangeCount();
    else if (value == false && item->data(ST_Updated).toBool() && updateChangeCount)
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

void MainWindow::setNotResetable(QStandardItem* item, bool value)
{
    item->setData(value, ST_NotResetable);
}

QString MainWindow::itemName(const WatchedSite& site, const QString& manualTitle)
{
    const QString& name = manualTitle.isEmpty() ? (site.page_title.isEmpty() ? site.info.url.toString() : site.page_title) : manualTitle;
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

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete && event->modifiers() == Qt::NoModifier)
    {
        if (ui.requestHistoryView->selectionModel()->hasSelection())
        {
            QModelIndex subIndex = ui.subsView->currentIndex();
            QStandardItem* subItem = subsModel.itemFromIndex(subIndex);
            int64_t site_id = subItem->data(ID).toLongLong();

            QModelIndexList selection;
            while (!(selection = ui.requestHistoryView->selectionModel()->selectedIndexes()).isEmpty())
            {
                QModelIndex index = selection[0];

                watcher.removeSiteProbe(site_id, index.row());

                probesModel.removeRow(index.row());
            }
            event->accept();
            return;
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::openEditor()
{
    QStandardItem *item = subsModel.itemFromIndex(ui.subsView->currentIndex());

    // Exit, if no model selection
    if (item == nullptr)
        return;

    int64_t id = item->data(ID).toLongLong();

    bool openTempFile = true;
    QString filename = editorFileName(id);
    if(!QFile::exists(filename))
    {
        QFile fout(filename);
        if (fout.open(QFile::WriteOnly))
            fout.write(ui.queryEdit->text().toLocal8Bit());
        else
        {
            openTempFile = false;
            Log::error("WebWatcher try to use temporary file (%s), but can't open it to writing, must be fixed for proper working", filename.toLocal8Bit().data());
            QMessageBox::warning(this, tr("Warning - External Editor"), tr("WebWatcher try to use temporary file \"%1\" for external editor, but can't open it for writing, so external editor unavailable").arg(filename));
        }
    }
    if (openTempFile)
    {
        Log::info("Use temporary file (%s) as file for external editing for watched site #%ld", filename.toLocal8Bit().data(), id);
        QDesktopServices::openUrl(QUrl::fromLocalFile(filename));
    }
}

QString MainWindow::editorFileName(int64_t id)
{
    return QDir::tempPath() + EDITOR_FILES_DIR + QString::fromLatin1("editor-file-%1.js").arg(id);
}

void MainWindow::setUpdatedNoCounter (QStandardItem* item, bool value)
{
    setUpdated(item, value, false);
}
