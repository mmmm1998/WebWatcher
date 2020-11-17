#include <tuple>
#include <iostream>
#include <functional>

#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QDir>
#include <QList>
#include <QStandardPaths>
#include <QMetaType>
#include <QThread>

#include "mainwindow.hpp"
#include "logging.hpp"
#include "timeunit.hpp"
#include "applicationsettings.hpp"

#include <cefengine.hpp>

struct DelayedLog
{
    DelayedLog() = default;
    ~DelayedLog() {print();}

    void print()
    {
        for (auto iter = functors.begin(); iter != functors.end(); iter++)
            iter->operator()();
        functors.clear();
    }

    void push(std::function<void()> logFunction)
    {
        functors.push_back(std::move(logFunction));
    }

    QList<std::function<void()>> functors;
};

#define DELAYED_LOG(LOG_OBJECT, CAPTURE, CODE) LOG_OBJECT.push([CAPTURE](){CODE;});

struct CefShutdownDefer
{
    CefShutdownDefer() = default;
    ~CefShutdownDefer() {
        Log::debug("Call ChromiumWebEngine::shutdown()");
        ChromiumWebEngine::shutdown();
    }
};

std::tuple<int, char**> duplicate(int argc, char *argv[])
{
    int argcd = argc;
    char** argvd = new char*[argc];
    for (int i = 0; i < argc; i++)
    {
        size_t size = strlen(argv[i]);
        argvd[i] = new char[size+1];
        strncpy(argvd[i], argv[i], size);
        argvd[i][size] = '\0';
    }
    return {argcd, argvd};
}

int main (int argc, char *argv[]) {
    // save argc and argv for Qt later using
    // It is necessary, because after CEF inisialization argc will trash and argv[0] will contains entire command
    int argcd; char** argvd;
    std::tie(argcd, argvd) = duplicate(argc, argv);

    // CEF 3 forking is actually exec, so it means, that forked process starts from `main` function
    // So, I start CEF initizialization here
    int chrome_exit_code = ChromiumWebEngine::init(argc, argv);
    if (chrome_exit_code >= 0) {
        return chrome_exit_code;
    }
    CefShutdownDefer defer;

    qRegisterMetaType<int64_t>("int64_t");

    int appReturnCode = 0;
    do {
        constexpr const char* version = "2.7.0";

        QApplication app(argcd, argvd);
        app.setApplicationName(QLatin1String("webwatcher"));
        app.setApplicationVersion(QLatin1String(version));

        DelayedLog delayedLog;

        DELAYED_LOG(delayedLog, version, Log::info("Start WebWatcher (v%s)", version))
        // Load program settings
        bool defaultSettingsUsed = false;
        WebWatcherApplSettings settings;
        const QLatin1String settingsFilename("webwatcher_settings.xml");
        const QString& AppDataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        const QString& settingsFilepath =  AppDataDirectory + QLatin1String("/") + settingsFilename;
        DELAYED_LOG(delayedLog, AppDataDirectory, Log::info("Found writable application directory: \"%s\"", AppDataDirectory.toLocal8Bit().data()))
        if (QFile::exists(settingsFilepath))
        {
            DELAYED_LOG(delayedLog, settingsFilename, Log::info("Found settings file (%s), load it", settingsFilename.data()))
            settings = loadSettings(settingsFilepath);
        }
        else
        {
            DELAYED_LOG(delayedLog, ,Log::info("Settings file not found, use default settings"))
            settings = defaultProgramSettings;
            defaultSettingsUsed = true;
        }

        const QString& settingsLocaleCode = settings.usedLanguage.bcp47Name();
        if (defaultSettingsUsed)
            DELAYED_LOG(delayedLog, ,Log::info("Setup settings locale from system (%s)", QLocale::system().bcp47Name().toLocal8Bit().data()))
        else
            DELAYED_LOG(delayedLog, settingsLocaleCode, Log::info("Use settings locale (%s)", settingsLocaleCode.toLocal8Bit().data()))

        //NOTE: Translator here because the object must be live in same scope, where live MainWindow and QApplication
        QTranslator translator;
        if (settingsLocaleCode != QString::fromLatin1("en"))
        {
            const QString& preferedLanguage = settings.usedLanguage.bcp47Name();
            if (translator.load(settings.usedLanguage, QLatin1String("webwatcher"), QLatin1String("_"), QLatin1String(":/translations")))
            {
                DELAYED_LOG(delayedLog, preferedLanguage, Log::info("Found localization file for prefered language (%s), so use it", preferedLanguage.toLocal8Bit().data()))
                const QString& testTranslation = translator.translate("MainWindow", "Settings");
                DELAYED_LOG(delayedLog, testTranslation, Log::debug("Test founded translator: \"%s\" -> \"%s\"", "Settings", testTranslation.toLocal8Bit().data()))
                bool success = app.installTranslator(&translator);
                if (!success)
                    DELAYED_LOG(delayedLog, &preferedLanguage, Log::error("Setting founded translator for language (%s) have failed with unknown error", preferedLanguage.toLocal8Bit().data()))
            }
            else
                DELAYED_LOG(delayedLog, preferedLanguage, Log::info("Localization for prefered language (%s) not found, will used default english text", preferedLanguage.toLocal8Bit().data()))
        }

        QCommandLineParser parser;
        parser.setApplicationDescription(QObject::tr(
            "This application is designed to periodically check the content of web pages to determine if they have "\
            "changed (so-called watched sites), and to manage the list of watched sites in an orderly and convenient way."
        ));
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption trayOption({QLatin1String("t"), QLatin1String("tray")}, QObject::tr("Startup WebWatcher in a tray", "tray CLI option explanation"));
        QCommandLineOption delayOption(QLatin1String("delay"), QObject::tr("Delay in <time> seconds before inisialization GUI, useful for automatic startup on login"), QLatin1String("time"), QLatin1String("0"));
        QCommandLineOption logLevelOption(QLatin1String("loglevel"), QObject::tr("Set <level> for the application logging. Available options: debug, info, warning, error, fatal, off. Default logging level is info.", "verbose CLI option explanation"), QLatin1String("level"), QLatin1String("info"));
        parser.addOption(trayOption);
        parser.addOption(delayOption);
        parser.addOption(logLevelOption);

        parser.parse(app.arguments());

        bool isHelpRequest = parser.isSet(QLatin1String("help"));
        bool isVersionRequest = parser.isSet(QLatin1String("version"));
        bool isCliRequest = isVersionRequest || isHelpRequest;

        // No need to extra logging for CLI request like --help or --version or something else, Error level is enough
        if (!isCliRequest)
        {
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
        }
        else
            Log::setLevel(Log::Level::Error);
        //Print all log messages delayed before this moment
        delayedLog.print();

        //Manual handle help and version option
        //Execution this after translation installation is important
        if (isHelpRequest)
        {
            std::cout << parser.helpText().toStdString() << std::endl;
            break;
        }
        else if (isVersionRequest)
        {
            std::cout << app.applicationVersion().toStdString() << std::endl;
            break;
        }

        if (parser.isSet(delayOption))
        {
            const QString& timeString = parser.value(delayOption).toLower();
            bool ok = false;
            int delayInSeconds = timeString.toInt(&ok);
            if (ok)
                QThread::sleep(delayInSeconds);
            else
                Log::info("Can't parse argument for delay option (\"%s\"), so delay won't happen", timeString);
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
