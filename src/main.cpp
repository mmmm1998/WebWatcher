#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QDir>
#include <QStandardPaths>

#include "logging.hpp"
#include "mainwindow.hpp"
#include "timeunit.hpp"
#include "applicationsettings.hpp"

int main (int argc, char *argv[]) {
    int appReturnCode;
    do {
        constexpr const char* version = "2.5.2";

        QApplication app(argc, argv);
        app.setApplicationName(QLatin1String("webwatcher"));
        app.setApplicationVersion(QLatin1String(version));

        QCommandLineParser parser;
        parser.setApplicationDescription(
            QObject::tr("This application is designed to periodically check the content of web pages to determine if they have changed (so-called watched sites), and to manage the list of watched sites in an orderly and convenient way."));
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption trayOption({QLatin1String("t"), QLatin1String("tray")}, QObject::tr("Startup WebWatcher in a tray", "tray CLI option explanation"));
        QCommandLineOption logLevelOption(QLatin1String("loglevel"), QObject::tr("Set <level> for the application logging. Available options: debug, info, warning, error, fatal, off. Default logging level is info.", "verbose CLI option explanation"), QObject::tr("level"));
        parser.addOption(trayOption);
        parser.addOption(logLevelOption);

        parser.process(app);

        const QString& requestedLogLevel = parser.value(logLevelOption).toLower();
        if (requestedLogLevel == QLatin1String("debug"))
            Log::setLevel(Log::Level::Debug);
        else if (requestedLogLevel == QLatin1String("info"))
            Log::setLevel(Log::Level::Info);
        else if (requestedLogLevel == QLatin1String("warning"))
            Log::setLevel(Log::Level::Warning);
        else if (requestedLogLevel == QLatin1String("error"))
            Log::setLevel(Log::Level::Error);
        else if (requestedLogLevel == QLatin1String("fatal"))
            Log::setLevel(Log::Level::Fatal);
        else if (requestedLogLevel == QLatin1String("off"))
            Log::setLevel(Log::Level::Off);
        else
            Log::setLevel(Log::Level::Info);

        Log::info("Start WebWatcher (v%s)", version);
        // Load program settings
        bool defaultSettingsUsed = false;
        WebWatcherApplSettings settings;
        const QLatin1String settingsFilename("webwatcher_settings.xml");
        const QString& settingsFilepath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1String("/") + settingsFilename;
        Log::info("Found writable application directory: \"%s\"", QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toLocal8Bit().data());
        if (QFile::exists(settingsFilepath))
        {
            Log::info("Found settings file (%s), load it", settingsFilename.data());
            settings = loadSettings(settingsFilepath);
        }
        else
        {
            Log::info("Settings file not found, use default settings");
            settings = defaultProgramSettings;
            defaultSettingsUsed = true;
        }

        const QString& settingsLocaleCode = settings.usedLanguage.bcp47Name();
        if (defaultSettingsUsed)
            Log::info("Setup settings locale from system (%s)", QLocale::system().bcp47Name().toLocal8Bit().data());
        else
            Log::info("Use settings locale (%s)", settingsLocaleCode.toLocal8Bit().data());

        if (settingsLocaleCode != QString::fromLatin1("en"))
        {
            QTranslator translator;
            if (translator.load(settings.usedLanguage, QLatin1String("webwatcher"), QLatin1String("_"), QLatin1String(":/translations")))
                app.installTranslator(&translator);
            else
                Log::info("Localization for prefered language (%s) not found, will used default english text", settings.usedLanguage.bcp47Name().toLocal8Bit().data());
        }

        ReadableDuration::init();

        MainWindow w(settings);
        w.setWindowIcon(QIcon(QLatin1String(":/icons/watch.png")));

        if (parser.isSet(trayOption) == false)
            w.show();
        else
            Log::info("Start the application in tray");

        appReturnCode = app.exec();

        saveSettings(settingsFilepath, settings);
    } while (appReturnCode == MainWindow::EXIT_CODE_REBOOT);

    return appReturnCode;
}
