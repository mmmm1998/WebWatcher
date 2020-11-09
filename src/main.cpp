#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>
#include <QDir>
#include <QStandardPaths>

#include "mainwindow.hpp"
#include "timeunit.hpp"
#include "applicationsettings.hpp"

int main (int argc, char *argv[]) {
    int appReturnCode;
    do {
        QApplication app(argc, argv);
        app.setApplicationName(QLatin1String("webwatcher"));
        app.setApplicationVersion(QLatin1String("2.5.2"));

        //TODO description
        QCommandLineParser parser;
        //parser.setApplicationDescription("");
        parser.addHelpOption();
        parser.addVersionOption();

        QCommandLineOption trayOption({QLatin1String("t"), QLatin1String("tray")}, QCoreApplication::translate("main", "Startup WebWatcher in a tray"));
        parser.addOption(trayOption);

        parser.process(app);

        // Load program settings
        WebWatcherApplSettings settings;
        const QString& settingsFilepath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QLatin1String("/webwatcher_settings.xml");
        if (QFile::exists(settingsFilepath))
            settings = loadSettings(settingsFilepath);
        else
            settings = defaultProgramSettings;

        QTranslator translator;
        qDebug() << "System locale: " << QLocale::system();
        qDebug() << "Settings locale: " << settings.usedLanguage;
        if (translator.load(settings.usedLanguage, QLatin1String("webwatcher"), QLatin1String("_"), QLatin1String(":/translations")))
            app.installTranslator(&translator);

        ReadableDuration::init();

        MainWindow w(settings);
        w.setWindowIcon(QIcon(QLatin1String(":/icons/watch.png")));

        if (parser.isSet(trayOption) == false)
            w.show();

        appReturnCode = app.exec();

        saveSettings(settingsFilepath, settings);
    } while (appReturnCode == MainWindow::EXIT_CODE_REBOOT);

    return appReturnCode;
}
