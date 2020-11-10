#include "applicationsettings.hpp"

#include <QStandardPaths>
#include <QDomElement>
#include <QFile>

#include "logging.hpp"

WebWatcherApplSettings loadSettings(const QString& settingsFilepath)
{
    WebWatcherApplSettings settings;

    QFile file(settingsFilepath);
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
    {
        Log::error("Can't open settings file \"%s\" for reading, use default settings", settingsFilepath.toLocal8Bit().data());
        settings = defaultProgramSettings;
    }

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
