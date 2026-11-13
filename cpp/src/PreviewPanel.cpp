#include "PreviewPanel.h"

#include "Theme.h"
#include "util.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QVBoxLayout>

PreviewSurface::PreviewSurface(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(200, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setAccessibleName(tr("Preview"));
    m_message = tr("Nothing to preview yet");
}

void PreviewSurface::setImage(const QImage &image)
{
    m_image = image;
    const qreal dpr = devicePixelRatioF();
    if (!m_image.isNull() && qAbs(m_image.devicePixelRatio() - dpr) > 0.01)
        m_image.setDevicePixelRatio(dpr);
    m_message.clear();
    update();
}

void PreviewSurface::setMessage(const QString &message)
{
    m_image = QImage();
    m_message = message;
    update();
}

void PreviewSurface::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    emit resized();
}

void PreviewSurface::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor("#000000"));
    if (!m_image.isNull()) {
        const qreal dpr = m_image.devicePixelRatio() > 0 ? m_image.devicePixelRatio() : 1.0;
        const QSizeF src(m_image.width() / dpr, m_image.height() / dpr);
        // Never upscale: a smaller frame is shown 1:1 instead of going blurry.
        const double s = qMin(1.0, qMin(width() / qMax(1.0, src.width()),
                                        height() / qMax(1.0, src.height())));
        const QRectF dst((width() - src.width() * s) / 2, (height() - src.height() * s) / 2,
                         src.width() * s, src.height() * s);
        if (qAbs(s - 1.0) < 1e-4) p.drawImage(dst.topLeft(), m_image);
        else {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(dst, m_image);
        }
    } else if (!m_message.isEmpty()) {
        p.setPen(Theme::muted());
        QFont f = p.font();
        f.setPointSizeF(f.pointSizeF() * 1.05);
        p.setFont(f);
        p.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, m_message);
    }
}

PreviewPanel::PreviewPanel(QWidget *parent) : QFrame(parent)
{
    setObjectName("Panel");
    setAccessibleName(tr("Preview"));

    m_engine = new PreviewEngine(this);
    m_surface = new PreviewSurface;
    connect(m_surface, &PreviewSurface::resized, this, [this]() {
        const qreal dpr = m_surface->devicePixelRatioF();
        if (m_surface->width() > 0 && m_surface->height() > 0)
            m_engine->setPreviewSize(QSize(qRound(m_surface->width() * dpr),
                                           qRound(m_surface->height() * dpr)));
    });

    connect(m_engine, &PreviewEngine::frameReady, this, [this](const QImage &image) {
        m_surface->setImage(image);
    });
    connect(m_engine, &PreviewEngine::positionChanged, this, [this](double seconds) {
        updateTimeLabel(seconds);
        emit positionChanged(seconds);
    });
    connect(m_engine, &PreviewEngine::playingChanged, this, [this](bool playing) {
        m_play->setIcon(Theme::icon(playing ? "pause" : "play", Theme::text(), Theme::px(18)));
        m_state->setText(playing ? tr("Playing") : QString());
        emit playingChanged(playing);
    });
    connect(m_engine, &PreviewEngine::statusMessage, this, [this](const QString &text) {
        if (!m_engine->isPlaying()) m_state->setText(text);
    });

    m_play = new QPushButton;
    m_play->setFixedSize(Theme::px(38), Theme::px(38));
    m_play->setIcon(Theme::icon("play", Theme::text(), Theme::px(18)));
    m_play->setIconSize(QSize(Theme::px(18), Theme::px(18)));
    m_play->setStyleSheet(QString("QPushButton { background:%1; border:none; border-radius:%2px; }"
                                  "QPushButton:hover { background:%3; }"
                                  "QPushButton:focus { border:2px solid %4; }")
                              .arg(Theme::panelHi().name())
                              .arg(Theme::px(19))
                              .arg(Theme::accentSoft().name())
                              .arg(Theme::accent().name()));
    m_play->setToolTip(tr("Play / pause (Space)"));
    m_play->setAccessibleName(tr("Play or pause"));
    connect(m_play, &QPushButton::clicked, this, &PreviewPanel::onPlayClicked);

    m_time = new QLabel("00:00.00 / 00:00.00");
    m_time->setObjectName("Timecode");
    m_time->setAccessibleName(tr("Playback position"));

    m_state = new QLabel;
    m_state->setObjectName("Hint");

    m_mute = new QPushButton;
    m_mute->setObjectName("Ghost");
    m_mute->setFixedSize(Theme::px(28), Theme::px(28));
    m_mute->setIcon(Theme::icon("speaker", Theme::muted(), Theme::px(15)));
    m_mute->setCheckable(true);
    m_mute->setToolTip(tr("Mute preview"));
    m_mute->setAccessibleName(tr("Mute preview"));
    connect(m_mute, &QPushButton::toggled, this, [this](bool muted) {
        m_mute->setIcon(Theme::icon(muted ? "speakerOff" : "speaker",
                                    muted ? Theme::danger() : Theme::muted(), Theme::px(15)));
        m_engine->setVolume(muted ? 0.0 : m_lastVolume);
    });

    m_volume = new QSlider(Qt::Horizontal);
    m_volume->setRange(0, 100);
    m_volume->setValue(90);
    m_volume->setFixedWidth(Theme::px(84));
    m_volume->setAccessibleName(tr("Preview volume"));
    connect(m_volume, &QSlider::valueChanged, this, [this](int value) {
        if (value > 0) m_lastVolume = value / 100.0;
        m_engine->setVolume(value / 100.0);
    });

    auto *controls = new QHBoxLayout;
    controls->setContentsMargins(Theme::px(10), Theme::px(6), Theme::px(10), Theme::px(6));
    controls->setSpacing(Theme::px(10));
    controls->addWidget(m_play);
    controls->addWidget(m_time);
    controls->addWidget(m_state);
    controls->addStretch(1);
    controls->addWidget(m_mute);
    controls->addWidget(m_volume);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);
    layout->setSpacing(0);
    layout->addWidget(m_surface, 1);
    layout->addLayout(controls);

    m_engine->setVolume(0.9);
}

QSize PreviewPanel::surfaceSize() const
{
    return m_surface->size();
}

QSize PreviewPanel::frameSize() const
{
    return m_engine->previewSize();
}

void PreviewPanel::refreshSize()
{
    const qreal dpr = m_surface->devicePixelRatioF();
    if (m_surface->width() > 0 && m_surface->height() > 0)
        m_engine->setPreviewSize(QSize(qRound(m_surface->width() * dpr),
                                       qRound(m_surface->height() * dpr)));
}

void PreviewPanel::setDuration(double seconds)
{
    m_duration = qMax(0.0, seconds);
    updateTimeLabel(m_engine->position());
}

void PreviewPanel::setTimeLabel(double seconds)
{
    updateTimeLabel(seconds);
}

void PreviewPanel::updateTimeLabel(double seconds)
{
    m_time->setText(timecode(seconds) + " / " + timecode(m_duration));
}

void PreviewPanel::onPlayClicked()
{
    emit playRequested();
}

void PreviewPanel::showMessage(const QString &text)
{
    m_surface->setMessage(text);
}

void PreviewPanel::resizeEvent(QResizeEvent *event)
{
    QFrame::resizeEvent(event);
}
