#pragma once

#include <QFrame>
#include <QImage>

#include "PreviewEngine.h"

class QLabel;
class QSlider;
class QPushButton;

class PreviewSurface : public QWidget {
    Q_OBJECT
public:
    explicit PreviewSurface(QWidget *parent = nullptr);
    void setImage(const QImage &image);
    void setMessage(const QString &message);

signals:
    void resized();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QImage m_image;
    QString m_message;
};

class PreviewPanel : public QFrame {
    Q_OBJECT
public:
    explicit PreviewPanel(QWidget *parent = nullptr);

    PreviewEngine *engine() const { return m_engine; }

    void setDuration(double seconds);
    void setTimeLabel(double seconds);
    void showMessage(const QString &text);
    QSize frameSize() const;
    QSize surfaceSize() const;
    void refreshSize();

signals:
    void playRequested();
    void positionChanged(double seconds);
    void playingChanged(bool playing);

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
