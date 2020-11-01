#include <QApplication>
#include <QCommandLineParser>
#include <QTranslator>

#include "mainwindow.h"
#include "timeunit.h"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String("webwatcher"));
    app.setApplicationVersion(QLatin1String("2.4.2"));

    //TODO description
    QCommandLineParser parser;
    //parser.setApplicationDescription("");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption trayOption({QLatin1String("t"), QLatin1String("tray")}, QCoreApplication::translate("main", "Startup WebWatcher in a tray"));
    parser.addOption(trayOption);

    parser.process(app);

    QTranslator translator;
    qDebug() << "System locale: " << QLocale::system();
    qDebug() << "System locale codes list" << QLocale::system().uiLanguages();
    if (translator.load(QLocale::system(), QLatin1String("webwatcher"), QLatin1String("_"), QLatin1String(":/translations")))
        app.installTranslator(&translator);

    ReadableDuration::init();

    MainWindow w;
    w.setWindowIcon(QIcon(QLatin1String(":/icons/watch.png")));

    if (parser.isSet(trayOption) == false)
        w.show();

    return app.exec();
}
