#include "applicationlanguage.h"

#include <QDir>
#include <QDebug>

bool LanguageDialog::isEqual(const QLocale& A, const QLocale& B)
{
    return A.language() == B.language() && A.country() == B.country() && A.script() == B.script();
}

LanguageDialog::LanguageDialog(const WebWatcherApplSettings& settings)
{
    m_ui.setupUi(this);

    int choosen = 0;
    const QList<QLocale> availableLocales = listAvailableTranslations();
    for (int i = 0; i < availableLocales.size(); i++)
    {
        const QLocale& locale = availableLocales[i];

        m_ui.languageComboBox->addItem(localeToHumanString(locale), locale.bcp47Name());
        if (isEqual(settings.usedLanguage, locale))
            choosen = i;
    }
    qDebug() << choosen;
    m_ui.languageComboBox->setCurrentIndex(choosen);
}

QLocale LanguageDialog::choosenLanguage()
{
    return QLocale(m_ui.languageComboBox->currentData().toString());
}

QList<QLocale> LanguageDialog::listAvailableTranslations()
{
    QLocale defaultEnglishLocale(QLocale::English, QLocale::LatinScript, QLocale::UnitedStates);
    QSet<QLocale> availableLocales;
    availableLocales << defaultEnglishLocale;

    QDir dir(QLatin1String(":/translations"));
    for (const QString& entry: dir.entryList())
    {
        static const QString webwatcherPrefix = QString::fromLatin1("webwatcher_");
        // ".ts" length is 3
        const QString& languageCode = entry.mid(webwatcherPrefix.size(), entry.size() - webwatcherPrefix.size() - 3);
        availableLocales << QLocale(languageCode);
    }

    return availableLocales.toList();
}

QString LanguageDialog::localeToHumanString(const QLocale& locale)
{
    QString s = locale.nativeLanguageName();
    if (locale.country() != QLocale::AnyCountry)
        s += QLatin1String(" (") + locale.nativeCountryName() + QLatin1String(")");
    return s;
}

