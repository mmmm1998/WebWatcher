#ifndef WATCHERINPUTDIALOG_H
#define WATCHERINPUTDIALOG_H

#include <qdialog.h>
#include <QScopedPointer>
#include <QUrl>

#include "ui_watcherinputdialog.h"
#include "timeunit.h"

class WatcherInputDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * Default constructor
     */
    WatcherInputDialog();

    std::int64_t updateInterval();

    QUrl site();
    QString title();
    QString jsQuery();

private:
    Ui::WatcherInputDialog m_ui;
};

#endif // WATCHERINPUTDIALOG_H
