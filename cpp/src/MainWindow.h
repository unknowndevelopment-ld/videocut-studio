#pragma once

#include <QMainWindow>
#include <QStringList>

#include "Project.h"

class MediaBin;
class PreviewPanel;
class PropertiesPanel;
class TimelineWidget;
class Renderer;
class ClipListDialog;
class Updater;
class QLabel;
class QAction;
class QPushButton;
class QSplitter;
class QMenu;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

    void openFiles(const QStringList &paths);
    bool openProjectFile(const QString &path);
    void checkRecovery();
    void runSelfTest(const QString &outDir);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void buildUi();
    void createActions();
    void createMenus();
    void wireSignals();
    void restoreSettings();
    void saveSettings();

    void pushUndo(const QString &label, const QString &key = QString());
    void updateTitle();
    void updateStatus();
    void updateActions();
    void announce(const QString &text);
    void markDirty();

    void newProject();
    void openProject();
    bool saveProject();
    bool saveProjectAs();
    bool maybeSave();
    void loadProject(Project project, const QString &path, bool recovered = false);
    QString recoveryPath() const;
    void scheduleAutosave();
    void writeAutosave();
    void clearRecovery();
    void importMedia();
    void exportVideo();

    void addMediaToTimeline(const QStringList &paths, const QString &trackId = QString(),
                            double at = -1.0);
    Track *pickTrack(ClipKind kind);
    double appendStart(const QString &trackId) const;
    void addText();
    void addShape(const QString &type);
    QMenu *createShapeMenu(QWidget *parent);

    void splitClip();
    void deleteClips(bool ripple);
    void duplicateClips();
    void copyClips();
    void cutClips();
    void pasteClips();
    void selectAll();
    void undo();
    void redo();
    void afterHistory(const QString &label, const QString &verb);

    void onProjectChanged();
    void onSelectionChanged();
    void onSeek(double seconds);
    void onPlayToggled(bool playing);
    void onPlayerPosition(double seconds);
    void togglePlay();
    bool sourceAt(double t, QString *path, double *seek) const;

    void setInterfaceScale(double scale);
    void toggleHighContrast();
    void showClipList();
    void showShortcuts();
    void showAbout();
    void checkForUpdates(bool interactive);
    void startUpdateDownload();
    void openLegal(const QString &fileName);
    void updateA11ySummary();
    bool textFieldFocused() const;

    Project m_project;
    History m_history;
    QString m_projectPath;
    bool m_dirty = false;
    bool m_playing = false;
    QStringList m_clipboard;

    MediaBin *m_mediaBin = nullptr;
    PreviewPanel *m_preview = nullptr;
    PropertiesPanel *m_properties = nullptr;
    TimelineWidget *m_timeline = nullptr;
    QSplitter *m_mainSplitter = nullptr;
    QSplitter *m_upperSplitter = nullptr;
    QSplitter *m_verticalSplitter = nullptr;
    ClipListDialog *m_clipList = nullptr;

    QLabel *m_status = nullptr;
    QLabel *m_info = nullptr;
    QAction *m_actSnap = nullptr;
    QPushButton *m_btnSnap = nullptr;
    double m_scale = 1.0;
    bool m_contrast = false;
    QAction *m_actionUndo = nullptr;
    QAction *m_actionRedo = nullptr;
    Updater *m_updater = nullptr;
    QTimer *m_autosave = nullptr;
    bool m_autosaved = false;
    bool m_recoveryHandled = false;
    bool m_checkInteractive = false;
    QString m_updateUrl;
    QString m_updateVersion;
};
