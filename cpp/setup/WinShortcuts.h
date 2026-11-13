#pragma once

#include <QString>

namespace WinShortcuts {

bool createShortcut(const QString &linkPath,
                    const QString &targetPath,
                    const QString &arguments,
                    const QString &workingDirectory,
                    const QString &description,
                    const QString &iconPath,
                    const QString &appId = QString());

QString startMenuDir();
QString desktopDir();

} // namespace WinShortcuts
