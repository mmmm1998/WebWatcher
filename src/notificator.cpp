#include "notificator.h"

#include <QPushButton>
#include <QDesktopServices>
#include <QDateTime>

Notificator::Notificator(const WatchedSite& site)
{
    m_ui.setupUi(this);
    this->setModal(true);
    this->setWindowModality(Qt::NonModal);
    this->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    connect(m_ui.okButton, &QPushButton::clicked, this, &QDialog::close);
    connect(m_ui.openUrlButton, &QPushButton::clicked, [url = site.url, this](){
        QDesktopServices::openUrl(url);
        this->close();
    });

    m_ui.text->setText(tr("%1 was updated in %2").arg(site.title).arg(QDateTime::currentDateTime().toString()));
}
