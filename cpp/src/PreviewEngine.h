#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QObject>
#include <QRectF>
#include <QSize>
#include <QTimer>

#include "Project.h"

class QMediaPlayer;
class QVideoSink;
class QAudioOutput;

/*  Real time timeline preview.
 *
 *  Every media clip gets its own decoder (QMediaPlayer) which is kept in sync
 *  with a master clock.  Frames are composited on the fly with QPainter, so
 *  nothing is ever pre-rendered: pressing play starts instantly and edits show
 *  up immediately.
 */
class PreviewEngine : public QObject {
    Q_OBJECT
public:
    explicit PreviewEngine(QObject *parent = nullptr);
    ~PreviewEngine() override;

    void setProject(Project *project);
    void invalidate();

    void setPlayhead(double seconds);
    double position() const { return m_position; }

    void play();
    void pause();
    bool isPlaying() const { return m_playing; }

    void setPreviewSize(const QSize &size);
    void setQuality(int level);
    int quality() const { return m_quality; }
    QSize previewSize() const { return m_size; }
    QImage lastFrame() const { return m_lastFrame; }
    int frameCount() const { return m_frameCount; }

    void setVolume(double volume);

    struct ClipBox {
        QString id;
        QRectF rect;       // project pixels, unrotated, centred on the clip
        double rotation = 0.0;
    };
    // Bounding boxes of the clips visible at `t`, bottom-most first.
    QVector<ClipBox> clipBoxes(double t) const;

signals:
    void frameReady(const QImage &frame);
    void positionChanged(double seconds);
    void playingChanged(bool playing);
    void statusMessage(const QString &text);

private:
    struct Layer;
    struct Placement {
        QSizeF size;
        QPointF center;
        double rotation = 0.0;
    };

    Placement placement(const Clip &clip, const QSizeF &imageSize, double t) const;
    QSizeF layerSourceSize(const Clip &clip, Layer *layer) const;

    void rebuild();
    void createPlayer(Layer *layer);
    void destroyLayer(Layer *layer);
    void scheduleSeek();
    void seekAll();
    void startLayer(Layer *layer, double t);
    void stopAll();
    void tick();
    void render();
    QImage composite(double t);
    QImage layerImage(Layer *layer);
    double fadeFactor(const Clip &clip, double t) const;
    void applyAudio(Layer *layer);

    Project *m_project = nullptr;
    QHash<QString, Layer *> m_layers;
    QSize m_size{960, 540};
    QSize m_requested{960, 540};
    int m_quality = 0;
    double m_position = 0.0;
    bool m_playing = false;
    double m_playBase = 0.0;
    QElapsedTimer m_clock;
    QTimer m_tick;
    QTimer m_seekTimer;
    int m_tickCount = 0;
    double m_volume = 0.9;
    QImage m_lastFrame;
    int m_frameCount = 0;
};
