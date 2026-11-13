#pragma once

#include <QWidget>

#include "Payload.h"

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;

class SetupWindow : public QWidget {
    Q_OBJECT
public:
    explicit SetupWindow(const QString &payloadPath, QWidget *parent = nullptr);
    ~SetupWindow() override;

    int runSilent();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    enum class State { Welcome, Working, Done, Failed };

    void setState(State state);
    void install();
    void writeRegistry();
    void makeShortcuts();
    void showError(const QString &message);
    void setProgress(qint64 done, qint64 total);
    void launchApp();
    QString installDir() const;

    QString m_payloadPath;
    Payload m_payload;
    QString m_version;
    qint64 m_total = 0;
    bool m_working = false;
    QPoint m_dragOrigin;

    QLabel *m_logo = nullptr;
    QLabel *m_title = nullptr;
    QLabel *m_subtitle = nullptr;
    QLabel *m_path = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_license = nullptr;
    QCheckBox *m_accept = nullptr;
    QCheckBox *m_desktop = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_install = nullptr;
    QPushButton *m_close = nullptr;
    QPushButton *m_launch = nullptr;
    QPushButton *m_browse = nullptr;
};
