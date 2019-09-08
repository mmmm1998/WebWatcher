#ifndef NOTIFICATOR_H
#define NOTIFICATOR_H

#include <QDialog>
#include <QScopedPointer>

#include "webwatcher.h"
#include "ui_notification.h"

class Notificator : public QDialog
{
    Q_OBJECT

public:
    Notificator(const WatchedSite& site);

private:
    Ui::WebWatcherNotificationDialog m_ui;
};

#endif // NOTIFICATOR_H
