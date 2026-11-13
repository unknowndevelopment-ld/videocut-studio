#include "MainWindow.h"
#include "MediaUtils.h"
#include "Theme.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <cstdio>
#endif

int main(int argc, char *argv[])
{
    QStringList args;
    for (int i = 1; i < argc; ++i) args << QString::fromLocal8Bit(argv[i]);

    const bool selfTest = args.contains("--selftest");
    const bool console = selfTest || args.contains("--console");

#ifdef Q_OS_WIN
    if (console) {
        if (AllocConsole()) {
            freopen("CONOUT$", "w", stdout);
            freopen("CONOUT$", "w", stderr);
            SetConsoleTitleW(L"VideoCut Studio — diagnostics");
        }
    }
#endif

    QApplication app(argc, argv);
    app.setApplicationName("VideoCut Studio");
    app.setOrganizationName("VideoCut");
    app.setApplicationVersion("1.0.0");
    app.setStyle(QStyleFactory::create("Fusion"));

    QSettings settings("VideoCut", "VideoCutStudio");
    Theme::apply(&app, settings.value("ui_scale", 1.0).toDouble(),
                 settings.value("high_contrast", false).toBool());

    if (!MediaUtils::available()) {
#ifdef Q_OS_WIN
        MessageBoxW(nullptr,
                    L"ffmpeg was not found.\n\nPut ffmpeg.exe and ffprobe.exe in an 'ffmpeg'\n"
                    L"folder next to VideoCutStudio.exe.",
                    L"VideoCut Studio", MB_ICONERROR | MB_OK);
#else
        fprintf(stderr, "ffmpeg not found\n");
#endif
        return 1;
    }

    MainWindow window;
    window.show();

    QStringList files;
    for (const QString &a : args) {
        if (!a.startsWith("--") && QFileInfo::exists(a) && MediaUtils::isMediaFile(a)) files << a;
    }
    if (!files.isEmpty()) window.openFiles(files);

    if (selfTest) {
        QString outDir;
        for (int i = 0; i < args.size(); ++i)
            if (args[i] == "--out" && i + 1 < args.size()) outDir = args[i + 1];
        if (outDir.isEmpty()) outDir = QDir::currentPath() + "/cpp_selftest";
        QMetaObject::invokeMethod(&window, [&window, outDir]() { window.runSelfTest(outDir); },
                                  Qt::QueuedConnection);
    }

    return app.exec();
}
