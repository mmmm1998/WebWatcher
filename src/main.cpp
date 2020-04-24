#include <QApplication>
#include <QCommandLineParser>

#include "mainwindow.h"

int main (int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String("webwatcher"));
    app.setApplicationVersion(QLatin1String("2.2.2"));

    //TODO description
    QCommandLineParser parser;
    //parser.setApplicationDescription("");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption trayOption({QLatin1String("t"), QLatin1String("tray")}, QCoreApplication::translate("main", "Startup WebWatcher in a tray"));
    parser.addOption(trayOption);

    parser.process(app);

    MainWindow w;
    w.setWindowIcon(QIcon(QLatin1String(":/icons/watch.png")));

    if (parser.isSet(trayOption) == false)
        w.show();

    return app.exec();
}
