#pragma once

#include <QObject>
#include <QProcess>
#include <QStringList>

#include "Project.h"

struct RenderOptions {
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    int crf = 20;
    QString preset = "medium";
    QString format = "mp4";
    QString audioBitrate = "192k";
    double maxDuration = 0.0;

    RenderOptions scaledTo(int maxWidth) const;
};

class Renderer : public QObject {
    Q_OBJECT
public:
    explicit Renderer(QObject *parent = nullptr);
    ~Renderer() override;

    static QString textPng(const Clip &clip);
    static QImage textFrame(const Clip &clip, double progress);
    static QString textSequence(const Clip &clip, double fps, double duration);
    static double textAnimationTime() { return 0.55; }

    static QImage shapeFrame(const Clip &clip);
    static QString shapePng(const Clip &clip);
    static QStringList buildCommand(const Project &project, const QString &outPath,
                                    const RenderOptions &opts, double *totalSeconds);
    static QString estimateSize(const Project &project, const RenderOptions &opts);

    void start(const Project &project, const QString &outPath, const RenderOptions &opts);
    void cancel();
    bool isRunning() const;

signals:
    void progress(double done, double total);
    void finished(const QString &path);
    void failed(const QString &message);

private:
    void onStdout();
    void onFinished(int exitCode, QProcess::ExitStatus status);

    QProcess *m_proc = nullptr;
    QByteArray m_buffer;
    double m_total = 0.0;
    QString m_outPath;
    bool m_cancelled = false;
};
