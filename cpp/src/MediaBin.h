#pragma once

#include <QWidget>

class QListWidget;
class QListWidgetItem;
class QLabel;
class Project;

class MediaBin : public QWidget {
    Q_OBJECT
public:
    explicit MediaBin(QWidget *parent = nullptr);

    void setProject(Project *project);
    void rebuild();
    void addPaths(const QStringList &paths);
    QStringList selectedPaths() const;
    int count() const;

signals:
    void addRequested(const QStringList &paths);
    void importRequested();
    void mediaAdded(const QStringList &paths);

private:
    void refreshIcons();
    void showContextMenu(const QPoint &pos);
    QIcon iconFor(const QString &path);

    Project *m_project = nullptr;
    QListWidget *m_list = nullptr;
    QLabel *m_hint = nullptr;
};
