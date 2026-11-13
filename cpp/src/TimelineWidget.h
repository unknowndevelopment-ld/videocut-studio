#pragma once

#include <QHash>
#include <QPixmap>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "Project.h"

class QScrollBar;

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(QWidget *parent = nullptr);

    void setProject(Project *project);
    Project *project() const { return m_project; }

    double playhead() const { return m_playhead; }
    void setPlayhead(double t, bool follow = false);
    void ensureVisible(double t);

    QStringList selectedIds() const;
    QVector<Clip *> selectedClips() const;
    void selectIds(const QStringList &ids, bool additive = false);
    void clearSelection();
    void addTransitionBetween(const QString &fromClipId, const QString &toClipId,
                              const QString &type = "dissolve", double duration = 0.5);
    void setTransitionType(const QString &toClipId, const QString &type);

    void setZoom(double pps, double anchorX = -1);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    double pixelsPerSecond() const { return m_pps; }

    void splitSelected();
    void deleteSelected(bool ripple);
    void duplicateSelected();
    void refresh();

    bool snapEnabled = true;
    QRectF clipRectForTest(const Clip &clip) const { return clipRect(clip); }
    int junctionCountForTest() const { return junctions().size(); }
    QRectF junctionRectForTest(int index) const { return junctions().value(index).rect; }

signals:
    void seek(double seconds);
    void changed();
    void undoPoint(const QString &label);
    void selectionChanged();
    void clipActivated(const QString &id);
    void mediaDropped(const QStringList &paths, const QString &trackId, double at);
    void announce(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Row {
        Track *track = nullptr;
        int top = 0;
        int height = 0;
    };
    struct Hit {
        Clip *clip = nullptr;
        QString zone;
    };

    QVector<Row> rows() const;
    QRect viewport() const;
    Row rowFor(const QPoint &pos) const;
    double timeToX(double t) const;
    double xToTime(double x) const;
    QRectF clipRect(const Clip &clip) const;
    Hit hitTest(const QPoint &pos) const;
    double snapTime(double start, double duration, const QString &skipId, bool snapEnd = false);
    void updateScrollBars();
    void drawClip(QPainter &p, const Clip &clip, const QRectF &rect, const Track &track);
    void drawFilmstrip(QPainter &p, const Clip &clip, const QRectF &rect);
    void drawStill(QPainter &p, const Clip &clip, const QRectF &rect);
    void drawWave(QPainter &p, const Clip &clip, const QRectF &rect, const QColor &color);
    void drawRuler(QPainter &p);
    void drawHeaders(QPainter &p, const QVector<Row> &rows);
    void drawPlayhead(QPainter &p);
    void updateA11y();
    void autoScroll(const QPoint &pos);
    void dragMove(const QPoint &pos);
    void trimLeft(const QPoint &pos, double dt);
    void trimRight(double dt);
    void contextMenu(const QPoint &pos);
    struct Junction {
        QString fromId;
        QString toId;
        QRectF rect;
    };
    QVector<Junction> junctions() const;
    bool junctionAt(const QPoint &pos, QString *fromId = nullptr, QString *toId = nullptr) const;
    QRectF transitionRect(const Transition &tr) const;
    QString transitionAt(const QPoint &pos) const;
    void applyTransitionFades(Transition &tr);
    void showTransitionMenu(const QPoint &globalPos, const QString &transitionId);
    void drawTransitions(QPainter &p);
    void drawJunctions(QPainter &p);
    QPixmap sheet(const QString &path);
    QPixmap poster(const QString &path);
    QVector<float> wave(const QString &path);

    Project *m_project = nullptr;
    double m_pps = 42.0;
    double m_scrollX = 0.0;
    int m_scrollY = 0;
    double m_playhead = 0.0;
    QSet<QString> m_selected;

    QScrollBar *m_hbar = nullptr;
    QScrollBar *m_vbar = nullptr;

    struct Drag {
        QString mode;
        QPoint startPos;
        QString clipId;
        QHash<QString, QVector<double>> originals;
        bool pushed = false;
        QString trackKind;
    } m_drag;
    bool m_dragging = false;

    QString m_hoverClip;
    QString m_hoverZone;
    QString m_hoverJunction;
    double m_snapLine = -1;
    QRect m_band;
    bool m_focusRing = false;

    QHash<QString, QPixmap> m_sheets;
    QHash<QString, QPixmap> m_posters;
    QHash<QString, QVector<float>> m_waves;
    QSet<QString> m_pending;
};
