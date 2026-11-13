#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class QObject;

struct MediaInfo {
    QString path;
    double duration = 0.0;
    int width = 0;
    int height = 0;
    double fps = 30.0;
    bool hasVideo = false;
    bool hasAudio = false;
    QString error;

    bool isImage() const { return hasVideo && duration <= 0.001; }
};

namespace MediaUtils {

QString ffmpegPath();
QString ffprobePath();
bool available();

QString appDataDir();
QString cacheDir();
QString defaultExportDir();

MediaInfo probe(const QString &path);
QString thumbSheet(const QString &path, int count = 64, double interval = 1.0, int height = 72);
QString poster(const QString &path, int height = 110);
QVector<float> waveform(const QString &path, int buckets = 1400);
QByteArray framePng(const QString &path, double seconds, int maxW, int maxH);

struct ProcResult {
    int code = -1;
    QByteArray out;
    QByteArray err;
    bool timeout = false;
};

ProcResult run(const QStringList &args, int timeoutMs = 120000);

void runAsync(QObject *context, std::function<void()> work, std::function<void()> done);

QString uniquePath(const QString &dir, const QString &stem, const QString &ext);

QStringList mediaExtensions();
bool isMediaFile(const QString &path);

} // namespace MediaUtils
