#pragma once

#include <QFrame>
#include <QImage>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QStringList>

#include "PreviewEngine.h"
#include "Project.h"

class QLabel;
class QSlider;
class QPushButton;

class PreviewSurface : public QWidget {
    Q_OBJECT
public:
    explicit PreviewSurface(QWidget *parent = nullptr);
    void setImage(const QImage &image);
    void setMessage(const QString &message);
    void setEngine(PreviewEngine *engine) { m_engine = engine; }
    void setProject(Project *project) { m_project = project; }
    void setSelection(const QStringList &ids);
    // Corner handles plus the rotation handle, in widget coordinates.
    QVector<QPointF> handlesFor(const QString &clipId) const;

signals:
    void resized();
    void selectionRequested(const QString &id);
    void transformStarted(const QString &label, const QString &key);
    void transformChanged();
    void transformFinished();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    enum class DragMode { None, Move, Scale, Rotate };

    QRectF imageRect() const;
    QPointF toProject(const QPointF &surface) const;
    QPointF toSurface(const QPointF &project) const;
    QPolygonF boxCorners(const PreviewEngine::ClipBox &box) const;
    QPointF rotateHandle(const PreviewEngine::ClipBox &box) const;
    int handleAt(const PreviewEngine::ClipBox &box, const QPointF &pos) const;
    bool boxContains(const PreviewEngine::ClipBox &box, const QPointF &pos) const;
    void drawOverlay(QPainter &p);
    void beginDrag(int handle, const PreviewEngine::ClipBox &box, const QPointF &pos);
    void applyDrag(const QPointF &pos, Qt::KeyboardModifiers modifiers);
    void endDrag();
    void updateHoverCursor(const QPointF &pos);

    QImage m_image;
    QString m_message;
    PreviewEngine *m_engine = nullptr;
    Project *m_project = nullptr;
    QStringList m_selection;

    DragMode m_drag = DragMode::None;
    bool m_dragArmed = false;
    QString m_dragId;
    QPointF m_pressSurface;
    QPointF m_pressProject;
    QPointF m_center;
    QSizeF m_boxSize;
    double m_startScale = 1.0;
    double m_startRotation = 0.0;
    double m_startX = 0.0;
    double m_startY = 0.0;
    double m_startPosX = 0.5;
    double m_startPosY = 0.5;
    double m_startAngle = 0.0;
    double m_startDistance = 1.0;
    int m_dragSerial = 0;
};

class PreviewPanel : public QFrame {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget *parent = nullptr);

    PreviewEngine *engine() const { return m_engine; }

    void setDuration(double seconds);
    void setTimeLabel(double seconds);
    void showMessage(const QString &text);
    void setProject(Project *project);
    void setSelection(const QStringList &ids);
    QSize frameSize() const;
    QSize surfaceSize() const;
    void refreshSize();

signals:
    void playRequested();
    void positionChanged(double seconds);
    void playingChanged(bool playing);
    void clipSelected(const QString &id);
    void transformStarted(const QString &label, const QString &key);
    void transformChanged();
    void transformFinished();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateTimeLabel(double seconds);
    void onPlayClicked();

    PreviewEngine *m_engine = nullptr;
    PreviewSurface *m_surface = nullptr;
    QPushButton *m_play = nullptr;
    QLabel *m_time = nullptr;
    QLabel *m_state = nullptr;
    QSlider *m_volume = nullptr;
    QPushButton *m_mute = nullptr;
    double m_duration = 0.0;
    double m_lastVolume = 0.9;
};
