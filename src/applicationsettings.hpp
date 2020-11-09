#ifndef APPLICATIONSETTINGS_H
#define APPLICATIONSETTINGS_H

#include <QString>
#include <QLocale>

struct WebWatcherApplSettings
{
    QLocale usedLanguage;
};

static const WebWatcherApplSettings defaultProgramSettings{
    QLocale::system()
};

WebWatcherApplSettings loadSettings(const QString& settingsFilepath);
void saveSettings(const QString& settingsFilepath, const WebWatcherApplSettings& settings);

#endif // APPLICATIONSETTINGS_H
