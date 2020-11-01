#include "applicationsettings.h"

#include <QStandardPaths>
#include <QDomElement>
#include <QFile>

WebWatcherApplSettings loadSettings(const QString& settingsFilepath)
{
    WebWatcherApplSettings settings;

    QFile file(settingsFilepath);
    //TODO: Better else branch?
    if (file.open(QFile::ReadOnly))
    {
        QDomDocument doc;
        doc.setContent(file.readAll());
        file.close();
        const QDomElement& root = doc.firstChildElement(QLatin1String("WebWatcherApplicationSettings"));


        const QDomElement& languageEl = root.firstChildElement(QLatin1String("UsedLanguage"));
        if (!languageEl.isNull())
            settings.usedLanguage =  QLocale(languageEl.text());
        else
            settings.usedLanguage = defaultProgramSettings.usedLanguage;
    }
    else
        settings = defaultProgramSettings;

    return settings;
}

void saveSettings(const QString& settingsFilepath, const WebWatcherApplSettings& settings)
{
    QDomDocument doc;
    QDomElement root = doc.createElement(QLatin1String("WebWatcherApplicationSettings"));

    QDomElement languageEl = doc.createElement(QLatin1String("UsedLanguage"));
    languageEl.appendChild(doc.createTextNode(settings.usedLanguage.bcp47Name()));
    root.appendChild(languageEl);

    doc.appendChild(root);
    QFile file(settingsFilepath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(doc.toByteArray(4));
        file.close();
    }
}
