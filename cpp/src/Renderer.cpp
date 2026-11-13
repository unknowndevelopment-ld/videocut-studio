#include "Renderer.h"

#include "MediaUtils.h"
#include "Theme.h"

#include <QCryptographicHash>
#include <climits>
#include <cmath>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QtMath>

namespace {

QString hexColor(const QString &value)
{
    static const QRegularExpression re("^#[0-9a-fA-F]{6}$");
    if (re.match(value).hasMatch()) return "0x" + value.mid(1);
    return "0x000000";
}

QStringList atempo(double speed)
{
    QStringList parts;
    double s = qMax(0.05, speed);
    while (s > 2.0) { parts << "atempo=2.0"; s /= 2.0; }
    while (s < 0.5) { parts << "atempo=0.5"; s /= 0.5; }
    if (qAbs(s - 1.0) > 1e-3) parts << QString("atempo=%1").arg(s, 0, 'f', 6);
    return parts;
}

int even(double v)
{
    return qMax(2, int(qRound(v / 2.0) * 2));
}

QStringList fitScale(const Clip &clip, int width, int height)
{
    const double s = qMax(0.02, clip.scale);
    const int boxW = even(width * s);
    const int boxH = even(height * s);
    if (clip.fit == "cover")
        return {QString("scale=w=%1:h=%2:force_original_aspect_ratio=increase").arg(boxW).arg(boxH)};
    if (clip.fit == "stretch")
        return {QString("scale=w=%1:h=%2").arg(boxW).arg(boxH)};
    return {QString("scale=w=%1:h=%2:force_original_aspect_ratio=decrease").arg(boxW).arg(boxH),
            QString("pad=%1:%2:(ow-iw)/2:(oh-ih)/2:color=black@0").arg(boxW).arg(boxH)};
}

QStringList effects(const Clip &clip)
{
    QStringList parts;
    if (qAbs(clip.brightness) > 1e-3 || qAbs(clip.contrast - 1.0) > 1e-3
        || qAbs(clip.saturation - 1.0) > 1e-3) {
        parts << QString("eq=brightness=%1:contrast=%2:saturation=%3")
                     .arg(clip.brightness, 0, 'f', 4)
                     .arg(clip.contrast, 0, 'f', 4)
                     .arg(clip.saturation, 0, 'f', 4);
    }
    if (qAbs(clip.hue) > 1e-3) parts << QString("hue=h=%1").arg(clip.hue, 0, 'f', 3);
    if (clip.blur > 1e-3) parts << QString("gblur=sigma=%1").arg(clip.blur, 0, 'f', 3);
    if (clip.sharpen > 1e-3) parts << QString("unsharp=5:5:%1:5:5:0").arg(clip.sharpen, 0, 'f', 3);
    if (clip.vignette) parts << "vignette=PI/5";
    if (qAbs(clip.rotation) > 1e-3) {
        const double angle = clip.rotation * 3.14159265358979323846 / 180.0;
        parts << QString("rotate=a=%1:ow=rotw(%1):oh=roth(%1):c=none")
                     .arg(angle, 0, 'f', 6);
    }
    return parts;
}

struct Graph {
    QStringList inputs;
    QStringList filters;
    QStringList audioLabels;
    QHash<QString, int> inputIndex;
    double total = 0.0;
    int inputCount = 0;

    int addInput(const QStringList &args)
    {
        const int index = inputCount++;
        inputs << args;
        return index;
    }
};

int clipInput(Graph &g, const Project &project, const Clip &clip, const RenderOptions &opts)
{
    auto it = g.inputIndex.constFind(clip.id);
    if (it != g.inputIndex.constEnd()) return it.value();

    int idx = -1;
    if (clip.kind == ClipKind::Text) {
        if (clip.textAnim != "none" && clip.duration > 0.1) {
            const QString dir = Renderer::textSequence(clip, opts.fps, clip.duration);
            if (!dir.isEmpty()) {
                idx = g.addInput({"-framerate", QString::number(opts.fps, 'g', 8),
                                  "-start_number", "0", "-i", dir + "/f%04d.png"});
                g.inputIndex.insert(clip.id, idx);
                return idx;
            }
        }
        const QString png = Renderer::textPng(clip);
        idx = g.addInput({"-loop", "1", "-framerate", QString::number(opts.fps, 'g', 8),
                          "-t", QString::number(qMax(clip.duration, 0.1) + 1.0, 'f', 3), "-i", png});
    } else if (!clip.path.isEmpty() && QFileInfo::exists(clip.path)) {
        const MediaInfo info = MediaUtils::probe(clip.path);
        if (clip.kind == ClipKind::Image || info.isImage()) {
            idx = g.addInput({"-loop", "1", "-framerate", QString::number(opts.fps, 'g', 8),
                              "-t", QString::number(qMax(clip.duration, 0.1) + 1.0, 'f', 3),
                              "-i", clip.path});
        } else {
            idx = g.addInput({"-i", clip.path});
        }
    } else {
        return -1;
    }
    g.inputIndex.insert(clip.id, idx);
    return idx;
}

QString videoChain(Graph &g, const Project &project, const Clip &clip, int idx, bool isText,
                   const RenderOptions &opts, const QString &label)
{
    const int W = opts.width;
    const int H = opts.height;
    const double speed = qMax(0.05, clip.speed);
    QStringList parts;

    if (!isText) {
        const double srcEnd = clip.srcIn + clip.duration * speed;
        parts << QString("trim=start=%1:end=%2").arg(clip.srcIn, 0, 'f', 6).arg(srcEnd, 0, 'f', 6);
    }
    parts << "format=rgba";
    parts << QString("setpts=(PTS-STARTPTS)/%1").arg(speed, 0, 'f', 6);
    parts << QString("fps=%1").arg(opts.fps, 0, 'g', 8);
    if (!isText) {
        parts << fitScale(clip, W, H);
        parts << "setsar=1";
    } else {
        const double k = double(H) / qMax(1, project.height);
        if (qAbs(k - 1.0) > 1e-3) parts << QString("scale=iw*%1:ih*%1").arg(k, 0, 'f', 5);
    }
    parts << effects(clip);
    if (clip.opacity < 0.999)
        parts << QString("colorchannelmixer=aa=%1").arg(qMax(0.0, clip.opacity), 0, 'f', 4);

    const double fadeIn = qMax(0.0, qMin(clip.fadeIn, clip.duration));
    const double fadeOut = qMax(0.0, qMin(clip.fadeOut, clip.duration - fadeIn * 0.5));
    if (fadeIn > 0.01) parts << QString("fade=t=in:st=0:d=%1:alpha=1").arg(fadeIn, 0, 'f', 3);
    if (fadeOut > 0.01)
        parts << QString("fade=t=out:st=%1:d=%2:alpha=1")
                     .arg(clip.duration - fadeOut, 0, 'f', 3).arg(fadeOut, 0, 'f', 3);

    parts << QString("setpts=PTS+%1/TB").arg(clip.start, 0, 'f', 6);
    g.filters << QString("[%1:v]").arg(idx) + parts.join(',') + "[" + label + "]";
    return label;
}

void audioChain(Graph &g, const Clip &clip, int idx, const QString &label)
{
    const double speed = qMax(0.05, clip.speed);
    const double srcEnd = clip.srcIn + clip.duration * speed;
    QStringList parts;
    parts << QString("atrim=start=%1:end=%2").arg(clip.srcIn, 0, 'f', 6).arg(srcEnd, 0, 'f', 6);
    parts << "asetpts=PTS-STARTPTS";
    parts << atempo(speed);
    parts << QString("volume=%1").arg(qMax(0.0, clip.volume), 0, 'f', 4);
    const double fadeIn = qMax(0.0, qMin(clip.fadeIn, clip.duration));
    const double fadeOut = qMax(0.0, qMin(clip.fadeOut, clip.duration - fadeIn * 0.5));
    if (fadeIn > 0.01) parts << QString("afade=t=in:st=0:d=%1").arg(fadeIn, 0, 'f', 3);
    if (fadeOut > 0.01)
        parts << QString("afade=t=out:st=%1:d=%2")
                     .arg(clip.duration - fadeOut, 0, 'f', 3).arg(fadeOut, 0, 'f', 3);
    const int delay = qMax(0, int(qRound(clip.start * 1000.0)));
    if (delay) parts << QString("adelay=%1|%1").arg(delay);
    parts << "aformat=sample_fmts=fltp:sample_rates=48000:channel_layouts=stereo";
    g.filters << QString("[%1:a]").arg(idx) + parts.join(',') + "[" + label + "]";
    g.audioLabels << label;
}

QString buildFilterGraph(Graph &g, const Project &projectConst, const RenderOptions &opts)
{
    Project &project = const_cast<Project &>(projectConst);
    const int W = opts.width;
    const int H = opts.height;
    g.filters << QString("color=c=%1:s=%2x%3:r=%4:d=%5,format=rgba[base]")
                     .arg(hexColor(project.background)).arg(W).arg(H)
                     .arg(opts.fps, 0, 'g', 8).arg(g.total, 0, 'f', 3);

    QString current = "base";
    int n = 0;
    auto videoTracks = project.videoTracks();
    for (int ti = videoTracks.size() - 1; ti >= 0; --ti) {
        Track *track = videoTracks[ti];
        if (track->hidden) continue;
        for (Clip *clipPtr : project.clipsOn(track->id)) {
            const Clip &clip = *clipPtr;
            if (clip.start >= g.total) continue;
            const int idx = clipInput(g, project, clip, opts);
            if (idx < 0) continue;
            const bool isText = clip.kind == ClipKind::Text;
            const QString label = QString("v%1").arg(n);
            videoChain(g, project, clip, idx, isText, opts, label);

            QString xExpr;
            QString yExpr;
            if (isText) {
                xExpr = QString("(main_w-overlay_w)*%1").arg(clip.posX, 0, 'f', 4);
                yExpr = QString("(main_h-overlay_h)*%1").arg(clip.posY, 0, 'f', 4);
                if (clip.textAnim == "slideUp" || clip.textAnim == "slideDown") {
                    const double d = 0.5;
                    const QString prog = QString("if(lt(t,%1),1-(t-%2)/%3,0)")
                                             .arg(clip.start + d, 0, 'f', 4)
                                             .arg(clip.start, 0, 'f', 4)
                                             .arg(d, 0, 'f', 4);
                    yExpr += (clip.textAnim == "slideUp" ? "+" : "-")
                            + QString("(%1)*0.09*main_h").arg(prog);
                }
            } else {
                xExpr = QString("(main_w-overlay_w)/2+%1").arg(clip.x, 0, 'f', 2);
                yExpr = QString("(main_h-overlay_h)/2+%1").arg(clip.y, 0, 'f', 2);
            }
            if (Transition *tr = project.transitionFor(clip.id)) {
                if (tr->isSlide()) {
                    const double d = qMax(0.05, tr->duration);
                    const QString prog = QString("if(lt(t,%1),1-(t-%2)/%3,0)")
                                             .arg(clip.start + d, 0, 'f', 4)
                                             .arg(clip.start, 0, 'f', 4)
                                             .arg(d, 0, 'f', 4);
                    if (tr->type == "slideLeft") xExpr += QString("+(%1)*main_w").arg(prog);
                    else if (tr->type == "slideRight") xExpr += QString("-(%1)*main_w").arg(prog);
                    else if (tr->type == "slideUp") yExpr += QString("+(%1)*main_h").arg(prog);
                    else if (tr->type == "slideDown") yExpr += QString("-(%1)*main_h").arg(prog);
                }
            }
            const double end = qMin(clip.end(), g.total);
            const QString out = QString("c%1").arg(n);
            g.filters << QString("[%1][%2]overlay=x='%3':y='%4':format=auto:eof_action=pass:"
                                 "enable='between(t,%5,%6)'[%7]")
                             .arg(current, label, xExpr, yExpr)
                             .arg(clip.start, 0, 'f', 4).arg(end, 0, 'f', 4).arg(out);
            current = out;
            ++n;
        }
    }
    g.filters << QString("[%1]null[vout]").arg(current);

    int a = 0;
    for (const Track &track : project.tracks) {
        if (track.muted) continue;
        for (Clip *clipPtr : project.clipsOn(track.id)) {
            const Clip &clip = *clipPtr;
            if (clip.mute || clip.kind == ClipKind::Image || clip.kind == ClipKind::Text) continue;
            if (clip.start >= g.total) continue;
            if (clip.path.isEmpty() || !QFileInfo::exists(clip.path)) continue;
            const MediaInfo info = MediaUtils::probe(clip.path);
            if (!info.hasAudio) continue;
            const int idx = clipInput(g, project, clip, opts);
            if (idx < 0) continue;
            audioChain(g, clip, idx, QString("a%1").arg(a++));
        }
    }

    if (a == 0) {
        g.filters << QString("anullsrc=r=48000:cl=stereo,atrim=0:%1[aout]").arg(g.total, 0, 'f', 3);
    } else if (a == 1) {
        g.filters << "[a0]anull[aout]";
    } else {
        QString joined;
        for (int i = 0; i < a; ++i) joined += QString("[a%1]").arg(i);
        g.filters << joined + QString("amix=inputs=%1:normalize=0:dropout_transition=0:"
                                      "duration=longest,alimiter=limit=0.98:level=0[aout]").arg(a);
    }
    return g.filters.join(';');
}

} // namespace

RenderOptions RenderOptions::scaledTo(int maxWidth) const
{
    if (width <= maxWidth) return *this;
    RenderOptions o = *this;
    const double k = double(maxWidth) / width;
    o.width = qMax(2, int(width * k) / 2 * 2);
    o.height = qMax(2, int(height * k) / 2 * 2);
    return o;
}

namespace {

QFont textFont(const Clip &clip, int size)
{
    QFont font;
    if (!clip.fontFamily.isEmpty()) {
        font.setFamily(clip.fontFamily);
    } else {
#ifdef Q_OS_WIN
        font.setFamily("Arial");
#endif
    }
    font.setPixelSize(size);
    font.setBold(clip.bold);
    font.setItalic(clip.italic);
    return font;
}

} // namespace

QImage Renderer::textFrame(const Clip &clip, double progress)
{
    const int size = qMax(6, clip.fontSize);
    const int outline = qMax(0, clip.outline);
    const QFont font = textFont(clip, size);
    const QFontMetrics fm(font);

    const QStringList lines = clip.text.isEmpty() ? QStringList{" "} : clip.text.split('\n');
    int textWidth = 0;
    for (const QString &line : lines)
        textWidth = qMax(textWidth, fm.horizontalAdvance(line.isEmpty() ? " " : line));

    const int lineHeight = int(size * 1.25);
    const int pad = outline + 6;
    const bool animated = clip.textAnim != "none";
    const int extra = animated ? int(lineHeight * 0.45) : 0;
    const int w = qMax(4, textWidth + pad * 2 + 4 + extra * 2);
    const int h = qMax(4, lineHeight * lines.size() + pad * 2 + 4 + extra * 2);

    QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const double p01 = qBound(0.0, progress, 1.0);
    const double eased = 1.0 - std::pow(1.0 - p01, 3.0);

    double alpha = 1.0;
    double dx = 0.0;
    double dy = 0.0;
    double scale = 1.0;
    int visible = INT_MAX;
    const QString anim = clip.textAnim;

    if (anim == "fade") alpha = p01;
    else if (anim == "pop") {
        scale = 0.55 + 0.45 * eased + 0.10 * std::sin(p01 * 3.141592653589793);
    } else if (anim == "slideUp") dy = (1.0 - eased) * lineHeight * 1.1;
    else if (anim == "slideDown") dy = -(1.0 - eased) * lineHeight * 1.1;
    else if (anim == "slideLeft") dx = (1.0 - eased) * textWidth * 0.75;
    else if (anim == "slideRight") dx = -(1.0 - eased) * textWidth * 0.75;
    else if (anim == "typewriter") {
        int total = 0;
        for (const QString &l : lines) total += l.size();
        visible = int(std::lround(p01 * total));
    }

    if (alpha > 0.001) {
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setFont(font);
        p.setOpacity(alpha);
        p.translate(w / 2.0 + dx, h / 2.0 + dy);
        p.scale(scale, scale);
        p.translate(-w / 2.0, -h / 2.0);

        int typingLine = -1;
        if (visible != INT_MAX) {
            int remaining = visible;
            for (int i = 0; i < lines.size(); ++i) {
                if (remaining <= lines[i].size()) { typingLine = i; break; }
                remaining -= lines[i].size();
            }
        }

        const QColor fill(clip.color);
        int y = pad + extra + lineHeight / 2;
        int remaining = visible;
        for (int i = 0; i < lines.size(); ++i) {
            QString shown = lines[i];
            if (remaining != INT_MAX) {
                shown = remaining <= 0 ? QString() : lines[i].left(qMin(remaining, lines[i].size()));
                remaining -= lines[i].size();
            }
            if (i == typingLine && p01 < 1.0) shown += QChar('|');
            if (shown.isEmpty()) { y += lineHeight; continue; }

            const int fullW = fm.horizontalAdvance(lines[i].isEmpty() ? QString(" ") : lines[i]);
            const int x = (w - fullW) / 2;
            if (outline > 0) {
                p.setPen(QColor("#000000"));
                for (int ox = -outline; ox <= outline; ++ox) {
                    for (int oy = -outline; oy <= outline; ++oy) {
                        if (ox * ox + oy * oy > outline * outline) continue;
                        p.drawText(x + ox, y + oy + fm.ascent() / 2, shown);
                    }
                }
            }
            p.setPen(fill);
            p.drawText(x, y + fm.ascent() / 2, shown);
            y += lineHeight;
        }
    }
    return img;
}

QString Renderer::textSequence(const Clip &clip, double fps, double duration)
{
    if (duration <= 0.05) return {};
    const int count = qMax(2, int(std::ceil(duration * fps)));
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
                                                (clip.text + "|" + QString::number(clip.fontSize) + "|"
                                                 + QString::number(clip.outline) + "|" + clip.color + "|"
                                                 + QString::number(int(clip.bold)) + "|"
                                                 + QString::number(int(clip.italic)) + "|"
                                                 + clip.fontFamily + "|" + clip.textAnim + "|"
                                                 + QString::number(count))
                                                    .toUtf8(),
                                                QCryptographicHash::Sha1).toHex().left(16));
    const QString dir = MediaUtils::cacheDir() + "/textseq_" + key;
    const QString lastFrame = QString("%1/f%2.png").arg(dir).arg(count - 1, 4, 10, QChar('0'));
    if (QFileInfo::exists(lastFrame) && QFileInfo(lastFrame).size() > 0) return dir;

    QDir().mkpath(dir);
    const double anim = qMin(textAnimationTime(), duration);
    for (int i = 0; i < count; ++i) {
        const double t = i / fps;
        const double progress = anim > 0.01 ? qBound(0.0, t / anim, 1.0) : 1.0;
        const QImage frame = textFrame(clip, progress);
        const QString path = QString("%1/f%2.png").arg(dir).arg(i, 4, 10, QChar('0'));
        if (!frame.save(path, "PNG")) return {};
    }
    return dir;
}

QString Renderer::textPng(const Clip &clip)
{
    const QString key = QString::fromLatin1(QCryptographicHash::hash(
                                                (clip.text + "|" + QString::number(clip.fontSize) + "|"
                                                 + QString::number(clip.outline) + "|" + clip.color + "|"
                                                 + QString::number(int(clip.bold)) + "|"
                                                 + QString::number(int(clip.italic)) + "|"
                                                 + clip.fontFamily)
                                                    .toUtf8(),
                                                QCryptographicHash::Sha1).toHex().left(16));
    const QString out = MediaUtils::cacheDir() + "/text_" + key + ".png";
    if (!QFileInfo::exists(out)) textFrame(clip, 1.0).save(out, "PNG");
    return out;
}

QStringList Renderer::buildCommand(const Project &project, const QString &outPath,
                                   const RenderOptions &opts, double *totalSeconds)
{
    Graph g;
    g.total = project.duration();
    if (opts.maxDuration > 0) g.total = qMin(g.total, opts.maxDuration);
    g.total = qMax(g.total, 0.2);
    const QString filter = buildFilterGraph(g, project, opts);

    QStringList args;
    args << MediaUtils::ffmpegPath() << "-y" << "-hide_banner" << "-nostdin"
         << "-loglevel" << "error" << "-progress" << "pipe:1" << "-nostats";
    args << g.inputs;
    args << "-filter_complex" << filter << "-map" << "[vout]";

    const QString fmt = opts.format.toLower();
    if (fmt == "gif") {
        args << "-map" << "[vout]" << "-loop" << "0" << "-r" << QString::number(qMin(opts.fps, 20.0), 'g', 4);
    } else {
        args << "-map" << "[aout]";
        if (fmt == "webm") {
            args << "-c:v" << "libvpx-vp9" << "-b:v" << "0" << "-crf"
                 << QString::number(qMin(63, opts.crf + 12)) << "-row-mt" << "1"
                 << "-pix_fmt" << "yuv420p" << "-deadline" << "good" << "-cpu-used" << "3"
                 << "-c:a" << "libopus" << "-b:a" << "128k";
        } else if (fmt == "mkv") {
            args << "-c:v" << "libx264" << "-preset" << opts.preset << "-crf"
                 << QString::number(opts.crf) << "-pix_fmt" << "yuv420p"
                 << "-c:a" << "aac" << "-b:a" << opts.audioBitrate;
        } else {
            args << "-c:v" << "libx264" << "-preset" << opts.preset << "-crf"
                 << QString::number(opts.crf) << "-pix_fmt" << "yuv420p"
                 << "-profile:v" << "high" << "-level" << "4.2"
                 << "-c:a" << "aac" << "-b:a" << opts.audioBitrate
                 << "-movflags" << "+faststart";
        }
    }
    args << "-t" << QString::number(g.total, 'f', 3)
         << "-max_muxing_queue_size" << "2048" << outPath;
    if (totalSeconds) *totalSeconds = g.total;
    return args;
}

QString Renderer::estimateSize(const Project &project, const RenderOptions &opts)
{
    double bits = double(opts.width) * opts.height * opts.fps * 0.07;
    double size = bits * qMax(project.duration(), 1.0) / 8.0;
    const char *units[] = {"B", "KB", "MB", "GB"};
    int u = 0;
    while (size >= 1024 && u < 3) { size /= 1024; ++u; }
    return QString("~%1 %2").arg(size, 0, 'f', 0).arg(units[u]);
}

Renderer::Renderer(QObject *parent) : QObject(parent) {}

Renderer::~Renderer()
{
    if (m_proc) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

bool Renderer::isRunning() const
{
    return m_proc && m_proc->state() != QProcess::NotRunning;
}

void Renderer::start(const Project &project, const QString &outPath, const RenderOptions &opts)
{
    if (isRunning()) cancel();
    m_outPath = outPath;
    m_cancelled = false;
    m_buffer.clear();

    QStringList args = buildCommand(project, outPath, opts, &m_total);
    const QString program = args.takeFirst();

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, &Renderer::onStdout);
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Renderer::onFinished);
    connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart) {
            emit failed("Could not start ffmpeg");
            m_proc->deleteLater();
            m_proc = nullptr;
        }
    });
    m_proc->start(program, args);
}

void Renderer::cancel()
{
    if (!m_proc) return;
    m_cancelled = true;
    m_proc->kill();
}

void Renderer::onStdout()
{
    if (!m_proc) return;
    m_buffer += m_proc->readAllStandardOutput();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(nl).trimmed();
        m_buffer.remove(0, nl + 1);
        if (line.startsWith("out_time_us=") || line.startsWith("out_time_ms=") || line.startsWith("out_time=")) {
            const QByteArray value = line.mid(line.indexOf('=') + 1);
            double seconds = 0;
            if (value.contains(':')) {
                const QList<QByteArray> parts = value.split(':');
                if (parts.size() == 3)
                    seconds = parts[0].toDouble() * 3600 + parts[1].toDouble() * 60 + parts[2].toDouble();
            } else {
                seconds = value.toDouble() / 1000000.0;
            }
            emit progress(qMin(seconds, m_total), m_total);
        }
    }
}

void Renderer::onFinished(int exitCode, QProcess::ExitStatus status)
{
    if (!m_proc) return;
    const QByteArray err = m_proc->readAllStandardError();
    m_proc->deleteLater();
    m_proc = nullptr;

    if (m_cancelled) {
        if (QFileInfo::exists(m_outPath) && QFileInfo(m_outPath).size() == 0) QFile::remove(m_outPath);
        emit failed("Cancelled");
        return;
    }
    if (exitCode != 0 || status != QProcess::NormalExit) {
        QString message = QString::fromUtf8(err).trimmed();
        if (message.size() > 1500) message = message.right(1500);
        if (message.isEmpty()) message = QString("ffmpeg exited with code %1").arg(exitCode);
        emit failed(message);
        return;
    }
    emit progress(m_total, m_total);
    emit finished(m_outPath);
}
