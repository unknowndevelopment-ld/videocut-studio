#include "PreviewEngine.h"

#include "MediaUtils.h"
#include "Renderer.h"
#include "Theme.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QPainter>
#include <QRadialGradient>
#include <QSet>
#include <QThread>
#include <QVideoFrame>
#include <QVideoSink>
#include <QtMath>
#include <algorithm>

namespace {

QSize fitInto(const QSizeF &image, const QSizeF &box, const QString &mode)
{
    if (image.isEmpty() || box.isEmpty()) return {};
    if (mode == "stretch") return box.toSize();
    if (mode == "cover")
        return QSizeF(image).scaled(box, Qt::KeepAspectRatioByExpanding).toSize();
    return QSizeF(image).scaled(box, Qt::KeepAspectRatio).toSize();
}

bool needsAdjust(const Clip &c)
{
    return qAbs(c.brightness) > 1e-3 || qAbs(c.contrast - 1.0) > 1e-3
            || qAbs(c.saturation - 1.0) > 1e-3 || qAbs(c.hue) > 1e-3;
}

QImage adjustImage(const QImage &source, const Clip &c)
{
    if (source.isNull() || !needsAdjust(c)) return source;

    QImage img = source.convertToFormat(QImage::Format_ARGB32);
    const double sat = qMax(0.0, c.saturation);
    const double hue = c.hue * 3.14159265358979323846 / 180.0;
    const double cosH = std::cos(hue);
    const double sinH = std::sin(hue);

    // saturation matrix
    const double sr = 0.299, sg = 0.587, sb = 0.114;
    double m[3][3] = {
        {sr + (1 - sr) * sat, sg * (1 - sat), sb * (1 - sat)},
        {sr * (1 - sat), sg + (1 - sg) * sat, sb * (1 - sat)},
        {sr * (1 - sat), sg * (1 - sat), sb + (1 - sb) * sat},
    };
    if (qAbs(c.hue) > 1e-3) {
        const double hm[3][3] = {
            {cosH + (1 - cosH) / 3, 1.0 / 3 * (1 - cosH) - std::sqrt(1.0 / 3) * sinH,
             1.0 / 3 * (1 - cosH) + std::sqrt(1.0 / 3) * sinH},
            {1.0 / 3 * (1 - cosH) + std::sqrt(1.0 / 3) * sinH, cosH + (1 - cosH) / 3,
             1.0 / 3 * (1 - cosH) - std::sqrt(1.0 / 3) * sinH},
            {1.0 / 3 * (1 - cosH) - std::sqrt(1.0 / 3) * sinH,
             1.0 / 3 * (1 - cosH) + std::sqrt(1.0 / 3) * sinH, cosH + (1 - cosH) / 3},
        };
        double out[3][3] = {};
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                for (int k = 0; k < 3; ++k) out[i][j] += hm[i][k] * m[k][j];
        std::copy(&out[0][0], &out[0][0] + 9, &m[0][0]);
    }

    const double contrast = c.contrast;
    const double offset = c.brightness * 255.0;
    const int h = img.height();
    const int w = img.width();
    for (int y = 0; y < h; ++y) {
        auto *line = reinterpret_cast<QRgb *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            const int a = qAlpha(line[x]);
            if (a == 0) continue;
            double r = ((qRed(line[x]) - 128) * contrast + 128) + offset;
            double g = ((qGreen(line[x]) - 128) * contrast + 128) + offset;
            double b = ((qBlue(line[x]) - 128) * contrast + 128) + offset;
            const double rr = m[0][0] * r + m[0][1] * g + m[0][2] * b;
            const double gg = m[1][0] * r + m[1][1] * g + m[1][2] * b;
            const double bb = m[2][0] * r + m[2][1] * g + m[2][2] * b;
            line[x] = qRgba(qBound(0, int(rr), 255), qBound(0, int(gg), 255),
                            qBound(0, int(bb), 255), a);
        }
    }
    return img;
}

} // namespace

struct PreviewEngine::Layer {
    Clip clip;
    QMediaPlayer *player = nullptr;
    QVideoSink *sink = nullptr;
    QAudioOutput *audio = nullptr;
    QImage still;
    QImage frame;
    bool started = false;
    bool hasFrame = false;
    double frameTime = -1;
};

PreviewEngine::PreviewEngine(QObject *parent) : QObject(parent)
{
    m_tick.setInterval(33);
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, &PreviewEngine::tick);

    m_seekTimer.setSingleShot(true);
    m_seekTimer.setInterval(45);
    connect(&m_seekTimer, &QTimer::timeout, this, [this]() {
        seekAll();
        render();
    });
}

PreviewEngine::~PreviewEngine()
{
    m_tick.stop();
    const auto layers = m_layers.values();
    for (Layer *layer : layers) destroyLayer(layer);
}

void PreviewEngine::setProject(Project *project)
{
    const auto layers = m_layers.values();
    for (Layer *layer : layers) destroyLayer(layer);
    m_layers.clear();
    m_project = project;
    m_position = 0.0;
    m_playing = false;
    m_lastFrame = QImage();
    rebuild();
    scheduleSeek();
}

void PreviewEngine::invalidate()
{
    if (!m_project) return;
    rebuild();
    if (!m_playing) {
        scheduleSeek();
        render();
    }
}

void PreviewEngine::rebuild()
{
    if (!m_project) return;
    QSet<QString> alive;
    for (const Clip &clip : m_project->clips) {
        if (clip.kind == ClipKind::Audio && clip.path.isEmpty()) continue;
        if (clip.kind == ClipKind::Image && clip.path.isEmpty()) continue;
        alive.insert(clip.id);
        Layer *layer = m_layers.value(clip.id, nullptr);
        if (!layer) {
            layer = new Layer;
            layer->clip = clip;
            m_layers.insert(clip.id, layer);
        } else {
            layer->clip = clip;
        }

        if (clip.kind == ClipKind::Text) {
            if (clip.textAnim == "none") {
                const QString png = Renderer::textPng(clip);
                if (!png.isEmpty()) layer->still = QImage(png);
            } else {
                layer->still = Renderer::textFrame(clip, 1.0);
            }
        } else if (clip.kind == ClipKind::Image) {
            const QImage base(clip.path);
            layer->still = adjustImage(base, clip);
        } else if (clip.kind == ClipKind::Shape) {
            layer->still = Renderer::shapeFrame(clip);
        } else if (clip.kind == ClipKind::Video && !clip.path.isEmpty()) {
            if (!layer->player) createPlayer(layer);
        } else if (clip.kind == ClipKind::Audio && !clip.path.isEmpty()) {
            if (!layer->player) createPlayer(layer);
        }
    }

    const auto ids = m_layers.keys();
    for (const QString &id : ids) {
        if (alive.contains(id)) continue;
        destroyLayer(m_layers.take(id));
    }
}

void PreviewEngine::createPlayer(Layer *layer)
{
    if (!layer || layer->player) return;
    layer->player = new QMediaPlayer(this);
    layer->audio = new QAudioOutput(this);
    layer->sink = new QVideoSink(this);
    layer->player->setAudioOutput(layer->audio);
    layer->player->setVideoSink(layer->sink);
    layer->player->setSource(QUrl::fromLocalFile(layer->clip.path));

    QMediaPlayer *player = layer->player;
    QVideoSink *sink = layer->sink;
    connect(sink, &QVideoSink::videoFrameChanged, this, [this, layer, sink](const QVideoFrame &frame) {
        if (m_layers.value(layer->clip.id) != layer) return;
        const QImage image = frame.toImage();
        if (image.isNull()) return;
        layer->frame = image;
        layer->hasFrame = true;
        layer->frameTime = m_position;
        if (!m_playing) render();
    });
    connect(player, &QMediaPlayer::errorOccurred, this, [this, layer](QMediaPlayer::Error error, const QString &message) {
        if (error == QMediaPlayer::NoError) return;
        if (m_layers.value(layer->clip.id) == layer)
            emit statusMessage(tr("Playback problem: %1").arg(message));
    });
}

void PreviewEngine::destroyLayer(Layer *layer)
{
    if (!layer) return;
    if (layer->player) {
        layer->player->stop();
        layer->player->disconnect();
        if (layer->sink) layer->sink->disconnect();
        layer->player->deleteLater();
        if (layer->sink) layer->sink->deleteLater();
        if (layer->audio) layer->audio->deleteLater();
    }
    delete layer;
}

// ------------------------------------------------------------------ transport

void PreviewEngine::play()
{
    if (!m_project || m_playing) return;
    if (m_project->duration() <= 0) return;
    if (m_position >= m_project->duration() - 0.02) m_position = 0.0;
    m_playing = true;
    m_playBase = m_position;
    m_clock.start();
    m_tickCount = 0;
    m_frameCount = 0;
    for (Layer *layer : m_layers) {
        layer->started = false;
        if (m_position >= layer->clip.start && m_position < layer->clip.end())
            startLayer(layer, m_position);
        else
            applyAudio(layer);
    }
    m_tick.start();
    emit playingChanged(true);
    emit statusMessage(tr("Playing"));
    render();
}

void PreviewEngine::pause()
{
    if (!m_playing) return;
    m_playing = false;
    m_tick.stop();
    stopAll();
    emit playingChanged(false);
    render();
}

void PreviewEngine::stopAll()
{
    for (Layer *layer : m_layers) {
        if (layer->player) layer->player->pause();
        layer->started = false;
    }
}

void PreviewEngine::setPlayhead(double seconds)
{
    if (!m_project) return;
    m_position = qMax(0.0, qMin(seconds, qMax(m_project->duration(), 0.0)));
    if (m_playing) {
        m_playBase = m_position;
        m_clock.restart();
        stopAll();
        for (Layer *layer : m_layers) {
            if (m_position >= layer->clip.start && m_position < layer->clip.end())
                startLayer(layer, m_position);
        }
    } else {
        scheduleSeek();
    }
    render();
}

void PreviewEngine::scheduleSeek()
{
    m_seekTimer.start();
}

void PreviewEngine::seekAll()
{
    if (!m_project) return;
    for (Layer *layer : m_layers) {
        if (!layer->player) continue;
        const Clip &clip = layer->clip;
        if (m_position >= clip.start && m_position < clip.end()) {
            const double local = (m_position - clip.start) * qMax(0.05, clip.speed) + clip.srcIn;
            // A stopped player does not paint while paused; nudge it into the
            // paused state so seeking produces a frame.
            if (layer->player->playbackState() == QMediaPlayer::StoppedState) {
                layer->player->play();
                layer->player->pause();
            }
            layer->player->setPosition(qint64(qMax(0.0, local) * 1000));
            applyAudio(layer);
        } else if (layer->player->playbackState() != QMediaPlayer::StoppedState) {
            layer->player->pause();
            layer->started = false;
        }
    }
}

void PreviewEngine::startLayer(Layer *layer, double t)
{
    if (!layer->player) return;
    const Clip &clip = layer->clip;
    const double local = clip.srcIn + (t - clip.start) * qMax(0.05, clip.speed);
    if (qAbs(layer->player->playbackRate() - qBound(0.25, clip.speed, 4.0)) > 1e-3)
        layer->player->setPlaybackRate(qBound(0.25, clip.speed, 4.0));
    layer->player->setPosition(qint64(qMax(0.0, local) * 1000));
    applyAudio(layer);
    layer->player->play();
    layer->started = true;
}

void PreviewEngine::applyAudio(Layer *layer)
{
    if (!layer->audio) return;
    const Clip &clip = layer->clip;
    bool muted = clip.mute || clip.kind == ClipKind::Image || clip.kind == ClipKind::Text
            || clip.kind == ClipKind::Shape;
    if (m_project) {
        if (Track *track = m_project->track(clip.track))
            if (track->muted) muted = true;
    }
    layer->audio->setMuted(muted || !m_playing);
    layer->audio->setVolume(qBound(0.0, m_volume * clip.volume, 1.0));
}

void PreviewEngine::tick()
{
    if (!m_project || !m_playing) return;
    double t = m_playBase + m_clock.elapsed() / 1000.0;
    const double duration = m_project->duration();
    if (t >= duration) {
        t = duration;
        m_position = t;
        emit positionChanged(t);
        emit frameReady(composite(t));
        pause();
        return;
    }
    m_position = t;
    emit positionChanged(t);

    ++m_tickCount;
    for (Layer *layer : m_layers) {
        const Clip &clip = layer->clip;
        const bool active = t >= clip.start && t < clip.end();
        if (active && !layer->started && layer->player) startLayer(layer, t);
        if (!active && layer->started) {
            if (layer->player) layer->player->pause();
            layer->started = false;
        }
        if (active && layer->player && m_tickCount % 90 == 0) {
            const double expected = (clip.srcIn + (t - clip.start) * qMax(0.05, clip.speed)) * 1000.0;
            if (qAbs(layer->player->position() - expected) > 1200.0)
                layer->player->setPosition(qint64(expected));
        }
    }
    m_lastFrame = composite(t);
    ++m_frameCount;
    emit frameReady(m_lastFrame);
}

// ------------------------------------------------------------------ drawing
double PreviewEngine::fadeFactor(const Clip &clip, double t) const
{
    double f = 1.0;
    if (clip.fadeIn > 0.01) {
        const double local = t - clip.start;
        if (local < clip.fadeIn) f = qMin(f, qMax(0.0, local / clip.fadeIn));
    }
    if (clip.fadeOut > 0.01) {
        const double remaining = clip.end() - t;
        if (remaining < clip.fadeOut) f = qMin(f, qMax(0.0, remaining / clip.fadeOut));
    }
    return f;
}

PreviewEngine::Placement PreviewEngine::placement(const Clip &clip, const QSizeF &imageSize,
                                                  double t) const
{
    Placement p;
    if (!m_project || imageSize.isEmpty()) return p;

    const double W = m_project->width;
    const double H = m_project->height;
    const double scale = qMax(0.02, clip.scale);

    if (clip.kind == ClipKind::Text) {
        p.size = QSizeF(imageSize.width() * scale, imageSize.height() * scale);
        p.center = QPointF((W - p.size.width()) * clip.posX + p.size.width() / 2.0 + clip.x,
                           (H - p.size.height()) * clip.posY + p.size.height() / 2.0 + clip.y);
    } else {
        const QSizeF box(W * scale, H * scale);
        p.size = QSizeF(fitInto(imageSize, box, clip.fit));
        p.center = QPointF(W / 2.0 + clip.x, H / 2.0 + clip.y);
    }

    if (Transition *tr = m_project->transitionFor(clip.id)) {
        if (tr->isSlide()) {
            const double d = qMax(0.05, tr->duration);
            const double prog = qBound(0.0, (t - clip.start) / d, 1.0);
            const double off = 1.0 - prog;
            if (tr->type == "slideLeft") p.center.rx() += off * W;
            else if (tr->type == "slideRight") p.center.rx() -= off * W;
            else if (tr->type == "slideUp") p.center.ry() += off * H;
            else if (tr->type == "slideDown") p.center.ry() -= off * H;
        }
    }

    p.rotation = clip.rotation;
    return p;
}

QSizeF PreviewEngine::layerSourceSize(const Clip &clip, Layer *layer) const
{
    if (layer) {
        if (!layer->still.isNull()) return QSizeF(layer->still.size());
        if (layer->hasFrame && !layer->frame.isNull()) return QSizeF(layer->frame.size());
    }
    if (!clip.path.isEmpty()) {
        const MediaInfo info = MediaUtils::probe(clip.path);
        if (info.width > 0 && info.height > 0) return QSizeF(info.width, info.height);
    }
    return {};
}

QVector<PreviewEngine::ClipBox> PreviewEngine::clipBoxes(double t) const
{
    QVector<ClipBox> out;
    if (!m_project || m_size.isEmpty()) return out;

    auto tracks = m_project->videoTracks();
    for (int i = tracks.size() - 1; i >= 0; --i) {
        Track *track = tracks[i];
        if (track->hidden) continue;
        for (Clip *clipPtr : m_project->clipsOn(track->id)) {
            const Clip &clip = *clipPtr;
            if (t < clip.start || t >= clip.end()) continue;
            if (qBound(0.0, clip.opacity * fadeFactor(clip, t), 1.0) <= 0.004) continue;
            Layer *layer = m_layers.value(clip.id, nullptr);
            if (!layer) continue;
            const QSizeF source = layerSourceSize(clip, layer);
            if (source.isEmpty()) continue;
            const Placement place = placement(clip, source, t);
            out.append({clip.id,
                        QRectF(place.center - QPointF(place.size.width() / 2.0,
                                                      place.size.height() / 2.0),
                               place.size),
                        place.rotation});
        }
    }
    return out;
}

QImage PreviewEngine::layerImage(Layer *layer)
{
    if (!layer) return {};
    if (layer->clip.kind == ClipKind::Text || layer->clip.kind == ClipKind::Image
        || layer->clip.kind == ClipKind::Shape)
        return layer->still;
    if (!layer->hasFrame) return {};
    return adjustImage(layer->frame, layer->clip);
}

QImage PreviewEngine::composite(double t)
{
    if (!m_project || m_size.isEmpty()) return {};
    QImage canvas(m_size, QImage::Format_ARGB32_Premultiplied);
    canvas.fill(QColor(m_project->background));

    QPainter p(&canvas);
    p.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);

    const double h = m_size.height();
    const double k = h / qMax(1, m_project->height);

    auto tracks = m_project->videoTracks();
    for (int i = tracks.size() - 1; i >= 0; --i) {
        Track *track = tracks[i];
        if (track->hidden) continue;
        for (Clip *clipPtr : m_project->clipsOn(track->id)) {
            const Clip &clip = *clipPtr;
            if (t < clip.start || t >= clip.end()) continue;
            Layer *layer = m_layers.value(clip.id, nullptr);
            if (!layer) continue;
            QImage image;
            if (clip.kind == ClipKind::Text) {
                const double anim = Renderer::textAnimationTime();
                const double progress = clip.textAnim == "none"
                        ? 1.0 : qBound(0.0, (t - clip.start) / anim, 1.0);
                image = progress >= 0.999 ? layer->still : Renderer::textFrame(clip, progress);
            } else if (clip.kind == ClipKind::Image || clip.kind == ClipKind::Shape) {
                image = layer->still;
            } else if (layer->hasFrame) {
                image = layer->frame;
            }
            if (image.isNull()) continue;

            const double opacity = qBound(0.0, clip.opacity * fadeFactor(clip, t), 1.0);
            if (opacity <= 0.004) continue;

            p.save();
            p.setOpacity(opacity);

            const Placement place = placement(clip, QSizeF(image.size()), t);
            const QSizeF target(place.size.width() * k, place.size.height() * k);
            const double cx = place.center.x() * k;
            const double cy = place.center.y() * k;
            // Grade at preview resolution instead of the source resolution.
            if (clip.kind == ClipKind::Video && needsAdjust(clip)) {
                if (image.width() > target.width() + 2)
                    image = image.scaled(target.toSize(), Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation);
                image = adjustImage(image, clip);
            }

            p.translate(cx, cy);
            if (qAbs(clip.rotation) > 0.01) p.rotate(clip.rotation);
            const QRectF targetRect(-target.width() / 2, -target.height() / 2,
                                    target.width(), target.height());
            p.drawImage(targetRect, image);

            if (clip.vignette) {
                QRadialGradient gradient(targetRect.center(), qMax(targetRect.width(), targetRect.height()) * 0.62);
                gradient.setColorAt(0.0, QColor(0, 0, 0, 0));
                gradient.setColorAt(0.62, QColor(0, 0, 0, 0));
                gradient.setColorAt(1.0, QColor(0, 0, 0, 165));
                p.setBrush(gradient);
                p.setPen(Qt::NoPen);
                p.drawRect(targetRect);
            }
            p.restore();
        }
    }
    p.end();
    return canvas;
}

void PreviewEngine::render()
{
    if (!m_project || m_project->clips.isEmpty()) return;
    const QImage frame = composite(m_position);
    if (frame.isNull()) return;
    m_lastFrame = frame;
    emit frameReady(frame);
}

void PreviewEngine::setPreviewSize(const QSize &size)
{
    if (size.isEmpty()) return;
    m_requested = size;
    QSize target = size;
    if (m_project && m_project->width > 0 && m_project->height > 0)
        target = QSize(m_project->width, m_project->height)
                     .scaled(size, Qt::KeepAspectRatio);
    if (m_quality == 1) target = QSize(target.width() / 2, target.height() / 2);
    else if (m_quality == 2) target = QSize(target.width() / 3, target.height() / 3);
    target = target.expandedTo(QSize(320, 180)).boundedTo(QSize(2560, 1440));
    if (target == m_size) return;
    m_size = target;
    render();
}

void PreviewEngine::setQuality(int level)
{
    if (level == m_quality) return;
    m_quality = qBound(0, level, 2);
    m_size = QSize();  // force a resize
    setPreviewSize(m_requested);
}

void PreviewEngine::setVolume(double volume)
{
    m_volume = qBound(0.0, volume, 1.0);
    for (Layer *layer : m_layers) applyAudio(layer);
}
