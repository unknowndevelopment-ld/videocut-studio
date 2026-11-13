#include "TimelineWidget.h"

#include "MediaUtils.h"
#include "Theme.h"

#include <QApplication>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFontMetrics>
#include <QLinearGradient>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QToolTip>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

namespace {

constexpr int kHeaderW = 168;
constexpr int kRulerH = 32;
constexpr int kGap = 3;
constexpr double kMinPps = 1.0;
constexpr double kMaxPps = 600.0;
constexpr double kMinDur = 0.04;

const double kSteps[] = {0.04, 0.1, 0.2, 0.5, 1, 2, 5, 10, 15, 30, 60, 120, 300, 600, 1800};

QColor clipColor(ClipKind kind)
{
    switch (kind) {
    case ClipKind::Video: return QColor("#8f1f2a");
    case ClipKind::Image: return QColor("#5e1b2e");
    case ClipKind::Audio: return QColor("#a8323f");
    case ClipKind::Text: return QColor("#c04a2e");
    }
    return QColor("#8f1f2a");
}

QString timeText(double seconds, int decimals)
{
    seconds = qMax(0.0, seconds);
    const int hours = int(seconds / 3600);
    const int minutes = int(seconds / 60) % 60;
    const double secs = std::fmod(seconds, 60.0);
    const int width = 2 + (decimals > 0 ? decimals + 1 : 0);
    const QString body = QString("%1:%2").arg(minutes, 2, 10, QChar('0'))
                             .arg(secs, width, 'f', decimals, QChar('0'));
    return hours > 0 ? QString("%1:%2").arg(hours).arg(body) : body;
}

} // namespace

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setAcceptDrops(true);
    setMinimumHeight(180);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAccessibleName(tr("Timeline"));
    setAccessibleDescription(tr("Multi-track timeline. Use the clip list panel for keyboard navigation."));

    m_hbar = new QScrollBar(Qt::Horizontal, this);
    m_vbar = new QScrollBar(Qt::Vertical, this);
    connect(m_hbar, &QScrollBar::valueChanged, this, [this](int v) {
        m_scrollX = v / m_pps;
        update();
    });    connect(m_vbar, &QScrollBar::valueChanged, this, [this](int v) {
        m_scrollY = v;
        update();
    });
}

void TimelineWidget::setProject(Project *project)
{
    m_project = project;
    m_selected.clear();
    m_scrollX = 0;
    m_scrollY = 0;
    m_playhead = 0;
    updateScrollBars();
    updateA11y();
    update();
}

// ------------------------------------------------------------------ geometry

QVector<TimelineWidget::Row> TimelineWidget::rows() const
{
    QVector<Row> out;
    if (!m_project) return out;
    int y = Theme::px(kRulerH) + Theme::px(4) - m_scrollY;
    for (const Track &t : m_project->tracks) {
        const int h = Theme::px(t.kind == ClipKind::Video ? 78 : 58);
        out.append({const_cast<Track *>(&t), y, h});
        y += h + Theme::px(kGap);
    }
    return out;
}

QRect TimelineWidget::viewport() const
{
    const int left = Theme::px(kHeaderW);
    const int top = Theme::px(kRulerH);
    return QRect(left, top, qMax(10, width() - left - Theme::px(12)),
                 qMax(10, height() - top - Theme::px(12)));
}

double TimelineWidget::timeToX(double t) const
{
    return Theme::px(kHeaderW) + (t - m_scrollX) * m_pps;
}

double TimelineWidget::xToTime(double x) const
{
    return m_scrollX + (x - Theme::px(kHeaderW)) / m_pps;
}

QRectF TimelineWidget::clipRect(const Clip &clip) const
{
    for (const Row &row : rows()) {
        if (row.track->id != clip.track) continue;
        const double x0 = timeToX(clip.start);
        const double x1 = timeToX(clip.end());
        return QRectF(x0, row.top + Theme::px(3), qMax(2.0, x1 - x0),
                      row.height - Theme::px(6));
    }
    return {};
}

TimelineWidget::Row TimelineWidget::rowFor(const QPoint &pos) const
{
    for (const Row &row : rows())
        if (pos.y() >= row.top && pos.y() < row.top + row.height) return row;
    return {};
}

// ------------------------------------------------------------------- data

QStringList TimelineWidget::selectedIds() const
{
    QStringList ids;
    if (!m_project) return ids;
    for (const Clip &c : m_project->clips)
        if (m_selected.contains(c.id)) ids << c.id;
    return ids;
}

QVector<Clip *> TimelineWidget::selectedClips() const
{
    QVector<Clip *> out;
    if (!m_project) return out;
    for (Clip &c : const_cast<QVector<Clip> &>(m_project->clips))
        if (m_selected.contains(c.id)) out << &c;
    return out;
}

void TimelineWidget::selectIds(const QStringList &ids, bool additive)
{
    if (!additive) m_selected.clear();
    for (const QString &id : ids) {
        if (additive && m_selected.contains(id)) m_selected.remove(id);
        else m_selected.insert(id);
    }
    updateA11y();
    update();
    emit selectionChanged();
}

void TimelineWidget::clearSelection()
{
    m_selected.clear();
    updateA11y();
    update();
    emit selectionChanged();
}

void TimelineWidget::setPlayhead(double t, bool follow)
{
    t = qMax(0.0, t);
    if (qAbs(t - m_playhead) < 1e-6) return;
    const double oldX = timeToX(m_playhead);
    const double oldScroll = m_scrollX;
    m_playhead = t;
    if (follow) ensureVisible(t);
    if (qAbs(oldScroll - m_scrollX) > 1e-6) {
        update();
        return;
    }
    // Repaint only the two vertical bands the playhead moved between.
    const int pad = Theme::px(62);
    update(QRect(int(oldX) - pad, 0, pad * 2, height()));
    update(QRect(int(timeToX(t)) - pad, 0, pad * 2, height()));
}

void TimelineWidget::ensureVisible(double t)
{
    const QRect vp = viewport();
    const double x = timeToX(t);
    if (x < vp.left() + 40) m_scrollX = qMax(0.0, t - (vp.width() * 0.15) / m_pps);
    else if (x > vp.right() - 40) m_scrollX = qMax(0.0, t - (vp.width() * 0.7) / m_pps);
    updateScrollBars();
}

void TimelineWidget::setZoom(double pps, double anchorX)
{
    pps = qBound(kMinPps, pps, kMaxPps);
    if (anchorX < 0) anchorX = viewport().center().x();
    const double tAnchor = xToTime(anchorX);
    m_pps = pps;
    m_scrollX = qMax(0.0, tAnchor - (anchorX - Theme::px(kHeaderW)) / m_pps);
    updateScrollBars();
    update();
}

void TimelineWidget::zoomIn() { setZoom(m_pps * 1.35); }
void TimelineWidget::zoomOut() { setZoom(m_pps / 1.35); }

void TimelineWidget::zoomFit()
{
    if (!m_project) return;
    const double duration = qMax(m_project->duration(), 5.0);
    m_scrollX = 0;
    m_pps = qBound(kMinPps, (viewport().width() - 24) / duration, kMaxPps);
    updateScrollBars();
    update();
}

void TimelineWidget::refresh()
{
    updateScrollBars();
    update();
}

void TimelineWidget::updateScrollBars()
{
    if (!m_project || !m_hbar || !m_vbar) return;
    const QRect vp = viewport();
    const double contentW = qMax(0.0, m_project->duration() + 10.0);
    const double page = vp.width() / m_pps;
    m_hbar->setRange(0, qMax(0, int((contentW - page) * m_pps)));
    m_hbar->setPageStep(qMax(20, vp.width()));
    m_hbar->setValue(int(m_scrollX * m_pps));

    int totalH = 0;
    for (const Track &t : m_project->tracks) totalH += Theme::px(t.kind == ClipKind::Video ? 78 : 58) + Theme::px(kGap);
    m_vbar->setRange(0, qMax(0, totalH + Theme::px(8) - vp.height()));
    m_vbar->setPageStep(qMax(20, vp.height()));
    m_vbar->setValue(m_scrollY);
}

// ------------------------------------------------------------------ caches

QPixmap TimelineWidget::sheet(const QString &path)
{
    auto it = m_sheets.constFind(path);
    if (it != m_sheets.constEnd()) return it.value();
    if (!m_pending.contains("sheet:" + path)) {
        m_pending.insert("sheet:" + path);
        MediaUtils::runAsync(this,
            [path]() { MediaUtils::thumbSheet(path); },
            [this, path]() {
                m_pending.remove("sheet:" + path);
                const QString file = MediaUtils::thumbSheet(path);
                if (!file.isEmpty()) {
                    const QPixmap pm(file);
                    if (!pm.isNull()) m_sheets.insert(path, pm);
                }
                update();
            });
    }
    return {};
}

QPixmap TimelineWidget::poster(const QString &path)
{
    auto it = m_posters.constFind(path);
    if (it != m_posters.constEnd()) return it.value();
    if (!m_pending.contains("poster:" + path)) {
        m_pending.insert("poster:" + path);
        MediaUtils::runAsync(this,
            [path]() { MediaUtils::poster(path); },
            [this, path]() {
                m_pending.remove("poster:" + path);
                const QString file = MediaUtils::poster(path);
                if (!file.isEmpty()) {
                    const QPixmap pm(file);
                    if (!pm.isNull()) m_posters.insert(path, pm);
                }
                update();
            });
    }
    return {};
}

QVector<float> TimelineWidget::wave(const QString &path)
{
    auto it = m_waves.constFind(path);
    if (it != m_waves.constEnd()) return it.value();
    if (!m_pending.contains("wave:" + path)) {
        m_pending.insert("wave:" + path);
        MediaUtils::runAsync(this,
            [path]() { MediaUtils::waveform(path); },
            [this, path]() {
                m_pending.remove("wave:" + path);
                m_waves.insert(path, MediaUtils::waveform(path));
                update();
            });
    }
    return {};
}

// ----------------------------------------------------------------- painting

void TimelineWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing, true);
    p.fillRect(rect(), Theme::bg());
    if (!m_project) return;

    const auto layout = rows();
    const QRect vp = viewport();

    for (int i = 0; i < layout.size(); ++i) {
        const Row &row = layout[i];
        if (row.top + row.height < Theme::px(kRulerH) || row.top > height()) continue;
        QColor base = (i % 2 == 0) ? Theme::panel() : Theme::panelAlt();
        if (row.track->hidden) base = base.darker(115);
        p.fillRect(QRect(Theme::px(kHeaderW), row.top, width(), row.height), base);
        p.setPen(QPen(Theme::border(), 1));
        p.drawLine(Theme::px(kHeaderW), row.top + row.height + 1, width(), row.top + row.height + 1);
    }

    // grid
    p.save();
    p.setClipRect(vp);
    double step = kSteps[0];
    for (double s : kSteps) {
        if (s * m_pps >= Theme::px(78)) { step = s; break; }
    }
    double t = std::floor(m_scrollX / step) * step;
    while (t < m_scrollX + vp.width() / m_pps + step) {
        const double x = timeToX(t);
        p.setPen(QPen(QColor(255, 255, 255, 12), 1));
        p.drawLine(QPointF(x, Theme::px(kRulerH)), QPointF(x, height()));
        t += step;
    }

    for (const Row &row : layout) {
        if (row.top + row.height < Theme::px(kRulerH) || row.top > height()) continue;
        for (Clip *clip : m_project->clipsOn(row.track->id)) {
            const QRectF r = clipRect(*clip);
            if (r.right() < vp.left() - 4 || r.left() > vp.right() + 4) continue;
            drawClip(p, *clip, r, *row.track);
        }
    }
    p.restore();

    if (m_dragging && m_drag.mode == "rubber") {
        p.setPen(QPen(Theme::accent(), 1, Qt::DashLine));
        p.setBrush(QColor(Theme::accent().red(), Theme::accent().green(), Theme::accent().blue(), 40));
        p.drawRoundedRect(m_band, 4, 4);
    }

    drawRuler(p);
    drawHeaders(p, layout);
    drawTransitions(p);
    drawPlayhead(p);

    if (m_snapLine >= 0) {
        const double x = timeToX(m_snapLine);
        if (x >= vp.left() && x <= vp.right()) {
            p.setPen(QPen(Theme::warn(), 1, Qt::DashLine));
            p.drawLine(QPointF(x, Theme::px(kRulerH)), QPointF(x, height()));
        }
    }

    if (m_project->clips.isEmpty()) {
        p.setPen(Theme::muted());
        QFont f = p.font();
        f.setPointSizeF(f.pointSizeF() * 1.05);
        p.setFont(f);
        p.drawText(vp, Qt::AlignCenter, tr("Drag media here\nor press Ctrl+I to import"));
    }

    if (m_focusRing) {
        p.setPen(QPen(Theme::accent(), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(1, 1, -2, -2), 8, 8);
    }
}

void TimelineWidget::drawClip(QPainter &p, const Clip &clip, const QRectF &rect, const Track &track)
{
    const bool selected = m_selected.contains(clip.id);
    QColor base = clipColor(clip.kind);
    if (track.hidden) base = base.darker(135);

    const double radius = Theme::px(7);
    QPainterPath path;
    path.addRoundedRect(rect, radius, radius);
    p.save();
    p.setClipPath(path);

    QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
    grad.setColorAt(0.0, base.lighter(118));
    grad.setColorAt(1.0, base.darker(118));
    p.fillPath(path, grad);

    if (clip.kind == ClipKind::Video && !clip.path.isEmpty()) drawFilmstrip(p, clip, rect);
    else if (clip.kind == ClipKind::Image && !clip.path.isEmpty()) drawStill(p, clip, rect);
    else if (clip.kind == ClipKind::Audio && !clip.path.isEmpty())
        drawWave(p, clip, rect, QColor(255, 255, 255, 120));
    else if (clip.kind == ClipKind::Text) {
        p.setPen(QColor(255, 255, 255, 70));
        QFont f = p.font();
        f.setItalic(true);
        p.setFont(f);
        p.drawText(rect.adjusted(Theme::px(10), Theme::px(24), -Theme::px(8), 0),
                   Qt::AlignLeft | Qt::AlignTop, clip.text.left(120));
    }

    if (clip.fadeIn > 0.01) {
        const double w = qMin(rect.width(), clip.fadeIn * m_pps);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 105));
        p.drawPolygon(QPolygonF({rect.topLeft(), QPointF(rect.left() + w, rect.top()), rect.bottomLeft()}));
    }
    if (clip.fadeOut > 0.01) {
        const double w = qMin(rect.width(), clip.fadeOut * m_pps);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 105));
        p.drawPolygon(QPolygonF({rect.topRight(), QPointF(rect.right() - w, rect.top()), rect.bottomRight()}));
    }

    const int labelH = Theme::px(18);
    if (rect.width() > Theme::px(34)) {
        QPainterPath labelPath;
        labelPath.addRoundedRect(QRectF(rect.left(), rect.top(), rect.width(), labelH), radius, radius);
        p.fillPath(labelPath, QColor(0, 0, 0, 100));
        p.setPen(QColor("#ffffff"));
        QFont f = p.font();
        f.setPointSizeF(qMax(7.0, 8.2 * Theme::scale));
        f.setBold(true);
        p.setFont(f);
        QString text = clip.label();
        if (qAbs(clip.speed - 1.0) > 1e-3) text += QString("  %1x").arg(clip.speed, 0, 'g', 3);
        const QFontMetrics fm(f);
        p.drawText(QRectF(rect.left() + Theme::px(7), rect.top(), rect.width() - Theme::px(12), labelH),
                   Qt::AlignVCenter | Qt::AlignLeft,
                   fm.elidedText(text, Qt::ElideRight, int(rect.width() - Theme::px(12))));
    }
    p.restore();

    if (selected) {
        p.setPen(QPen(Theme::accent(), 2));
    } else if (m_hoverClip == clip.id) {
        p.setPen(QPen(QColor(255, 255, 255, 120), 1.2));
    } else {
        p.setPen(QPen(QColor(0, 0, 0, 130), 1));
    }
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    if ((selected || m_hoverClip == clip.id) && rect.width() > Theme::px(30)) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255, 200));
        const double hw = Theme::px(4);
        p.drawRoundedRect(QRectF(rect.left() + Theme::px(2), rect.top() + Theme::px(7), hw,
                                 rect.height() - Theme::px(14)), 2, 2);
        p.drawRoundedRect(QRectF(rect.right() - Theme::px(2) - hw, rect.top() + Theme::px(7), hw,
                                 rect.height() - Theme::px(14)), 2, 2);
    }
}

void TimelineWidget::drawFilmstrip(QPainter &p, const Clip &clip, const QRectF &rect)
{
    const QPixmap pm = sheet(clip.path);
    if (pm.isNull()) {
        p.fillRect(rect, QColor(255, 255, 255, 12));
        return;
    }
    const double tileW = pm.width() / 8.0;
    const double tileH = pm.height() / 8.0;
    const double drawH = rect.height() - Theme::px(18);
    if (drawH < 10 || tileH <= 0) return;
    const double drawW = tileW * (drawH / tileH);
    double x = rect.left();
    int guard = 0;
    while (x < rect.right() && guard < 400) {
        const double srcT = clip.srcIn + ((x - rect.left()) / qMax(1.0, m_pps)) * clip.speed;
        int index = qBound(0, int(qMax(0.0, srcT)), 63);
        const int col = index % 8;
        const int row = index / 8;
        p.drawPixmap(QRectF(x, rect.top() + Theme::px(18), drawW, drawH), pm,
                     QRectF(col * tileW, row * tileH, tileW, tileH));
        x += drawW;
        ++guard;
    }
}

void TimelineWidget::drawStill(QPainter &p, const Clip &clip, const QRectF &rect)
{
    const QPixmap pm = poster(clip.path);
    if (pm.isNull()) {
        p.fillRect(rect, QColor(255, 255, 255, 12));
        return;
    }
    const double drawH = rect.height() - Theme::px(18);
    const double drawW = pm.width() * (drawH / qMax(1, pm.height()));
    double x = rect.left();
    int guard = 0;
    while (x < rect.right() && guard < 200) {
        p.drawPixmap(QRectF(x, rect.top() + Theme::px(18), drawW, drawH), pm, QRectF(pm.rect()));
        x += drawW;
        ++guard;
    }
}

void TimelineWidget::drawWave(QPainter &p, const Clip &clip, const QRectF &rect, const QColor &color)
{
    const QVector<float> peaks = wave(clip.path);
    const double mid = rect.center().y() + Theme::px(6);
    const double half = qMax(4.0, (rect.height() - Theme::px(26)) / 2.0);
    if (peaks.isEmpty()) {
        p.setPen(QPen(color, 1));
        p.drawLine(QPointF(rect.left(), mid), QPointF(rect.right(), mid));
        return;
    }
    p.setPen(QPen(color, 1));
    const int total = peaks.size();
    const double srcSpan = qMax(0.001, clip.duration * clip.speed);
    for (double x = rect.left() + 1; x < rect.right() - 1; x += 2.0) {
        const double rel = (x - rect.left()) / qMax(1.0, rect.width());
        const double srcT = clip.srcIn + rel * srcSpan;
        int idx = int(srcT / qMax(0.001, clip.srcIn + srcSpan) * total);
        idx = qBound(0, idx, total - 1);
        const double amp = peaks[idx] * half;
        p.drawLine(QPointF(x, mid - amp), QPointF(x, mid + amp));
    }
}

void TimelineWidget::drawRuler(QPainter &p)
{
    const int rulerH = Theme::px(kRulerH);
    const int headerW = Theme::px(kHeaderW);
    p.fillRect(QRect(0, 0, width(), rulerH), Theme::panel());
    p.setPen(QPen(Theme::border(), 1));
    p.drawLine(0, rulerH - 1, width(), rulerH - 1);

    const QRect vp = viewport();
    double step = kSteps[0];
    for (double s : kSteps) {
        if (s * m_pps >= Theme::px(78)) { step = s; break; }
    }
    const double small = step / 5.0;

    p.save();
    p.setClipRect(QRect(headerW, 0, vp.width(), rulerH));
    QFont f = p.font();
    f.setPointSizeF(qMax(7.0, 8.4 * Theme::scale));
    p.setFont(f);
    double t = std::floor(m_scrollX / small) * small;
    while (t < m_scrollX + vp.width() / m_pps + small) {
        const double x = timeToX(t);
        const bool major = qAbs(t / step - qRound(t / step)) < 1e-6;
        p.setPen(Theme::muted());
        p.drawLine(QPointF(x, rulerH - Theme::px(major ? 10 : 5)), QPointF(x, rulerH - 1));
        if (major) {
            p.drawText(QPointF(x + Theme::px(5), rulerH - Theme::px(13)),
                       timeText(t, step >= 1 ? 0 : 2));
        }
        t += small;
    }
    p.restore();

    p.fillRect(QRect(0, 0, headerW, rulerH), Theme::panel());
    p.setPen(Theme::muted());
    QFont bold = p.font();
    bold.setPointSizeF(qMax(7.0, 8.8 * Theme::scale));
    bold.setBold(true);
    bold.setLetterSpacing(QFont::AbsoluteSpacing, 1.0);
    p.setFont(bold);
    p.drawText(QRect(Theme::px(14), 0, headerW - Theme::px(20), rulerH),
               Qt::AlignVCenter | Qt::AlignLeft, tr("TIMELINE"));
    p.setPen(QPen(Theme::border(), 1));
    p.drawLine(headerW - 1, 0, headerW - 1, height());
}

void TimelineWidget::drawHeaders(QPainter &p, const QVector<Row> &layout)
{
    const int headerW = Theme::px(kHeaderW);
    const int rulerH = Theme::px(kRulerH);
    p.fillRect(QRect(0, rulerH, headerW, height() - rulerH), Theme::panel());

    QFont f = p.font();
    f.setPointSizeF(qMax(7.0, 9.2 * Theme::scale));
    p.setFont(f);

    for (const Row &row : layout) {
        if (row.top + row.height < rulerH || row.top > height()) continue;
        p.save();
        p.setClipRect(QRect(0, rulerH, headerW, height() - rulerH));
        const QColor accent = row.track->kind == ClipKind::Video ? Theme::accent() : Theme::good();
        p.fillRect(QRect(Theme::px(8), row.top + Theme::px(10), Theme::px(4),
                         row.height - Theme::px(20)), accent);

        QFont bold = f;
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(row.track->hidden ? Theme::muted() : Theme::text());
        p.drawText(QRect(Theme::px(20), row.top, Theme::px(58), row.height),
                   Qt::AlignVCenter | Qt::AlignLeft, row.track->name);

        const int btn = Theme::px(20);
        const int gap = Theme::px(4);
        const int x0 = headerW - Theme::px(10) - 3 * (btn + gap);
        const int y = row.top + row.height / 2 - btn / 2;
        auto slotRect = [&](int i) { return QRect(x0 + i * (btn + gap), y, btn, btn); };
        auto drawToggle = [&](int slot, const QString &icon, bool active, const QColor &activeColor) {
            const QColor tint = active ? activeColor : Theme::muted();
            const QRect r = slotRect(slot);
            p.drawPixmap(r.x() + (r.width() - Theme::px(16)) / 2,
                         r.y() + (r.height() - Theme::px(16)) / 2,
                         Theme::icon(icon, tint, Theme::px(16)).pixmap(Theme::px(16), Theme::px(16)));
        };
        drawToggle(0, row.track->muted ? "speakerOff" : "speaker", row.track->muted, Theme::danger());
        if (row.track->kind == ClipKind::Video)
            drawToggle(1, row.track->hidden ? "eyeOff" : "eye", row.track->hidden, Theme::warn());
        drawToggle(2, row.track->locked ? "lock" : "unlock", row.track->locked, Theme::warn());
        p.restore();
    }

    p.setPen(QPen(Theme::border(), 1));
    p.drawLine(headerW - 1, rulerH, headerW - 1, height());
}

// -------------------------------------------------------------- transitions

QRectF TimelineWidget::transitionRect(const Transition &tr) const
{
    if (!m_project) return {};
    const Clip *to = const_cast<Project *>(m_project)->clip(tr.toClip);
    const Clip *from = const_cast<Project *>(m_project)->clip(tr.fromClip);
    if (!to || !from) return {};
    for (const Row &row : rows()) {
        if (row.track->id != tr.track) continue;
        const double x0 = timeToX(to->start);
        const double x1 = timeToX(from->end());
        const double w = qMax(double(Theme::px(18)), qMax(0.0, x1 - x0));
        return QRectF(x0, row.top + row.height / 2 - Theme::px(9), w, Theme::px(18));
    }
    return {};
}

void TimelineWidget::drawTransitions(QPainter &p)
{
    if (!m_project || m_project->transitions.isEmpty()) return;
    p.save();
    p.setClipRect(viewport());
    QFont f = p.font();
    f.setPointSizeF(qMax(6.5, 7.6 * Theme::scale));
    f.setBold(true);
    p.setFont(f);
    for (const Transition &trans : m_project->transitions) {
        const QRectF r = transitionRect(trans);
        if (r.isNull() || r.right() < viewport().left() || r.left() > viewport().right()) continue;
        QPainterPath path;
        path.addRoundedRect(r, Theme::px(5), Theme::px(5));
        QLinearGradient grad(r.topLeft(), r.bottomLeft());
        grad.setColorAt(0.0, Theme::accent().lighter(112));
        grad.setColorAt(1.0, Theme::accent().darker(118));
        p.fillPath(path, grad);
        p.setPen(QPen(QColor(0, 0, 0, 120), 1));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        if (r.width() > Theme::px(52)) {
            p.setPen(QColor("#ffffff"));
            p.drawText(r, Qt::AlignCenter, trans.type == "black" ? tr("Fade") : tr("Dissolve"));
        } else {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#ffffff"));
            const QPointF c = r.center();
            p.drawPolygon(QPolygonF({QPointF(c.x() - Theme::px(4), c.y()),
                                     QPointF(c.x(), c.y() - Theme::px(3)),
                                     QPointF(c.x() + Theme::px(4), c.y()),
                                     QPointF(c.x(), c.y() + Theme::px(3))}));
        }
    }
    p.restore();
}

QString TimelineWidget::transitionAt(const QPoint &pos) const
{
    if (!m_project) return {};
    for (int i = m_project->transitions.size() - 1; i >= 0; --i)
        if (transitionRect(m_project->transitions[i]).contains(pos))
            return m_project->transitions[i].id;
    return {};
}

void TimelineWidget::applyTransitionFades(Transition &tr)
{
    Clip *to = m_project ? m_project->clip(tr.toClip) : nullptr;
    Clip *from = m_project ? m_project->clip(tr.fromClip) : nullptr;
    if (!to || !from) return;
    const double d = qMax(0.05, tr.duration);
    if (tr.isSlide()) {
        to->fadeIn = 0.0;
        from->fadeOut = 0.0;
    } else if (tr.type == "black") {
        to->fadeIn = d;
        from->fadeOut = d;
    } else {
        to->fadeIn = d;
        from->fadeOut = 0.0;
    }
}

void TimelineWidget::addTransitionBetween(const QString &fromClipId, const QString &toClipId,
                                          const QString &type, double duration)
{
    if (!m_project) return;
    Clip *from = m_project->clip(fromClipId);
    Clip *to = m_project->clip(toClipId);
    if (!from || !to || from->track != to->track) return;
    if (m_project->transitionFor(toClipId)) return;

    duration = qBound(0.08, duration, qMin(from->duration, to->duration) * 0.9);
    if (duration < 0.08) return;

    emit undoPoint(tr("Add transition"));
    if (to->start > from->end() - duration + 1e-4) {
        const double shift = to->start - (from->end() - duration);
        for (Clip *c : m_project->clipsOn(to->track))
            if (c->start >= to->start - 1e-4) c->start = qMax(0.0, c->start - shift);
    }
    Transition transition;
    transition.track = from->track;
    transition.fromClip = fromClipId;
    transition.toClip = toClipId;
    transition.type = type;
    transition.duration = duration;
    m_project->transitions.append(transition);
    applyTransitionFades(m_project->transitions.last());
    m_project->sortClips();
    emit changed();
    emit announce(tr("Transition added"));
    update();
}

void TimelineWidget::showTransitionMenu(const QPoint &globalPos, const QString &id)
{
    if (!m_project) return;
    Transition *trans = nullptr;
    for (auto &t : m_project->transitions)
        if (t.id == id) { trans = &t; break; }
    if (!trans) return;

    const QVector<QPair<QString, QString>> types = {
        {tr("Dissolve"), "dissolve"},
        {tr("Fade through black"), "black"},
        {tr("Slide left"), "slideLeft"},
        {tr("Slide right"), "slideRight"},
        {tr("Slide up"), "slideUp"},
        {tr("Slide down"), "slideDown"},
    };

    QMenu menu(this);
    for (const auto &t : types) {
        auto *action = menu.addAction(t.first);
        action->setCheckable(true);
        action->setChecked(trans->type == t.second);
        const QString type = t.second;
        connect(action, &QAction::triggered, this, [this, id, type]() {
            for (auto &item : m_project->transitions) {
                if (item.id != id) continue;
                emit undoPoint(tr("Change transition"));
                item.type = type;
                applyTransitionFades(item);
                break;
            }
            emit changed();
            emit announce(tr("Transition changed"));
            update();
        });
    }
    menu.addSeparator();
    connect(menu.addAction(tr("Remove transition")), &QAction::triggered, this, [this, id]() {
        Transition copy;
        bool found = false;
        for (const auto &t : m_project->transitions)
            if (t.id == id) { copy = t; found = true; break; }
        if (!found) return;
        emit undoPoint(tr("Remove transition"));
        Clip *from = m_project->clip(copy.fromClip);
        Clip *to = m_project->clip(copy.toClip);
        if (from) from->fadeOut = 0.0;
        if (to) {
            to->fadeIn = 0.0;
            // restore the original spacing
            if (from && to->start < from->end() - 0.01) {
                for (Clip *c : m_project->clipsOn(to->track))
                    if (c->start >= to->start - 1e-4) c->start += copy.duration;
            }
        }
        m_project->removeTransition(id);
        m_project->sortClips();
        emit changed();
        emit announce(tr("Transition removed"));
        update();
    });
    menu.exec(globalPos);
}

void TimelineWidget::drawPlayhead(QPainter &p)
{
    const double x = timeToX(m_playhead);
    const int headerW = Theme::px(kHeaderW);
    if (x < headerW - 2 || x > width()) return;

    p.setPen(QPen(Theme::playhead(), 1.6));
    p.drawLine(QPointF(x, Theme::px(kRulerH) - 2), QPointF(x, height()));

    p.setPen(Qt::NoPen);
    p.setBrush(Theme::playhead());
    const int h = Theme::px(kRulerH);
    p.drawPolygon(QPolygonF({QPointF(x - Theme::px(7), h - Theme::px(13)),
                             QPointF(x + Theme::px(7), h - Theme::px(13)),
                             QPointF(x + Theme::px(7), h - Theme::px(4)),
                             QPointF(x, h + Theme::px(3)),
                             QPointF(x - Theme::px(7), h - Theme::px(4))}));

    QFont f = p.font();
    f.setPointSizeF(qMax(7.0, 8.0 * Theme::scale));
    f.setBold(true);
    p.setFont(f);
    const QString label = timeText(m_playhead, 2);
    const QFontMetrics fm(f);
    const int tw = fm.horizontalAdvance(label) + Theme::px(10);
    QRectF bubble(x + Theme::px(6), Theme::px(2), tw, Theme::px(18));
    if (bubble.right() > width()) bubble.moveLeft(x - tw - Theme::px(6));
    p.setBrush(QColor(Theme::playhead().red(), Theme::playhead().green(), Theme::playhead().blue(), 210));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(bubble, Theme::px(5), Theme::px(5));
    p.setPen(Qt::white);
    p.drawText(bubble, Qt::AlignCenter, label);
}

// ---------------------------------------------------------------- hit test

TimelineWidget::Hit TimelineWidget::hitTest(const QPoint &pos) const
{
    for (const Row &row : rows()) {
        if (pos.y() < row.top || pos.y() >= row.top + row.height) continue;
        auto clips = m_project->clipsOn(row.track->id);
        for (int i = clips.size() - 1; i >= 0; --i) {
            const QRectF r = clipRect(*clips[i]);
            if (!r.contains(pos.x(), pos.y())) continue;
            if (r.width() > Theme::px(26) && pos.x() <= r.left() + Theme::px(7)) return {clips[i], "left"};
            if (r.width() > Theme::px(26) && pos.x() >= r.right() - Theme::px(7)) return {clips[i], "right"};
            return {clips[i], "body"};
        }
    }
    return {};
}

// ------------------------------------------------------------------ events

void TimelineWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int bar = Theme::px(12);
    m_hbar->setGeometry(Theme::px(kHeaderW), height() - bar, qMax(10, width() - Theme::px(kHeaderW) - bar), bar);
    m_vbar->setGeometry(width() - bar, Theme::px(kRulerH), bar, qMax(10, height() - Theme::px(kRulerH) - bar));
    updateScrollBars();
}

void TimelineWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) {
        updateScrollBars();
        update();
    }
}

void TimelineWidget::focusInEvent(QFocusEvent *event)
{
    m_focusRing = true;
    update();
    QWidget::focusInEvent(event);
}

void TimelineWidget::focusOutEvent(QFocusEvent *event)
{
    m_focusRing = false;
    update();
    QWidget::focusOutEvent(event);
}

void TimelineWidget::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (event->modifiers() & Qt::ControlModifier) {
        setZoom(m_pps * (delta > 0 ? 1.18 : 1 / 1.18), event->position().x());
    } else if (event->modifiers() & Qt::ShiftModifier) {
        m_scrollY = qMax(0, m_scrollY - delta / 2);
        updateScrollBars();
        update();
    } else {
        m_scrollX = qMax(0.0, m_scrollX - (delta / 120.0) * 90.0 / m_pps);
        updateScrollBars();
        update();
    }
    event->accept();
}

void TimelineWidget::mousePressEvent(QMouseEvent *event)
{
    if (!m_project) return;
    setFocus();
    const QPoint pos = event->position().toPoint();
    const int headerW = Theme::px(kHeaderW);
    const int rulerH = Theme::px(kRulerH);

    if (event->button() == Qt::MiddleButton) {
        m_dragging = true;
        m_drag = {};
        m_drag.mode = "pan";
        m_drag.startPos = pos;
        m_drag.originals.insert("scroll", {m_scrollX});
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() == Qt::RightButton) {
        contextMenu(pos);
        return;
    }
    if (pos.y() < rulerH) {
        emit seek(qMax(0.0, xToTime(pos.x())));
        m_dragging = true;
        m_drag = {};
        m_drag.mode = "playhead";
        return;
    }
    if (pos.x() < headerW) {
        const Row row = rowFor(pos);
        if (!row.track) return;
        const int btn = Theme::px(20);
        const int gap = Theme::px(4);
        const int x0 = headerW - Theme::px(10) - 3 * (btn + gap);
        const int y = row.top + row.height / 2 - btn / 2;
        const bool isVideo = row.track->kind == ClipKind::Video;
        if (QRect(x0, y, btn, btn).contains(pos)) {
            emit undoPoint(tr("Toggle mute"));
            row.track->muted = !row.track->muted;
            emit changed();
            emit announce(row.track->muted ? tr("Track muted") : tr("Track unmuted"));
            update();
            return;
        }
        if (QRect(x0 + (btn + gap), y, btn, btn).contains(pos)) {
            if (!isVideo) return;
            emit undoPoint(tr("Toggle visibility"));
            row.track->hidden = !row.track->hidden;
            emit changed();
            emit announce(row.track->hidden ? tr("Track hidden") : tr("Track visible"));
            update();
            return;
        }
        if (QRect(x0 + 2 * (btn + gap), y, btn, btn).contains(pos)) {
            emit undoPoint(tr("Toggle lock"));
            row.track->locked = !row.track->locked;
            emit changed();
            emit announce(row.track->locked ? tr("Track locked") : tr("Track unlocked"));
            update();
            return;
        }
        return;
    }

    const QString trHit = transitionAt(pos);
    if (!trHit.isEmpty()) {
        showTransitionMenu(mapToGlobal(pos), trHit);
        return;
    }

    const Hit hit = hitTest(pos);
    if (!hit.clip) {
        if (!(event->modifiers() & Qt::ControlModifier)) clearSelection();
        m_dragging = true;
        m_drag = {};
        m_drag.mode = "rubber";
        m_drag.startPos = pos;
        m_band = QRect(pos, pos);
        return;
    }

    const bool additive = event->modifiers() & Qt::ControlModifier;
    if (additive) selectIds({hit.clip->id}, true);
    else if (!m_selected.contains(hit.clip->id)) selectIds({hit.clip->id}, false);
    if (!m_selected.contains(hit.clip->id)) return;

    const Row row = rowFor(pos);
    if (row.track && row.track->locked) {
        setToolTip(tr("Track is locked"));
        return;
    }

    m_dragging = true;
    m_drag = {};
    m_drag.mode = hit.zone;
    m_drag.startPos = pos;
    m_drag.clipId = hit.clip->id;
    for (Clip *c : selectedClips())
        m_drag.originals.insert(c->id, {c->start, c->duration, c->srcIn});
    setCursor(hit.zone == "body" ? Qt::OpenHandCursor : Qt::SizeHorCursor);
}

void TimelineWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_project) return;
    const QPoint pos = event->position().toPoint();

    if (!m_dragging) {
        const Hit hit = hitTest(pos);
        const QString newHover = hit.clip ? hit.clip->id : QString();
        if (newHover != m_hoverClip || hit.zone != m_hoverZone) {
            m_hoverClip = newHover;
            m_hoverZone = hit.zone;
            update();
        }
        if (pos.y() < Theme::px(kRulerH)) setCursor(Qt::PointingHandCursor);
        else if (hit.clip) setCursor(hit.zone == "body" ? Qt::OpenHandCursor : Qt::SizeHorCursor);
        else unsetCursor();
        return;
    }

    if (m_drag.mode == "pan") {
        const double dx = (pos.x() - m_drag.startPos.x()) / m_pps;
        m_scrollX = qMax(0.0, m_drag.originals.value("scroll").value(0) - dx);
        updateScrollBars();
        update();
        return;
    }
    if (m_drag.mode == "playhead") {
        emit seek(qMax(0.0, xToTime(pos.x())));
        return;
    }
    if (m_drag.mode == "rubber") {
        m_band = QRect(m_drag.startPos, pos).normalized();
        QStringList hits;
        for (const Row &row : rows())
            for (Clip *clip : m_project->clipsOn(row.track->id))
                if (clipRect(*clip).intersects(QRectF(m_band))) hits << clip->id;
        m_selected = QSet<QString>(hits.begin(), hits.end());
        update();
        return;
    }

    autoScroll(pos);
    if (!m_drag.pushed) {
        emit undoPoint(m_drag.mode == "body" ? tr("Move clip") : tr("Trim clip"));
        m_drag.pushed = true;
    }

    if (m_drag.mode == "body") dragMove(pos);
    else if (m_drag.mode == "left") trimLeft(pos, (pos.x() - m_drag.startPos.x()) / m_pps);
    else if (m_drag.mode == "right") trimRight((pos.x() - m_drag.startPos.x()) / m_pps);

    updateScrollBars();
    update();
}

void TimelineWidget::autoScroll(const QPoint &pos)
{
    const QRect vp = viewport();
    if (pos.x() > vp.right() - 20) m_scrollX += 8 / m_pps;
    else if (pos.x() < vp.left() + 20) m_scrollX = qMax(0.0, m_scrollX - 8 / m_pps);
}

void TimelineWidget::dragMove(const QPoint &pos)
{
    Clip *primary = m_project->clip(m_drag.clipId);
    if (!primary) return;
    const auto orig = m_drag.originals.value(primary->id);
    const double start0 = orig.value(0);
    const double dur0 = orig.value(1);
    double newStart = qMax(0.0, start0 + (pos.x() - m_drag.startPos.x()) / m_pps);
    newStart = snapTime(newStart, dur0, primary->id);
    const double delta = newStart - start0;

    for (auto it = m_drag.originals.constBegin(); it != m_drag.originals.constEnd(); ++it) {
        if (it.key() == "scroll") continue;
        if (Clip *c = m_project->clip(it.key())) c->start = qMax(0.0, it.value().value(0) + delta);
    }

    if (m_drag.originals.size() == 1) {
        const Row row = rowFor(pos);
        if (row.track && row.track->id != primary->track && !row.track->locked) {
            const bool ok = (row.track->kind == ClipKind::Audio && primary->kind == ClipKind::Audio)
                    || (row.track->kind == ClipKind::Video && primary->kind != ClipKind::Audio);
            if (ok) primary->track = row.track->id;
        }
    }
    m_project->sortClips();
    emit changed();
}

void TimelineWidget::trimLeft(const QPoint &, double dt)
{
    Clip *clip = m_project->clip(m_drag.clipId);
    if (!clip) return;
    const auto orig = m_drag.originals.value(clip->id);
    const double s0 = orig.value(0);
    const double d0 = orig.value(1);
    const double src0 = orig.value(2);
    const double speed = qMax(0.05, clip->speed);
    double newStart = qMax(0.0, s0 + dt);
    newStart = qMin(newStart, s0 + d0 - kMinDur);
    double delta = newStart - s0;
    double newSrc = src0 + delta * speed;
    if (newSrc < 0) {
        newSrc = 0;
        newStart = s0 - src0 / speed;
        delta = newStart - s0;
    }
    clip->start = newStart;
    clip->srcIn = qMax(0.0, newSrc);
    clip->duration = qMax(kMinDur, d0 - delta);
    emit changed();
}

void TimelineWidget::trimRight(double dt)
{
    Clip *clip = m_project->clip(m_drag.clipId);
    if (!clip) return;
    const auto orig = m_drag.originals.value(clip->id);
    const double d0 = orig.value(1);
    const double src0 = orig.value(2);
    const double speed = qMax(0.05, clip->speed);
    double newDur = qMax(kMinDur, d0 + dt);
    if (clip->srcDur > 0) newDur = qMin(newDur, qMax(kMinDur, (clip->srcDur - src0) / speed));
    if (snapEnabled) {
        const double end = snapTime(clip->start + newDur, 0, clip->id, true);
        newDur = qMax(kMinDur, end - clip->start);
    }
    clip->duration = newDur;
    emit changed();
}

double TimelineWidget::snapTime(double start, double duration, const QString &skipId, bool snapEnd)
{
    m_snapLine = -1;
    if (!snapEnabled || !m_project) return start;
    const double threshold = Theme::px(9) / m_pps;
    QVector<double> candidates{0.0, m_playhead, m_project->duration()};
    for (const Clip &c : m_project->clips) {
        if (c.id == skipId) continue;
        candidates << c.start << c.end();
    }
    double best = start;
    double bestDist = threshold;
    for (double cand : candidates) {
        const double dist = qAbs(cand - start);
        if (dist < bestDist) { best = cand; bestDist = dist; }
    }
    if (!snapEnd && duration > 0) {
        for (double cand : candidates) {
            const double dist = qAbs(cand - (start + duration));
            if (dist < bestDist) { best = cand - duration; bestDist = dist; }
        }
    }
    if (bestDist < threshold) {
        m_snapLine = snapEnd ? best + duration : best;
        return qMax(0.0, best);
    }
    return start;
}

void TimelineWidget::mouseReleaseEvent(QMouseEvent *)
{
    if (!m_dragging) return;
    const QString mode = m_drag.mode;
    m_dragging = false;
    m_snapLine = -1;
    unsetCursor();
    if (mode == "rubber") {
        updateA11y();
        emit selectionChanged();
    } else if (mode != "pan" && mode != "playhead") {
        if (m_project) m_project->sortClips();
        emit changed();
        updateScrollBars();
    }
    m_drag = {};
    update();
}

void TimelineWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    const Hit hit = hitTest(event->position().toPoint());
    if (hit.clip) emit clipActivated(hit.clip->id);
}

void TimelineWidget::contextMenu(const QPoint &pos)
{
    if (!m_project) return;
    const QString trHit = transitionAt(pos);
    if (!trHit.isEmpty()) {
        showTransitionMenu(mapToGlobal(pos), trHit);
        return;
    }
    const Hit hit = hitTest(pos);
    QMenu menu(this);
    if (hit.clip) {
        if (!m_selected.contains(hit.clip->id)) selectIds({hit.clip->id});
        menu.addAction(tr("Split at playhead"), this, &TimelineWidget::splitSelected);
        menu.addAction(tr("Duplicate"), this, &TimelineWidget::duplicateSelected);
        menu.addSeparator();
        menu.addAction(tr("Delete"), this, [this]() { deleteSelected(false); });
        menu.addAction(tr("Ripple delete"), this, [this]() { deleteSelected(true); });
        auto clips = m_project->clipsOn(hit.clip->track);
        Clip *next = nullptr;
        for (int i = 0; i + 1 < clips.size(); ++i)
            if (clips[i]->id == hit.clip->id) next = clips[i + 1];
        if (next && next->start <= hit.clip->end() + 0.02
            && !m_project->transitionFor(next->id)) {
            menu.addSeparator();
            const QString fromId = hit.clip->id;
            const QString toId = next->id;
            menu.addAction(tr("Add transition"), this, [this, fromId, toId]() {
                addTransitionBetween(fromId, toId);
            });
        }
    } else {
        menu.addAction(tr("Add video track"), this, [this]() {
            emit undoPoint(tr("Add track"));
            m_project->addTrack(ClipKind::Video);
            emit changed();
            update();
        });
        menu.addAction(tr("Add audio track"), this, [this]() {
            emit undoPoint(tr("Add track"));
            m_project->addTrack(ClipKind::Audio);
            emit changed();
            update();
        });
        menu.addSeparator();
        menu.addAction(tr("Zoom to fit"), this, &TimelineWidget::zoomFit);
    }
    menu.exec(mapToGlobal(pos));
}

// ---------------------------------------------------------------- commands

void TimelineWidget::splitSelected()
{
    if (!m_project) return;
    const double t = m_playhead;
    QVector<Clip> copies;
    for (Clip *c : selectedClips())
        if (c->start < t && t < c->end()) copies << *c;
    if (copies.isEmpty()) return;
    emit undoPoint(tr("Split clip"));
    for (const Clip &source : copies) {
        Clip right = source;
        right.id = newId("clip");
        right.start = t;
        right.duration = source.end() - t;
        right.srcIn = source.srcIn + (t - source.start) * qMax(0.05, source.speed);
        if (Clip *left = m_project->clip(source.id)) left->duration = t - source.start;
        m_project->clips.append(right);
    }
    m_project->sortClips();
    emit changed();
    emit announce(tr("Clip split at %1").arg(timeText(t, 2)));
    update();
}

void TimelineWidget::duplicateSelected()
{
    if (!m_project) return;
    QVector<Clip> copies;
    for (Clip *c : selectedClips()) copies << *c;
    if (copies.isEmpty()) return;
    emit undoPoint(tr("Duplicate clip"));
    QStringList ids;
    for (Clip copy : copies) {
        copy.id = newId("clip");
        copy.start = copy.end();
        m_project->clips.append(copy);
        ids << copy.id;
    }
    m_project->sortClips();
    selectIds(ids);
    emit changed();
    emit announce(tr("Clip duplicated"));
    update();
}

void TimelineWidget::deleteSelected(bool ripple)
{
    if (!m_project) return;
    QVector<Clip> copies;
    for (Clip *c : selectedClips()) copies << *c;
    if (copies.isEmpty()) return;
    emit undoPoint(tr("Delete clip"));
    for (const Clip &c : copies) {
        const QString trackId = c.track;
        const double end = c.end();
        const double dur = c.duration;
        m_project->removeClip(c.id);
        if (ripple) {
            for (Clip *other : m_project->clipsOn(trackId))
                if (other->start >= end - 1e-6) other->start = qMax(0.0, other->start - dur);
        }
    }
    clearSelection();
    emit changed();
    emit announce(tr("Clip deleted"));
    update();
}

// ---------------------------------------------------------------- dnd

void TimelineWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasFormat("application/x-videocut-media")) event->acceptProposedAction();
    else event->ignore();
}

void TimelineWidget::dragMoveEvent(QDragMoveEvent *event)
{
    event->acceptProposedAction();
}

void TimelineWidget::dropEvent(QDropEvent *event)
{
    if (!event->mimeData()->hasFormat("application/x-videocut-media")) return;
    const QString raw = QString::fromUtf8(event->mimeData()->data("application/x-videocut-media"));
    const QStringList paths = raw.split('\n', Qt::SkipEmptyParts);
    const QPoint pos = event->position().toPoint();
    const Row row = rowFor(pos);
    if (!row.track) return;
    double t = qMax(0.0, xToTime(pos.x()));
    if (snapEnabled) {
        t = snapTime(t, 0, QString());
        m_snapLine = -1;
    }
    emit mediaDropped(paths, row.track->id, t);
    event->acceptProposedAction();
}

// ---------------------------------------------------------------- a11y

void TimelineWidget::updateA11y()
{
    if (!m_project) return;
    const int count = m_selected.size();
    QString desc;
    if (count == 0) desc = tr("No clip selected. %1 clips on %2 tracks.")
                               .arg(m_project->clips.size()).arg(m_project->tracks.size());
    else {
        QStringList names;
        for (Clip *c : selectedClips()) names << c->label();
        desc = tr("%1 of %2 clips selected: %3")
                   .arg(count).arg(m_project->clips.size()).arg(names.join(", "));
    }
    setAccessibleDescription(desc);
    emit announce(desc);
}
