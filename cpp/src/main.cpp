#include "MainWindow.h"
#include "MediaUtils.h"
#include "Theme.h"

#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>
#include <cstdio>
#endif

namespace {

// Set VIDEOCUT_LOG_FILE to keep a timestamped log next to a crash report.
void logToFile(QtMsgType, const QMessageLogContext &, const QString &message)
{
    const QString path = qEnvironmentVariable("VIDEOCUT_LOG_FILE");
    if (path.isEmpty()) return;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;
    file.write(QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toUtf8());
    file.write("  ");
    file.write(message.toUtf8());
    file.write("\n");
}

} // namespace

int main(int argc, char *argv[])
{
    if (!qEnvironmentVariable("VIDEOCUT_LOG_FILE").isEmpty())
        qInstallMessageHandler(logToFile);

#ifdef Q_OS_WIN
    // Must match the shortcut's AppUserModelID so the taskbar groups/pins the
    // app like any other installed Windows application.
    SetCurrentProcessExplicitAppUserModelID(L"VideoCut.Studio");
#endif

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
    app.setApplicationVersion(QStringLiteral(VIDEOCUT_VERSION));
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
    QString projectToOpen;
    for (const QString &a : args) {
        if (a.startsWith("--") || !QFileInfo::exists(a)) continue;
        if (a.endsWith(".vcs", Qt::CaseInsensitive)) projectToOpen = a;
        else if (MediaUtils::isMediaFile(a)) files << a;
    }

    if (!projectToOpen.isEmpty()) {
        window.openProjectFile(projectToOpen);
    } else {
        if (!files.isEmpty()) window.openFiles(files);
        if (!selfTest && files.isEmpty()) {
            // Give the window a moment to appear, then offer crash recovery.
            QTimer::singleShot(400, &window, [&window]() { window.checkRecovery(); });
        }
    }

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
