#pragma once

#include <QString>
#include <QtMath>

inline QString timecode(double seconds, int decimals = 2)
{
    seconds = qMax(0.0, seconds);
    const int hours = int(seconds / 3600.0);
    const int minutes = int(seconds / 60.0) % 60;
    const double secs = std::fmod(seconds, 60.0);
    const int width = 2 + (decimals > 0 ? decimals + 1 : 0);
    const QString body = QString("%1:%2").arg(minutes, 2, 10, QChar('0'))
                             .arg(secs, width, 'f', decimals, QChar('0'));
    return hours > 0 ? QString("%1:%2").arg(hours).arg(body) : body;
}

inline QString humanDuration(double seconds)
{
    seconds = qMax(0.0, seconds);
    if (seconds < 1.0) return QString("%1 ms").arg(int(seconds * 1000));
    if (seconds < 60.0) return QString("%1 s").arg(seconds, 0, 'f', seconds < 10 ? 1 : 0);
    const int m = int(seconds / 60);
    const int s = int(std::fmod(seconds, 60.0));
    return s ? QString("%1 min %2 s").arg(m).arg(s) : QString("%1 min").arg(m);
}

inline QString humanSize(double bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB"};
    int u = 0;
    while (bytes >= 1024 && u < 3) { bytes /= 1024; ++u; }
    return QString("%1 %2").arg(bytes, 0, 'f', 0).arg(units[u]);
}
