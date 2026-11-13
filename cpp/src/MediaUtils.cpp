#include "MediaUtils.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QPointer>
#include <QProcess>
#include <QRunnable>
#include <QStandardPaths>
#include <QThreadPool>
#include <QtMath>

namespace MediaUtils {

static QString s_ffmpeg;
static QString s_ffprobe;
static QHash<QString, MediaInfo> s_probeCache;

static QString findTool(const QString &exe)
{
    const QString appDir = QCoreApplication::applicationDirPath();
    QStringList candidates;
    const QByteArray env = qgetenv(exe.startsWith("ffmpeg") ? "VIDEOCUT_FFMPEG" : "VIDEOCUT_FFPROBE");
    if (!env.isEmpty()) candidates << QString::fromLocal8Bit(env);
    candidates << appDir + "/ffmpeg/" + exe
               << appDir + "/" + exe
               << appDir + "/vendor/ffmpeg/" + exe
               << appDir + "/../vendor/ffmpeg/" + exe
               << appDir + "/../../vendor/ffmpeg/" + exe;
#ifdef VIDEOCUT_SOURCE_DIR
    candidates << QStringLiteral(VIDEOCUT_SOURCE_DIR) + "/vendor/ffmpeg/" + exe;
#endif
    for (const QString &c : candidates) {
        if (QFileInfo::exists(c)) return QDir::cleanPath(c);
    }
    return exe; // PATH
}

QString ffmpegPath()
{
    if (s_ffmpeg.isEmpty()) s_ffmpeg = findTool("ffmpeg.exe");
    return s_ffmpeg;
}

QString ffprobePath()
{
    if (s_ffprobe.isEmpty()) s_ffprobe = findTool("ffprobe.exe");
    return s_ffprobe;
}

bool available()
{
    ProcResult r = run({ffmpegPath(), "-version"}, 20000);
    return r.code == 0;
}

QString appDataDir()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    const QString path = base + "/VideoCutStudio";
    QDir().mkpath(path);
    return path;
}

QString cacheDir()
{
    const QString path = appDataDir() + "/cache";
    QDir().mkpath(path);
    return path;
}

QString defaultExportDir()
{
    for (const char *name : {"Videos", "Desktop"}) {
        const QString p = QDir::homePath() + "/" + name;
        if (QDir(p).exists()) return p;
    }
    return QDir::homePath();
}

ProcResult run(const QStringList &args, int timeoutMs)
{
    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start(args.first(), args.mid(1));
    ProcResult r;
    if (!proc.waitForStarted(10000)) {
        r.err = "Could not start " + args.first().toUtf8();
        return r;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        r.timeout = true;
        proc.kill();
        proc.waitForFinished(3000);
        r.err = "Timed out";
        r.code = -1;
        return r;
    }
    r.out = proc.readAllStandardOutput();
    r.err = proc.readAllStandardError();
    r.code = proc.exitCode();
    return r;
}

void runAsync(QObject *context, std::function<void()> work, std::function<void()> done)
{
    QPointer<QObject> ctx(context);
    class Job : public QRunnable {
    public:
        std::function<void()> work;
        std::function<void()> done;
        QPointer<QObject> ctx;
        void run() override
        {
            try {
                if (work) work();
            } catch (...) {
            }
            if (ctx && done) {
                QMetaObject::invokeMethod(ctx.data(), [d = done]() { d(); }, Qt::QueuedConnection);
            }
        }
    };
    auto *job = new Job;
    job->work = std::move(work);
    job->done = std::move(done);
    job->ctx = ctx;
    QThreadPool::globalInstance()->start(job);
}

static QString cacheKey(const QString &path, const QString &salt)
{
    const QFileInfo fi(path);
    const QString raw = QDir::cleanPath(fi.absoluteFilePath()) + "|" + QString::number(fi.size())
            + "|" + QString::number(fi.lastModified().toSecsSinceEpoch()) + "|" + salt;
    return QString::fromLatin1(
        QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha1).toHex().left(20));
}

MediaInfo probe(const QString &path)
{
    const QString key = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    auto it = s_probeCache.constFind(key);
    if (it != s_probeCache.constEnd()) return it.value();

    MediaInfo info;
    info.path = key;
    if (!QFileInfo::exists(key)) {
        info.error = "File not found";
        s_probeCache.insert(key, info);
        return info;
    }

    QImageReader reader(key);
    if (reader.canRead() && reader.format().toLower() != "gif") {
        const QSize sz = reader.size();
        info.hasVideo = true;
        info.width = sz.width();
        info.height = sz.height();
        info.duration = 0.0;
        s_probeCache.insert(key, info);
        return info;
    }

    ProcResult r = run({ffprobePath(), "-v", "error", "-print_format", "json",
                        "-show_format", "-show_streams", key}, 60000);
    if (r.code != 0) {
        info.error = QString::fromUtf8(r.err).trimmed();
        s_probeCache.insert(key, info);
        return info;
    }
    const QJsonObject root = QJsonDocument::fromJson(r.out).object();
    info.duration = root.value("format").toObject().value("duration").toString().toDouble();
    for (const auto &v : root.value("streams").toArray()) {
        const QJsonObject st = v.toObject();
        const QString type = st.value("codec_type").toString();
        if (type == "video" && !info.hasVideo) {
            if (st.value("disposition").toObject().value("attached_pic").toInt() == 1) continue;
            info.hasVideo = true;
            info.width = st.value("width").toInt();
            info.height = st.value("height").toInt();
            const QString rate = st.value("avg_frame_rate").toString("30/1");
            const QStringList parts = rate.split('/');
            if (parts.size() == 2) {
                const double n = parts[0].toDouble();
                const double d = parts[1].toDouble();
                if (d > 0 && n / d >= 1 && n / d <= 240) info.fps = n / d;
            }
            for (const auto &sd : st.value("side_data_list").toArray()) {
                const QJsonObject so = sd.toObject();
                if (so.contains("rotation")) {
                    const int rot = so.value("rotation").toInt();
                    if (rot == 90 || rot == 270) std::swap(info.width, info.height);
                }
            }
        } else if (type == "audio" && !info.hasAudio) {
            info.hasAudio = true;
        }
    }
    s_probeCache.insert(key, info);
    return info;
}

QString thumbSheet(const QString &path, int count, double interval, int height)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) return {};
    const QString out = cacheDir() + "/sheet_" + cacheKey(path, QStringLiteral("sheet%1-%2-%3")
                                                                 .arg(count).arg(interval).arg(height)) + ".jpg";
    if (QFileInfo::exists(out) && QFileInfo(out).size() > 0) return out;
    const int cols = 8;
    const int rows = qMax(1, (count + cols - 1) / cols);
    const QString vf = QStringLiteral("fps=1/%1,scale=-2:%2,tile=%3x%4:padding=0:margin=0")
                           .arg(interval).arg(height).arg(cols).arg(rows);
    ProcResult r = run({ffmpegPath(), "-v", "error", "-y", "-i", path, "-vf", vf,
                        "-frames:v", "1", "-q:v", "5", out}, 120000);
    if (r.code == 0 && QFileInfo::exists(out)) return out;
    return {};
}

QString poster(const QString &path, int height)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) return {};
    const MediaInfo info = probe(path);
    if (info.isImage()) return path;
    const QString out = cacheDir() + "/thumb_" + cacheKey(path, "thumb" + QString::number(height)) + ".jpg";
    if (QFileInfo::exists(out) && QFileInfo(out).size() > 0) return out;
    const double seek = qMin(1.0, qMax(0.0, info.duration * 0.1));
    ProcResult r = run({ffmpegPath(), "-v", "error", "-y", "-ss", QString::number(seek, 'f', 3),
                        "-i", path, "-frames:v", "1", "-vf", "scale=-2:" + QString::number(height),
                        "-q:v", "4", out}, 60000);
    if (r.code == 0 && QFileInfo::exists(out)) return out;
    return {};
}

QVector<float> waveform(const QString &path, int buckets)
{
    QVector<float> peaks;
    if (path.isEmpty() || !QFileInfo::exists(path)) return peaks;
    const QString cache = cacheDir() + "/wave_" + cacheKey(path, "wave" + QString::number(buckets)) + ".bin";
    QFile f(cache);
    if (f.exists() && f.open(QIODevice::ReadOnly)) {
        QDataStream ds(&f);
        ds.setVersion(QDataStream::Qt_6_0);
        ds >> peaks;
        if (!peaks.isEmpty()) return peaks;
    }

    ProcResult r = run({ffmpegPath(), "-v", "error", "-i", path, "-vn", "-ac", "1", "-ar", "4000",
                        "-f", "s16le", "-"}, 300000);
    if (r.out.isEmpty()) return peaks;
    const auto *samples = reinterpret_cast<const qint16 *>(r.out.constData());
    const int n = r.out.size() / 2;
    if (n <= 0) return peaks;
    const int chunk = qMax(1, n / buckets);
    const int usable = (n / chunk) * chunk;
    peaks.reserve(usable / chunk);
    for (int i = 0; i < usable; i += chunk) {
        float maxv = 0.f;
        for (int j = 0; j < chunk; ++j) {
            const float v = qAbs(samples[i + j]) / 32768.f;
            if (v > maxv) maxv = v;
        }
        peaks.append(maxv);
    }
    if (f.open(QIODevice::WriteOnly)) {
        QDataStream ds(&f);
        ds.setVersion(QDataStream::Qt_6_0);
        ds << peaks;
    }
    return peaks;
}

QByteArray framePng(const QString &path, double seconds, int maxW, int maxH)
{
    if (path.isEmpty() || !QFileInfo::exists(path)) return {};
    const MediaInfo info = probe(path);
    if (info.isImage()) {
        QImage img(path);
        if (img.isNull()) return {};
        img = img.scaled(maxW, maxH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QByteArray data;
        QBuffer buffer(&data);
        buffer.open(QIODevice::WriteOnly);
        img.save(&buffer, "PNG");
        return data;
    }
    const QString scale = QStringLiteral("scale=w='min(iw,%1)':h='min(ih,%2)':"
                                         "force_original_aspect_ratio=decrease")
                              .arg(maxW).arg(maxH);
    ProcResult r = run({ffmpegPath(), "-v", "error", "-ss", QString::number(qMax(0.0, seconds), 'f', 3),
                        "-i", path, "-frames:v", "1", "-vf", scale,
                        "-f", "image2pipe", "-vcodec", "png", "-"}, 45000);
    return r.out;
}

QString uniquePath(const QString &dir, const QString &stem, const QString &ext)
{
    QString candidate = dir + "/" + stem + ext;
    int n = 1;
    while (QFileInfo::exists(candidate)) {
        candidate = dir + "/" + stem + " (" + QString::number(n) + ")" + ext;
        ++n;
    }
    return candidate;
}

QStringList mediaExtensions()
{
    return {"*.mp4", "*.mov", "*.mkv", "*.webm", "*.avi", "*.m4v", "*.mpg", "*.mpeg",
            "*.wmv", "*.flv", "*.ts", "*.mts", "*.m2ts", "*.3gp", "*.ogv",
            "*.mp3", "*.wav", "*.m4a", "*.aac", "*.flac", "*.ogg", "*.opus", "*.wma",
            "*.png", "*.jpg", "*.jpeg", "*.webp", "*.bmp", "*.gif", "*.tif", "*.tiff"};
}

bool isMediaFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    static const QStringList exts = {
        "mp4", "mov", "mkv", "webm", "avi", "m4v", "mpg", "mpeg", "wmv", "flv", "ts",
        "mts", "m2ts", "3gp", "ogv", "mp3", "wav", "m4a", "aac", "flac", "ogg", "opus",
        "wma", "aiff", "aif", "png", "jpg", "jpeg", "webp", "bmp", "gif", "tif", "tiff"};
    return exts.contains(suffix);
}

} // namespace MediaUtils
