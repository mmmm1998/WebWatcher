#include "watcherinputdialog.h"

using namespace std;

WatcherInputDialog::WatcherInputDialog()
{
    m_ui.setupUi(this);

    const QStringList& units = ReadableDuration::supportedUnits();
    for (const QString& unit: units)
        m_ui.intervalComboBox->addItem(unit);
}

int64_t WatcherInputDialog::updateInterval()
{
    const QString& unitName = m_ui.intervalComboBox->currentText();
    const int64_t count = m_ui.intervalEdit->text().toLongLong();
    ReadableDuration::TimeUnit timeType = ReadableDuration::unitType(unitName);
    return ReadableDuration::toMs(count, timeType);
}

QUrl WatcherInputDialog::site()
{
    return QUrl(m_ui.siteEdit->text());
}

QString WatcherInputDialog::jsQuery()
{
    return m_ui.queryEdit->text();
}
