#include "WinShortcuts.h"

#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#endif

namespace WinShortcuts {

QString startMenuDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (base.isEmpty())
        base = QDir::homePath() + "/AppData/Roaming/Microsoft/Windows/Start Menu/Programs";
    return base;
}

QString desktopDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (base.isEmpty()) base = QDir::homePath() + "/Desktop";
    return base;
}

bool createShortcut(const QString &linkPath, const QString &targetPath, const QString &arguments,
                    const QString &workingDirectory, const QString &description,
                    const QString &iconPath)
{
#ifdef Q_OS_WIN
    IShellLinkW *link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                IID_IShellLinkW, reinterpret_cast<void **>(&link))))
        return false;

    link->SetPath(reinterpret_cast<LPCWSTR>(targetPath.utf16()));
    link->SetArguments(reinterpret_cast<LPCWSTR>(arguments.utf16()));
    link->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workingDirectory.utf16()));
    link->SetDescription(reinterpret_cast<LPCWSTR>(description.utf16()));
    if (!iconPath.isEmpty())
        link->SetIconLocation(reinterpret_cast<LPCWSTR>(iconPath.utf16()), 0);

    bool ok = false;
    IPersistFile *file = nullptr;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&file)))) {
        ok = SUCCEEDED(file->Save(reinterpret_cast<LPCOLESTR>(linkPath.utf16()), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
#else
    Q_UNUSED(linkPath);
    Q_UNUSED(targetPath);
    Q_UNUSED(arguments);
    Q_UNUSED(workingDirectory);
    Q_UNUSED(description);
    Q_UNUSED(iconPath);
    return false;
#endif
}

} // namespace WinShortcuts
