#include "SetupWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("VideoCut Studio Setup");
    app.setOrganizationName("VideoCut");
    app.setWindowIcon(QIcon(":/logo.png"));

    // The native stub unpacks this UI to %TEMP% and points us at the real file.
    QString payloadPath = QCoreApplication::applicationFilePath();
    const QStringList args = app.arguments();
    for (int i = 0; i + 1 < args.size(); ++i) {
        if (args[i] == "--payload" && QFileInfo::exists(args[i + 1]))
            payloadPath = args[i + 1];
    }

    SetupWindow window(payloadPath);
    if (args.contains("--silent")) return window.runSilent();

    if (args.contains("--screenshot")) {
        window.show();
        const int index = args.indexOf("--screenshot");
        const QString path = (index + 1 < args.size()) ? args[index + 1] : QString("setup.png");
        QTimer::singleShot(700, &window, [&window, path]() {
            window.grab().save(path);
            QCoreApplication::quit();
        });
        return app.exec();
    }

    window.show();
    return app.exec();
}
