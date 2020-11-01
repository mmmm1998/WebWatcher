#ifndef LANGUAGEDIALOG_H
#define LANGUAGEDIALOG_H

#include <qdialog.h>
#include <QScopedPointer>
#include <QUrl>
#include <QSet>
#include <QLocale>

#include "applicationsettings.h"

#include "ui_languagedialog.h"

class LanguageDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * Default constructor
     */
    LanguageDialog(const WebWatcherApplSettings& settings);

    QLocale choosenLanguage();

    static bool isEqual(const QLocale& A, const QLocale& B);
private:
    QList<QLocale> listAvailableTranslations();
    static QString localeToHumanString(const QLocale& locale);

private:
    Ui::LanguageDialog m_ui;
};

#endif // LANGUAGEDIALOG_H
