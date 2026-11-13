#pragma once

#include <QFrame>
#include <QVector>
#include <functional>

#include "Project.h"

class QVBoxLayout;
class QScrollArea;
class QLabel;
class QDoubleSpinBox;
class QSlider;
class QComboBox;
class QCheckBox;

class SliderRow : public QWidget {
    Q_OBJECT
public:
    SliderRow(double lo, double hi, int decimals, const QString &suffix, QWidget *parent = nullptr);
    double value() const;
    void setValue(double value);

signals:
    void valueChanged(double value);

private:
    QSlider *m_slider = nullptr;
    QDoubleSpinBox *m_spin = nullptr;
    int m_decimals = 2;
    double m_factor = 100.0;
    bool m_guard = false;
};

class PropertiesPanel : public QFrame {
    Q_OBJECT
public:
    explicit PropertiesPanel(QWidget *parent = nullptr);

    void setProject(Project *project);
    void setClips(const QVector<Clip *> &clips);
    void syncValues();

signals:
    void changed();
    void undoPoint(const QString &label, const QString &key);

private:
    void rebuild();
    void clearForm();
    void buildProjectForm();
    void buildClipForm();
    void buildTextForm(Clip *clip);
    void buildShapeForm(Clip *clip);
    void addHeader(const QString &text);
    void addSection(const QString &text);
    void addRow(const QString &label, QWidget *field);
    void addWidgetRow(QWidget *widget);
    void applyToClips(const QString &label, const QString &key, const std::function<void(Clip &)> &fn);
    Clip *first() const { return m_clips.isEmpty() ? nullptr : m_clips.first(); }
    QString idsKey() const;

    Project *m_project = nullptr;
    QVector<Clip *> m_clips;
    QScrollArea *m_scroll = nullptr;
    QWidget *m_content = nullptr;
    QVBoxLayout *m_form = nullptr;
    QLabel *m_title = nullptr;
    QVector<std::function<void()>> m_syncers;
    bool m_syncing = false;
};
