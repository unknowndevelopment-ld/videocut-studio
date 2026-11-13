#pragma once

#include <QDialog>

#include "Project.h"
#include "Renderer.h"

class QLineEdit;
class QComboBox;
class QSlider;
class QLabel;
class QProgressBar;
class QPushButton;

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportDialog(const Project &project, QWidget *parent = nullptr);

private:
    RenderOptions options() const;
    void updateSummary();
    void browse();
    void startExport();
    void onFormatChanged();
    void setBusy(bool busy);
    void openFolder();

    Project m_project;
    Renderer *m_renderer = nullptr;
    QString m_output;

    QLineEdit *m_path = nullptr;
    QComboBox *m_resolution = nullptr;
    QComboBox *m_fps = nullptr;
    QComboBox *m_format = nullptr;
    QSlider *m_quality = nullptr;
    QLabel *m_qualityLabel = nullptr;
    QLabel *m_summary = nullptr;
    QLabel *m_status = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_export = nullptr;
    QPushButton *m_close = nullptr;
    QPushButton *m_open = nullptr;
};
