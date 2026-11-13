#include "PreviewPanel.h"

#include "Theme.h"
#include "util.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QResizeEvent>
#include <QSlider>
#include <QTransform>
#include <QVBoxLayout>
#include <QtMath>
#include <cmath>

PreviewSurface::PreviewSurface(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(200, 120);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
    setAccessibleName(tr("Preview"));
    setAccessibleDescription(tr("Click a clip to select it. Drag its body to move, a corner to "
                               "resize, or the round handle to rotate."));
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
        const QRectF dst = imageRect();
        const qreal dpr = m_image.devicePixelRatio() > 0 ? m_image.devicePixelRatio() : 1.0;
        if (qAbs(dst.width() - m_image.width() / dpr) < 1e-4) p.drawImage(dst.topLeft(), m_image);
        else {
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            p.drawImage(dst, m_image);
        }
        if (m_engine && m_project && !m_engine->isPlaying() && !m_selection.isEmpty())
            drawOverlay(p);
    } else if (!m_message.isEmpty()) {
        p.setPen(Theme::muted());
        QFont f = p.font();
        f.setPointSizeF(f.pointSizeF() * 1.05);
        p.setFont(f);
        p.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter | Qt::TextWordWrap, m_message);
    }
}

QRectF PreviewSurface::imageRect() const
{
    if (m_image.isNull()) return {};
    const qreal dpr = m_image.devicePixelRatio() > 0 ? m_image.devicePixelRatio() : 1.0;
    const QSizeF src(m_image.width() / dpr, m_image.height() / dpr);
    if (src.isEmpty()) return {};
    // Never upscale: a smaller frame is shown 1:1 instead of going blurry.
    const double s = qMin(1.0, qMin(width() / qMax(1.0, src.width()),
                                    height() / qMax(1.0, src.height())));
    return QRectF((width() - src.width() * s) / 2, (height() - src.height() * s) / 2,
                  src.width() * s, src.height() * s);
}

QPointF PreviewSurface::toProject(const QPointF &surface) const
{
    const QRectF box = imageRect();
    if (!m_project || box.width() <= 0) return {};
    const double s = box.width() / qMax(1, m_project->width);
    return (surface - box.topLeft()) / s;
}

QPointF PreviewSurface::toSurface(const QPointF &project) const
{
    const QRectF box = imageRect();
    if (!m_project || m_project->width <= 0) return {};
    const double s = box.width() / double(m_project->width);
    return box.topLeft() + project * s;
}

QPolygonF PreviewSurface::boxCorners(const PreviewEngine::ClipBox &box) const
{
    const QPointF c = toSurface(box.rect.center());
    const QRectF r = imageRect();
    const double s = m_project ? r.width() / qMax(1, m_project->width) : 1.0;
    const double hx = box.rect.width() * s / 2.0;
    const double hy = box.rect.height() * s / 2.0;
    const QTransform t = QTransform().rotate(box.rotation);
    QPolygonF out;
    for (const QPointF &local : {QPointF(-hx, -hy), QPointF(hx, -hy),
                                 QPointF(hx, hy), QPointF(-hx, hy)})
        out << c + t.map(local);
    return out;
}

QPointF PreviewSurface::rotateHandle(const PreviewEngine::ClipBox &box) const
{
    const QPointF c = toSurface(box.rect.center());
    const QRectF r = imageRect();
    const double s = m_project ? r.width() / qMax(1, m_project->width) : 1.0;
    const double hy = box.rect.height() * s / 2.0;
    return c + QTransform().rotate(box.rotation).map(QPointF(0, -hy - 26.0));
}

int PreviewSurface::handleAt(const PreviewEngine::ClipBox &box, const QPointF &pos) const
{
    const QPolygonF corners = boxCorners(box);
    for (int i = 0; i < corners.size(); ++i) {
        if (QLineF(corners[i], pos).length() <= 9.0) return i;
    }
    if (QLineF(rotateHandle(box), pos).length() <= 11.0) return 4;
    return -1;
}

bool PreviewSurface::boxContains(const PreviewEngine::ClipBox &box, const QPointF &pos) const
{
    const QPointF c = toSurface(box.rect.center());
    const QRectF r = imageRect();
    const double s = m_project ? r.width() / qMax(1, m_project->width) : 1.0;
    const QPointF local = QTransform().rotate(-box.rotation).map(pos - c);
    return qAbs(local.x()) <= box.rect.width() * s / 2.0
            && qAbs(local.y()) <= box.rect.height() * s / 2.0;
}

void PreviewSurface::drawOverlay(QPainter &p)
{
    if (!m_engine) return;
    const QVector<PreviewEngine::ClipBox> boxes = m_engine->clipBoxes(m_engine->position());
    for (const PreviewEngine::ClipBox &box : boxes) {
        if (!m_selection.contains(box.id)) continue;

        const QPolygonF corners = boxCorners(box);
        const QPointF rot = rotateHandle(box);
        const QPointF topMid = (corners[0] + corners[1]) / 2.0;

        p.setRenderHint(QPainter::Antialiasing, true);
        QPen line(Theme::accent(), 1.4, Qt::DashLine);
        line.setDashPattern({5.0, 4.0});
        p.setPen(line);
        p.setBrush(Qt::NoBrush);
        p.drawPolygon(corners);
        p.drawLine(topMid, rot);

        p.setPen(QPen(Theme::accent(), 1.2));
        p.setBrush(QColor("#ffffff"));
        for (const QPointF &corner : corners)
            p.drawRoundedRect(QRectF(corner - QPointF(4.5, 4.5), QSizeF(9, 9)), 2, 2);

        p.setBrush(Theme::accent());
        p.drawEllipse(rot, 5.5, 5.5);
    }
}

void PreviewSurface::setSelection(const QStringList &ids)
{
    if (m_selection == ids) return;
    m_selection = ids;
    update();
}

QVector<QPointF> PreviewSurface::handlesFor(const QString &clipId) const
{
    QVector<QPointF> out;
    if (!m_engine || !m_project) return out;
    for (const PreviewEngine::ClipBox &box : m_engine->clipBoxes(m_engine->position())) {
        if (box.id != clipId) continue;
        const QPolygonF corners = boxCorners(box);
        for (const QPointF &corner : corners) out << corner;
        out << rotateHandle(box);
        break;
    }
    return out;
}

void PreviewSurface::mousePressEvent(QMouseEvent *event)
{
    if (!m_engine || !m_project || m_engine->isPlaying() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QVector<PreviewEngine::ClipBox> boxes = m_engine->clipBoxes(m_engine->position());
    const QPointF pos = event->position();

    for (int i = boxes.size() - 1; i >= 0; --i) {
        if (!m_selection.contains(boxes[i].id)) continue;
        const int handle = handleAt(boxes[i], pos);
        if (handle >= 0) {
            beginDrag(handle, boxes[i], pos);
            return;
        }
    }

    for (int i = boxes.size() - 1; i >= 0; --i) {
        if (!boxContains(boxes[i], pos)) continue;
        if (!m_selection.contains(boxes[i].id)) emit selectionRequested(boxes[i].id);
        beginDrag(-1, boxes[i], pos);
        return;
    }

    emit selectionRequested(QString());
    m_selection.clear();
    update();
}

void PreviewSurface::beginDrag(int handle, const PreviewEngine::ClipBox &box, const QPointF &pos)
{
    Clip *clip = m_project ? m_project->clip(box.id) : nullptr;
    if (!clip) return;

    m_dragId = box.id;
    m_dragArmed = false;
    m_pressSurface = pos;
    m_pressProject = toProject(pos);
    m_center = box.rect.center();
    m_boxSize = box.rect.size();
    m_startScale = clip->scale;
    m_startRotation = clip->rotation;
    m_startX = clip->x;
    m_startY = clip->y;
    m_startPosX = clip->posX;
    m_startPosY = clip->posY;
    m_startAngle = std::atan2(m_pressProject.y() - m_center.y(),
                              m_pressProject.x() - m_center.x());
    m_startDistance = qMax(1.0, QLineF(m_center, m_pressProject).length());
    m_drag = handle == 4 ? DragMode::Rotate
                         : handle >= 0 ? DragMode::Scale : DragMode::Move;
    setCursor(m_drag == DragMode::Rotate ? Qt::CrossCursor
                                         : m_drag == DragMode::Move ? Qt::SizeAllCursor
                                                                    : Qt::SizeFDiagCursor);
}

void PreviewSurface::applyDrag(const QPointF &pos, Qt::KeyboardModifiers modifiers)
{
    if (m_drag == DragMode::None) return;
    Clip *clip = m_project ? m_project->clip(m_dragId) : nullptr;
    if (!clip) {
        endDrag();
        return;
    }

    if (!m_dragArmed) {
        if (QLineF(pos, m_pressSurface).length() < 2.0) return;
        m_dragArmed = true;
        emit transformStarted(tr("Transform clip"),
                              QString("preview-transform:%1:%2").arg(m_dragId).arg(++m_dragSerial));
    }

    const QPointF p = toProject(pos);
    if (m_drag == DragMode::Move) {
        const QPointF delta = p - m_pressProject;
        if (clip->kind == ClipKind::Text) {
            const double freeX = qMax(1.0, m_project->width - m_boxSize.width());
            const double freeY = qMax(1.0, m_project->height - m_boxSize.height());
            clip->posX = qBound(0.0, m_startPosX + delta.x() / freeX, 1.0);
            clip->posY = qBound(0.0, m_startPosY + delta.y() / freeY, 1.0);
        } else {
            clip->x = m_startX + delta.x();
            clip->y = m_startY + delta.y();
        }
    } else if (m_drag == DragMode::Scale) {
        const double distance = QLineF(m_center, p).length();
        const double ratio = distance / m_startDistance;
        clip->scale = qBound(0.05, m_startScale * ratio, 4.0);
        if (clip->kind == ClipKind::Text && m_startScale > 0.001) {
            // Keep the text centred under the handles while it grows/shrinks.
            const double factor = clip->scale / m_startScale;
            clip->x = m_startX + m_boxSize.width() * (1.0 - factor) * (0.5 - m_startPosX);
            clip->y = m_startY + m_boxSize.height() * (1.0 - factor) * (0.5 - m_startPosY);
        }
    } else {
        const double angle = std::atan2(p.y() - m_center.y(), p.x() - m_center.x());
        double degrees = m_startRotation + qRadiansToDegrees(angle - m_startAngle);
        if (modifiers & Qt::ShiftModifier) degrees = std::round(degrees / 15.0) * 15.0;
        while (degrees > 180.0) degrees -= 360.0;
        while (degrees < -180.0) degrees += 360.0;
        clip->rotation = degrees;
    }

    emit transformChanged();
    update();
}

void PreviewSurface::endDrag()
{
    if (m_drag == DragMode::None) return;
    const bool armed = m_dragArmed;
    m_drag = DragMode::None;
    m_dragArmed = false;
    m_dragId.clear();
    unsetCursor();
    if (armed) emit transformFinished();
    update();
}

void PreviewSurface::mouseMoveEvent(QMouseEvent *event)
{
    if (m_drag != DragMode::None) {
        applyDrag(event->position(), event->modifiers());
        return;
    }
    updateHoverCursor(event->position());
    QWidget::mouseMoveEvent(event);
}

void PreviewSurface::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) endDrag();
    QWidget::mouseReleaseEvent(event);
}

void PreviewSurface::leaveEvent(QEvent *event)
{
    if (m_drag == DragMode::None) unsetCursor();
    QWidget::leaveEvent(event);
}

void PreviewSurface::updateHoverCursor(const QPointF &pos)
{
    if (!m_engine || !m_project || m_engine->isPlaying()) {
        unsetCursor();
        return;
    }
    const QVector<PreviewEngine::ClipBox> boxes = m_engine->clipBoxes(m_engine->position());
    for (int i = boxes.size() - 1; i >= 0; --i) {
        if (!m_selection.contains(boxes[i].id)) continue;
        const int handle = handleAt(boxes[i], pos);
        if (handle == 4) setCursor(Qt::CrossCursor);
        else if (handle == 0 || handle == 2) setCursor(Qt::SizeFDiagCursor);
        else if (handle == 1 || handle == 3) setCursor(Qt::SizeBDiagCursor);
        else continue;
        return;
    }
    for (int i = boxes.size() - 1; i >= 0; --i) {
        if (boxContains(boxes[i], pos)) {
            setCursor(Qt::SizeAllCursor);
            return;
        }
    }
    unsetCursor();
}

PreviewPanel::PreviewPanel(QWidget *parent) : QFrame(parent)
{
    setObjectName("Panel");
    setAccessibleName(tr("Preview"));

    m_engine = new PreviewEngine(this);
    m_surface = new PreviewSurface;
    m_surface->setEngine(m_engine);
    connect(m_surface, &PreviewSurface::selectionRequested, this, &PreviewPanel::clipSelected);
    connect(m_surface, &PreviewSurface::transformStarted, this, &PreviewPanel::transformStarted);
    connect(m_surface, &PreviewSurface::transformChanged, this, &PreviewPanel::transformChanged);
    connect(m_surface, &PreviewSurface::transformFinished, this, &PreviewPanel::transformFinished);
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

void PreviewPanel::setProject(Project *project)
{
    m_surface->setProject(project);
}

void PreviewPanel::setSelection(const QStringList &ids)
{
    m_surface->setSelection(ids);
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
