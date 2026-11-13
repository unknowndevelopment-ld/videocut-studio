#include "MainWindow.h"

#include "ExportDialog.h"
#include "MediaBin.h"
#include "MediaUtils.h"
#include "PreviewPanel.h"
#include "PropertiesPanel.h"
#include "Renderer.h"
#include "Theme.h"
#include "TimelineWidget.h"
#include "Updater.h"
#include "util.h"

#include <QAccessible>
#include <QAbstractSpinBox>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTextEdit>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <cmath>
#include <memory>

namespace {

const char *kProjectFilter = "VideoCut Studio project (*.vcs);;All files (*.*)";

QString shortName(const Clip &c)
{
    return c.label();
}

} // namespace

// ------------------------------------------------------------ ClipListDialog

class ClipListDialog : public QDialog {
    Q_OBJECT
public:
    explicit ClipListDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle(tr("Clip list"));
        setAccessibleName(tr("Clip list"));
        resize(Theme::px(620), Theme::px(400));

        auto *hint = new QLabel(tr("Accessible clip list — select one or more clips, then use the "
                                   "timeline commands. Press Enter to jump to a clip."));
        hint->setWordWrap(true);
        hint->setObjectName("Hint");

        m_list = new QListWidget;
        m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
        m_list->setAccessibleName(tr("Clips"));
        m_list->setAccessibleDescription(tr("Choose clips; the timeline selection follows this list."));

        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(Theme::px(16), Theme::px(16), Theme::px(16), Theme::px(16));
        layout->addWidget(hint);
        layout->addWidget(m_list, 1);

        connect(m_list, &QListWidget::itemSelectionChanged, this, [this]() {
            if (m_updating) return;
            emit selectionChanged(selectedIds());
        });
        connect(m_list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
            if (item) emit activated(item->data(Qt::UserRole).toString());
        });
    }

    void setProject(const Project &project)
    {
        m_updating = true;
        const QStringList previous = selectedIds();
        m_list->clear();
        for (const Clip &c : project.clips) {
            Track *track = const_cast<Project &>(project).track(c.track);
            const QString row = QString("%1   %2   %3 – %4")
                                    .arg(track ? track->name : "?", shortName(c),
                                         timecode(c.start), timecode(c.end()));
            auto *item = new QListWidgetItem(row);
            item->setData(Qt::UserRole, c.id);
            item->setToolTip(QString("%1\n%2\n%3 – %4  (%5)")
                                 .arg(shortName(c), kindName(c.kind).toUpper(),
                                      timecode(c.start), timecode(c.end()),
                                      humanDuration(c.duration)));
            m_list->addItem(item);
        }
        m_updating = false;
        setSelection(previous);
    }

    void setSelection(const QStringList &ids)
    {
        m_updating = true;
        for (int i = 0; i < m_list->count(); ++i) {
            auto *item = m_list->item(i);
            item->setSelected(ids.contains(item->data(Qt::UserRole).toString()));
        }
        m_updating = false;
    }

    QStringList selectedIds() const
    {
        QStringList ids;
        for (auto *item : m_list->selectedItems()) ids << item->data(Qt::UserRole).toString();
        return ids;
    }

signals:
    void selectionChanged(const QStringList &ids);
    void activated(const QString &id);

private:
    QListWidget *m_list = nullptr;
    bool m_updating = false;
};

// ----------------------------------------------------------------- MainWindow

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_project(Project::createDefault()), m_history(m_project)
{
    setWindowTitle(tr("VideoCut Studio"));
    setMinimumSize(Theme::px(1040), Theme::px(680));
    resize(Theme::px(1480), Theme::px(940));
    setAcceptDrops(true);
    setWindowIcon(Theme::icon("play", Theme::accent(), 64));

    QSettings settings("VideoCut", "VideoCutStudio");
    m_scale = settings.value("ui_scale", 1.0).toDouble();
    m_contrast = settings.value("high_contrast", false).toBool();
    for (const QString &file : settings.value("custom_fonts").toStringList())
        QFontDatabase::addApplicationFont(file);
    Theme::apply(qApp, m_scale, m_contrast);

    buildUi();
    createActions();
    createMenus();
    wireSignals();
    restoreSettings();

    m_mediaBin->setProject(&m_project);
    m_properties->setProject(&m_project);
    m_timeline->setProject(&m_project);
    m_preview->engine()->setQuality(QSettings("VideoCut", "VideoCutStudio")
                                        .value("preview_quality", 0).toInt());
    m_preview->engine()->setProject(&m_project);
    m_preview->refreshSize();
    m_preview->setDuration(m_project.duration());
    m_timeline->zoomFit();
    updateTitle();
    updateStatus();
    updateActions();
    if (m_project.clips.isEmpty()) {
        announce(tr("Ready. Press Ctrl+I to import a video."));
    }

    m_updater = new Updater(this);
    connect(m_updater, &Updater::upToDate, this, [this]() {
        if (m_checkInteractive)
            QMessageBox::information(this, tr("No updates"),
                                     tr("You are running the latest version (%1).")
                                         .arg(Updater::version()));
    });
    connect(m_updater, &Updater::failed, this, [this](const QString &message) {
        if (m_checkInteractive) QMessageBox::warning(this, tr("Update check failed"), message);
    });
    connect(m_updater, &Updater::updateAvailable, this,
            [this](const QString &version, const QString &url, const QString &notes) {
                m_updateVersion = version;
                m_updateUrl = url;
                if (!m_checkInteractive) {
                    announce(tr("Update %1 available — Help → Check for updates").arg(version));
                    return;
                }
                QMessageBox box(this);
                box.setWindowTitle(tr("Update available"));
                box.setText(tr("VideoCut Studio %1 is available — you have %2.")
                                .arg(version, Updater::version()));
                if (!notes.isEmpty()) box.setInformativeText(notes.left(700));
                box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                box.button(QMessageBox::Yes)->setText(tr("Download and install"));
                box.button(QMessageBox::No)->setText(tr("Later"));
                if (box.exec() == QMessageBox::Yes) startUpdateDownload();
            });
    connect(m_updater, &Updater::downloadFinished, this,
            [this](const QString &file, const QString &version) {
                for (QProgressDialog *dialog : findChildren<QProgressDialog *>()) dialog->close();
                const auto answer = QMessageBox::question(
                    this, tr("Ready to install"),
                    tr("VideoCut Studio %1 has been downloaded.\n\n"
                       "The editor will close and the installer will finish the update.").arg(version),
                    QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);
                if (answer != QMessageBox::Ok) return;
                QProcess::startDetached("msiexec.exe", {"/i", QDir::toNativeSeparators(file),
                                                        "/passive", "/norestart"});
                m_dirty = false;
                qApp->quit();
            });

    if (!qApp->arguments().contains("--selftest") && Updater::configured()) {
        QTimer::singleShot(9000, this, [this]() {
            m_checkInteractive = false;
            m_updater->check(false);
        });
    }
}

// ---------------------------------------------------------------------- UI

QPushButton *makeToolButton(const QString &text, const QString &iconName,
                            const QString &tooltip = QString(), bool iconOnly = false)
{
    auto *button = new QPushButton(iconOnly ? QString() : "  " + text);
    button->setObjectName("Tool");
    button->setIcon(Theme::icon(iconName, Theme::text(), Theme::px(17)));
    button->setIconSize(QSize(Theme::px(17), Theme::px(17)));
    button->setToolTip(tooltip);
    button->setAccessibleName(text.isEmpty() ? tooltip : text);
    button->setFocusPolicy(Qt::NoFocus);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

void MainWindow::buildUi()
{
    m_mediaBin = new MediaBin;
    m_mediaBin->setMinimumWidth(Theme::px(250));
    m_mediaBin->setMaximumWidth(Theme::px(380));

    m_preview = new PreviewPanel;
    m_preview->setMinimumWidth(Theme::px(460));

    m_properties = new PropertiesPanel;
    m_properties->setMinimumWidth(Theme::px(300));
    m_properties->setMaximumWidth(Theme::px(420));

    m_timeline = new TimelineWidget;
    m_timeline->setMinimumHeight(Theme::px(230));

    auto *topBar = new QFrame;
    topBar->setStyleSheet(QString("QFrame { background:%1; border:none; border-bottom:1px solid %2; }")
                              .arg(Theme::panel().name(), Theme::border().name()));
    auto *bar = new QHBoxLayout(topBar);
    bar->setContentsMargins(Theme::px(14), Theme::px(9), Theme::px(14), Theme::px(9));
    bar->setSpacing(Theme::px(4));

    auto *brand = new QLabel(tr("VideoCut Studio"));
    brand->setStyleSheet(QString("color:%1; font-size:%2px; font-weight:800; letter-spacing:0.4px;")
                             .arg(Theme::text().name()).arg(int(std::lround(13 * Theme::scale))));
    brand->setAccessibleName(tr("VideoCut Studio"));

    auto *importBtn = makeToolButton(tr("Import"), "plus", tr("Import media (Ctrl+I)"));
    importBtn->setObjectName("Primary");
    auto *textBtn = makeToolButton(tr("Text"), "text", tr("Add text (T)"));
    auto *splitBtn = makeToolButton(tr("Split"), "split", tr("Split at playhead (S)"));
    auto *deleteBtn = makeToolButton(tr("Delete"), "trash", tr("Delete selection (Del)"));
    auto *undoBtn = makeToolButton(QString(), "undo", tr("Undo (Ctrl+Z)"), true);
    auto *redoBtn = makeToolButton(QString(), "redo", tr("Redo (Ctrl+Y)"), true);
    m_btnSnap = makeToolButton(tr("Snap"), "magnet", tr("Snapping (N)"));
    m_btnSnap->setCheckable(true);
    m_btnSnap->setChecked(true);
    auto *zoomOutBtn = makeToolButton(QString(), "zoomOut", tr("Zoom out (-)"), true);
    auto *zoomInBtn = makeToolButton(QString(), "zoomIn", tr("Zoom in (+)"), true);
    auto *fitBtn = makeToolButton(QString(), "fit", tr("Zoom to fit (F)"), true);
    auto *listBtn = makeToolButton(QString(), "list", tr("Accessible clip list (Alt+L)"), true);
    auto *exportBtn = makeToolButton(tr("Export"), "export", tr("Export video (Ctrl+E)"));
    exportBtn->setObjectName("Primary");

    auto separator = []() {
        auto *line = new QFrame;
        line->setFixedSize(1, Theme::px(22));
        line->setStyleSheet(QString("background:%1;").arg(Theme::border().name()));
        return line;
    };

    bar->addWidget(brand);
    bar->addSpacing(Theme::px(14));
    bar->addWidget(importBtn);
    bar->addWidget(textBtn);
    bar->addSpacing(Theme::px(4));
    bar->addWidget(separator());
    bar->addSpacing(Theme::px(4));
    bar->addWidget(splitBtn);
    bar->addWidget(deleteBtn);
    bar->addWidget(undoBtn);
    bar->addWidget(redoBtn);
    bar->addSpacing(Theme::px(4));
    bar->addWidget(separator());
    bar->addSpacing(Theme::px(4));
    bar->addWidget(m_btnSnap);
    bar->addWidget(zoomOutBtn);
    bar->addWidget(zoomInBtn);
    bar->addWidget(fitBtn);
    bar->addWidget(listBtn);
    bar->addStretch(1);
    bar->addWidget(exportBtn);

    connect(importBtn, &QPushButton::clicked, this, &MainWindow::importMedia);
    connect(textBtn, &QPushButton::clicked, this, &MainWindow::addText);
    connect(splitBtn, &QPushButton::clicked, this, &MainWindow::splitClip);
    connect(deleteBtn, &QPushButton::clicked, this, [this]() { deleteClips(false); });
    connect(undoBtn, &QPushButton::clicked, this, &MainWindow::undo);
    connect(redoBtn, &QPushButton::clicked, this, &MainWindow::redo);
    connect(zoomOutBtn, &QPushButton::clicked, m_timeline, &TimelineWidget::zoomOut);
    connect(zoomInBtn, &QPushButton::clicked, m_timeline, &TimelineWidget::zoomIn);
    connect(fitBtn, &QPushButton::clicked, m_timeline, &TimelineWidget::zoomFit);
    connect(listBtn, &QPushButton::clicked, this, &MainWindow::showClipList);
    connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportVideo);
    connect(m_btnSnap, &QPushButton::toggled, this, [this](bool on) {
        m_timeline->snapEnabled = on;
        if (m_actSnap && m_actSnap->isChecked() != on) {
            m_actSnap->blockSignals(true);
            m_actSnap->setChecked(on);
            m_actSnap->blockSignals(false);
        }
    });

    m_upperSplitter = new QSplitter(Qt::Horizontal);
    m_upperSplitter->addWidget(m_preview);
    m_upperSplitter->addWidget(m_properties);
    m_upperSplitter->setStretchFactor(0, 1);
    m_upperSplitter->setStretchFactor(1, 0);
    m_upperSplitter->setSizes({Theme::px(980), Theme::px(340)});

    m_verticalSplitter = new QSplitter(Qt::Vertical);
    m_verticalSplitter->addWidget(m_upperSplitter);
    m_verticalSplitter->addWidget(m_timeline);
    m_verticalSplitter->setStretchFactor(0, 1);
    m_verticalSplitter->setStretchFactor(1, 0);
    m_verticalSplitter->setSizes({Theme::px(520), Theme::px(330)});

    m_mainSplitter = new QSplitter(Qt::Horizontal);
    m_mainSplitter->addWidget(m_mediaBin);
    m_mainSplitter->addWidget(m_verticalSplitter);
    m_mainSplitter->setStretchFactor(0, 0);
    m_mainSplitter->setStretchFactor(1, 1);
    m_mainSplitter->setSizes({Theme::px(265), Theme::px(1180)});

    auto *central = new QWidget;
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(topBar);
    auto *body = new QWidget;
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(Theme::px(10), Theme::px(10), Theme::px(10), Theme::px(10));
    bodyLayout->addWidget(m_mainSplitter);
    outer->addWidget(body, 1);
    setCentralWidget(central);

    m_status = new QLabel;
    m_status->setAccessibleName(tr("Status"));
    m_status->setStyleSheet(QString("color:%1;").arg(Theme::muted().name()));
    m_info = new QLabel;
    m_info->setStyleSheet(QString("color:%1;").arg(Theme::muted().name()));
    statusBar()->addWidget(m_status, 1);
    statusBar()->addPermanentWidget(m_info);
}

// ----------------------------------------------------------------- actions

void MainWindow::createActions()
{
    auto *actNew = new QAction(tr("New project"), this);
    actNew->setShortcut(QKeySequence("Ctrl+N"));
    connect(actNew, &QAction::triggered, this, &MainWindow::newProject);

    auto *actOpen = new QAction(tr("Open project…"), this);
    actOpen->setShortcut(QKeySequence("Ctrl+O"));
    connect(actOpen, &QAction::triggered, this, &MainWindow::openProject);

    auto *actSave = new QAction(tr("Save project"), this);
    actSave->setShortcut(QKeySequence("Ctrl+S"));
    connect(actSave, &QAction::triggered, this, [this]() { saveProject(); });

    auto *actSaveAs = new QAction(tr("Save project as…"), this);
    actSaveAs->setShortcut(QKeySequence("Ctrl+Shift+S"));
    connect(actSaveAs, &QAction::triggered, this, [this]() { saveProjectAs(); });

    auto *actImport = new QAction(tr("Import media…"), this);
    actImport->setShortcut(QKeySequence("Ctrl+I"));
    connect(actImport, &QAction::triggered, this, &MainWindow::importMedia);

    auto *actExport = new QAction(tr("Export video…"), this);
    actExport->setShortcut(QKeySequence("Ctrl+E"));
    connect(actExport, &QAction::triggered, this, &MainWindow::exportVideo);

    auto *actQuit = new QAction(tr("Quit"), this);
    actQuit->setShortcut(QKeySequence("Ctrl+Q"));
    connect(actQuit, &QAction::triggered, this, &QWidget::close);

    auto *actUndo = new QAction(tr("Undo"), this);
    actUndo->setShortcut(QKeySequence("Ctrl+Z"));
    connect(actUndo, &QAction::triggered, this, &MainWindow::undo);

    auto *actRedo = new QAction(tr("Redo"), this);
    actRedo->setShortcut(QKeySequence("Ctrl+Y"));
    connect(actRedo, &QAction::triggered, this, &MainWindow::redo);

    auto *actCut = new QAction(tr("Cut"), this);
    actCut->setShortcut(QKeySequence("Ctrl+X"));
    connect(actCut, &QAction::triggered, this, &MainWindow::cutClips);

    auto *actCopy = new QAction(tr("Copy"), this);
    actCopy->setShortcut(QKeySequence("Ctrl+C"));
    connect(actCopy, &QAction::triggered, this, &MainWindow::copyClips);

    auto *actPaste = new QAction(tr("Paste"), this);
    actPaste->setShortcut(QKeySequence("Ctrl+V"));
    connect(actPaste, &QAction::triggered, this, &MainWindow::pasteClips);

    auto *actDelete = new QAction(tr("Delete"), this);
    actDelete->setShortcut(QKeySequence("Del"));
    connect(actDelete, &QAction::triggered, this, [this]() { deleteClips(false); });

    auto *actRipple = new QAction(tr("Ripple delete"), this);
    actRipple->setShortcut(QKeySequence("Shift+Del"));
    connect(actRipple, &QAction::triggered, this, [this]() { deleteClips(true); });

    auto *actSplit = new QAction(tr("Split at playhead\tS"), this);
    connect(actSplit, &QAction::triggered, this, &MainWindow::splitClip);

    auto *actDuplicate = new QAction(tr("Duplicate"), this);
    actDuplicate->setShortcut(QKeySequence("Ctrl+D"));
    connect(actDuplicate, &QAction::triggered, this, &MainWindow::duplicateClips);

    auto *actSelectAll = new QAction(tr("Select all"), this);
    actSelectAll->setShortcut(QKeySequence("Ctrl+A"));
    connect(actSelectAll, &QAction::triggered, this, &MainWindow::selectAll);

    auto *actAddText = new QAction(tr("Text clip\tT"), this);
    connect(actAddText, &QAction::triggered, this, &MainWindow::addText);

    auto *actAddTransition = new QAction(tr("Transition after selected clip"), this);
    connect(actAddTransition, &QAction::triggered, this, [this]() {
        auto selected = m_timeline->selectedClips();
        if (selected.isEmpty()) {
            announce(tr("Select a clip first"));
            return;
        }
        Clip *clip = selected.first();
        auto clips = m_project.clipsOn(clip->track);
        for (int i = 0; i + 1 < clips.size(); ++i) {
            if (clips[i]->id != clip->id) continue;
            Clip *next = clips[i + 1];
            if (next->start > clip->end() + 0.02) {
                announce(tr("The next clip needs to touch this one"));
                return;
            }
            if (m_project.transitionFor(next->id)) {
                announce(tr("There is already a transition here"));
                return;
            }
            m_timeline->addTransitionBetween(clip->id, next->id);
            return;
        }
        announce(tr("No following clip on this track"));
    });

    auto *actAddVideoTrack = new QAction(tr("Video track"), this);
    connect(actAddVideoTrack, &QAction::triggered, this, [this]() {
        pushUndo(tr("Add video track"));
        m_project.addTrack(ClipKind::Video);
        onProjectChanged();
        announce(tr("Video track added"));
    });

    auto *actAddAudioTrack = new QAction(tr("Audio track"), this);
    connect(actAddAudioTrack, &QAction::triggered, this, [this]() {
        pushUndo(tr("Add audio track"));
        m_project.addTrack(ClipKind::Audio);
        onProjectChanged();
        announce(tr("Audio track added"));
    });

    auto *actZoomIn = new QAction(tr("Zoom in\t+"), this);
    connect(actZoomIn, &QAction::triggered, m_timeline, &TimelineWidget::zoomIn);
    auto *actZoomOut = new QAction(tr("Zoom out\t-"), this);
    connect(actZoomOut, &QAction::triggered, m_timeline, &TimelineWidget::zoomOut);
    auto *actFit = new QAction(tr("Zoom to fit\tF"), this);
    connect(actFit, &QAction::triggered, m_timeline, &TimelineWidget::zoomFit);

    m_actSnap = new QAction(tr("Snapping\tN"), this);
    m_actSnap->setCheckable(true);
    m_actSnap->setChecked(true);
    connect(m_actSnap, &QAction::toggled, this, [this](bool on) {
        m_timeline->snapEnabled = on;
        if (m_btnSnap->isChecked() != on) {
            m_btnSnap->blockSignals(true);
            m_btnSnap->setChecked(on);
            m_btnSnap->blockSignals(false);
        }
    });

    auto *actClipList = new QAction(tr("Clip list…"), this);
    actClipList->setShortcut(QKeySequence("Alt+L"));
    connect(actClipList, &QAction::triggered, this, &MainWindow::showClipList);

    auto *qualityMenu = new QMenu(tr("Preview quality"), this);
    auto *qualityGroup = new QActionGroup(this);
    const QVector<QPair<QString, int>> qualities = {
        {tr("Full resolution"), 0}, {tr("Half resolution"), 1}, {tr("Quarter (fastest)"), 2}};
    const int savedQuality = QSettings("VideoCut", "VideoCutStudio")
                                 .value("preview_quality", 0).toInt();
    for (const auto &q : qualities) {
        auto *action = qualityMenu->addAction(q.first);
        action->setCheckable(true);
        action->setChecked(q.second == savedQuality);
        qualityGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, level = q.second, title = q.first]() {
            m_preview->engine()->setQuality(level);
            QSettings("VideoCut", "VideoCutStudio").setValue("preview_quality", level);
            announce(tr("Preview quality: %1").arg(title));
        });
    }

    auto *actContrast = new QAction(tr("High contrast mode"), this);
    actContrast->setShortcut(QKeySequence("Alt+H"));
    actContrast->setCheckable(true);
    actContrast->setChecked(m_contrast);
    connect(actContrast, &QAction::triggered, this, &MainWindow::toggleHighContrast);

    auto *actScaleUp = new QAction(tr("Larger interface"), this);
    actScaleUp->setShortcut(QKeySequence("Alt+Plus"));
    connect(actScaleUp, &QAction::triggered, this, [this]() { setInterfaceScale(m_scale + 0.1); });
    auto *actScaleDown = new QAction(tr("Smaller interface"), this);
    actScaleDown->setShortcut(QKeySequence("Alt+Minus"));
    connect(actScaleDown, &QAction::triggered, this, [this]() { setInterfaceScale(m_scale - 0.1); });
    auto *actScaleReset = new QAction(tr("Reset interface size"), this);
    connect(actScaleReset, &QAction::triggered, this, [this]() { setInterfaceScale(1.0); });

    auto *actShortcuts = new QAction(tr("Keyboard shortcuts"), this);
    actShortcuts->setShortcut(QKeySequence("F1"));
    connect(actShortcuts, &QAction::triggered, this, &MainWindow::showShortcuts);

    auto *actUpdates = new QAction(tr("Check for updates…"), this);
    connect(actUpdates, &QAction::triggered, this, [this]() { checkForUpdates(true); });

    auto *actTerms = new QAction(tr("Terms of Service"), this);
    connect(actTerms, &QAction::triggered, this, [this]() { openLegal("TERMS.md"); });
    auto *actPrivacy = new QAction(tr("Privacy Policy"), this);
    connect(actPrivacy, &QAction::triggered, this, [this]() { openLegal("PRIVACY.md"); });
    auto *actThirdParty = new QAction(tr("Third-party notices"), this);
    connect(actThirdParty, &QAction::triggered, this, [this]() { openLegal("THIRD-PARTY.md"); });

    auto *actAbout = new QAction(tr("About VideoCut Studio"), this);
    connect(actAbout, &QAction::triggered, this, &MainWindow::showAbout);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(actNew);
    fileMenu->addAction(actOpen);
    fileMenu->addAction(actSave);
    fileMenu->addAction(actSaveAs);
    fileMenu->addSeparator();
    fileMenu->addAction(actImport);
    fileMenu->addAction(actExport);
    fileMenu->addSeparator();
    fileMenu->addAction(actQuit);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(actUndo);
    editMenu->addAction(actRedo);
    editMenu->addSeparator();
    editMenu->addAction(actCut);
    editMenu->addAction(actCopy);
    editMenu->addAction(actPaste);
    editMenu->addAction(actDuplicate);
    editMenu->addSeparator();
    editMenu->addAction(actSplit);
    editMenu->addAction(actDelete);
    editMenu->addAction(actRipple);
    editMenu->addSeparator();
    editMenu->addAction(actSelectAll);

    auto *addMenu = menuBar()->addMenu(tr("&Add"));
    addMenu->addAction(actAddText);
    addMenu->addAction(actAddTransition);
    addMenu->addAction(actAddVideoTrack);
    addMenu->addAction(actAddAudioTrack);

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(actZoomIn);
    viewMenu->addAction(actZoomOut);
    viewMenu->addAction(actFit);
    viewMenu->addSeparator();
    viewMenu->addAction(m_actSnap);
    viewMenu->addAction(actClipList);
    viewMenu->addMenu(qualityMenu);
    viewMenu->addSeparator();
    viewMenu->addAction(actScaleUp);
    viewMenu->addAction(actScaleDown);
    viewMenu->addAction(actScaleReset);
    viewMenu->addAction(actContrast);

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(actShortcuts);
    helpMenu->addSeparator();
    helpMenu->addAction(actUpdates);
    helpMenu->addAction(actTerms);
    helpMenu->addAction(actPrivacy);
    helpMenu->addAction(actThirdParty);
    helpMenu->addSeparator();
    helpMenu->addAction(actAbout);

    // keep references for enabling/disabling
    actUndo->setObjectName("actUndo");
    actRedo->setObjectName("actRedo");
    m_actionUndo = actUndo;
    m_actionRedo = actRedo;

    addActions({actUndo, actRedo, actCut, actCopy, actPaste, actDelete, actRipple,
                actDuplicate, actSelectAll, actSplit, actAddText});
}

void MainWindow::createMenus() {}

void MainWindow::wireSignals()
{
    connect(m_timeline, &TimelineWidget::seek, this, &MainWindow::onSeek);
    connect(m_timeline, &TimelineWidget::changed, this, &MainWindow::onProjectChanged);
    connect(m_timeline, &TimelineWidget::undoPoint, this, [this](const QString &label) { pushUndo(label); });
    connect(m_timeline, &TimelineWidget::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(m_timeline, &TimelineWidget::clipActivated, this, [this](const QString &id) {
        if (Clip *c = m_project.clip(id))
            if (c->kind == ClipKind::Text) m_properties->setClips({c});
    });
    connect(m_timeline, &TimelineWidget::mediaDropped, this,
            [this](const QStringList &paths, const QString &trackId, double at) {
                m_mediaBin->addPaths(paths);
                addMediaToTimeline(paths, trackId, at);
            });
    connect(m_timeline, &TimelineWidget::announce, this, &MainWindow::announce);

    connect(m_preview, &PreviewPanel::playRequested, this, &MainWindow::togglePlay);
    connect(m_preview, &PreviewPanel::playingChanged, this, &MainWindow::onPlayToggled);
    connect(m_preview, &PreviewPanel::positionChanged, this, &MainWindow::onPlayerPosition);

    connect(m_mediaBin, &MediaBin::addRequested, this,
            [this](const QStringList &paths) { addMediaToTimeline(paths); });
    connect(m_mediaBin, &MediaBin::importRequested, this, &MainWindow::importMedia);
    connect(m_mediaBin, &MediaBin::mediaAdded, this, [this]() { markDirty(); });

    connect(m_properties, &PropertiesPanel::changed, this, &MainWindow::onProjectChanged);
    connect(m_properties, &PropertiesPanel::undoPoint, this, [this](const QString &label, const QString &key) { pushUndo(label, key); });
}

// -------------------------------------------------------------------- state

void MainWindow::pushUndo(const QString &label, const QString &key)
{
    m_history.push(label, key);
    updateActions();
}

void MainWindow::markDirty()
{
    if (m_dirty) return;
    m_dirty = true;
    updateTitle();
}

void MainWindow::updateTitle()
{
    setWindowTitle(QString("%1%2 — VideoCut Studio")
                       .arg(m_project.name.isEmpty() ? tr("Untitled") : m_project.name)
                       .arg(m_dirty ? " *" : ""));
}

void MainWindow::updateStatus()
{
    m_info->setText(tr("%1 x %2 · %3 fps · %4 · %5 clips")
                        .arg(m_project.width).arg(m_project.height)
                        .arg(m_project.fps, 0, 'g', 5)
                        .arg(timecode(m_project.duration()))
                        .arg(m_project.clips.size()));
}

void MainWindow::updateActions()
{
    if (m_actionUndo) m_actionUndo->setEnabled(m_history.canUndo());
    if (m_actionRedo) m_actionRedo->setEnabled(m_history.canRedo());
}

void MainWindow::announce(const QString &text)
{
    if (m_status) m_status->setText(text);
    if (m_status) {
        QAccessibleEvent event(m_status, QAccessible::Alert);
        QAccessible::updateAccessibility(&event);
    }
}

void MainWindow::updateA11ySummary()
{
    m_timeline->setAccessibleDescription(
        tr("%1 clips, %2 tracks, position %3 of %4. %5 selected.")
            .arg(m_project.clips.size()).arg(m_project.tracks.size())
            .arg(timecode(m_timeline->playhead())).arg(timecode(m_project.duration()))
            .arg(m_timeline->selectedIds().size()));
}

bool MainWindow::textFieldFocused() const
{
    QWidget *w = QApplication::focusWidget();
    if (!w) return false;
    return qobject_cast<QLineEdit *>(w) || qobject_cast<QPlainTextEdit *>(w)
            || qobject_cast<QTextEdit *>(w) || qobject_cast<QAbstractSpinBox *>(w)
            || qobject_cast<QComboBox *>(w);
}

// ------------------------------------------------------------------ preview

void MainWindow::onProjectChanged()
{
    markDirty();
    m_timeline->refresh();
    m_properties->setClips(m_timeline->selectedClips());
    updateStatus();
    updateA11ySummary();
    if (m_playing) m_preview->engine()->pause();
    m_preview->engine()->invalidate();
    m_preview->setDuration(m_project.duration());
}

void MainWindow::onSelectionChanged()
{
    m_properties->setClips(m_timeline->selectedClips());
    updateA11ySummary();
    if (m_clipList && m_clipList->isVisible())
        m_clipList->setSelection(m_timeline->selectedIds());
}

void MainWindow::onSeek(double seconds)
{
    seconds = qMax(0.0, seconds);
    m_timeline->setPlayhead(seconds);
    m_preview->setTimeLabel(seconds);
    m_preview->engine()->setPlayhead(seconds);
}

void MainWindow::togglePlay()
{
    PreviewEngine *engine = m_preview->engine();
    if (engine->isPlaying()) {
        engine->pause();
        return;
    }
    if (m_project.duration() <= 0) {
        announce(tr("Timeline is empty"));
        return;
    }
    engine->setPlayhead(m_timeline->playhead());
    engine->play();
}

void MainWindow::onPlayToggled(bool playing)
{
    m_playing = playing;
    if (!playing)
        m_preview->setTimeLabel(m_preview->engine()->position());
}

void MainWindow::onPlayerPosition(double seconds)
{
    if (!m_playing) return;
    m_timeline->setPlayhead(seconds, true);
}
// ------------------------------------------------------------------- media

void MainWindow::importMedia()
{
    QSettings settings("VideoCut", "VideoCutStudio");
    const QString start = settings.value("media_dir", MediaUtils::defaultExportDir()).toString();
    const QString filter = tr("Media files (%1);;All files (*.*)").arg(MediaUtils::mediaExtensions().join(' '));
    const QStringList paths = QFileDialog::getOpenFileNames(this, tr("Import media"), start, filter);
    if (paths.isEmpty()) return;
    settings.setValue("media_dir", QFileInfo(paths.first()).absolutePath());
    pushUndo(tr("Import media"));
    m_mediaBin->addPaths(paths);
    markDirty();
    if (m_project.clips.isEmpty()) addMediaToTimeline(paths);
    else announce(tr("Added %1 file(s) to the media bin").arg(paths.size()));
}

Track *MainWindow::pickTrack(ClipKind kind)
{
    if (kind == ClipKind::Audio) {
        auto tracks = m_project.audioTracks();
        for (int i = tracks.size() - 1; i >= 0; --i)
            if (!tracks[i]->locked) return tracks[i];
        return m_project.addTrack(ClipKind::Audio);
    }
    auto video = m_project.videoTracks();
    if (kind == ClipKind::Text) {
        for (Track *t : video)
            if (!t->locked) return t;
        return m_project.addTrack(ClipKind::Video);
    }
    for (int i = video.size() - 1; i >= 0; --i)
        if (!video[i]->locked) return video[i];
    return m_project.addTrack(ClipKind::Video);
}

double MainWindow::appendStart(const QString &trackId) const
{
    double start = 0;
    bool any = false;
    for (const Clip &c : m_project.clips) {
        if (c.track != trackId) continue;
        start = qMax(start, c.end());
        any = true;
    }
    return any ? start : m_timeline->playhead();
}

void MainWindow::addMediaToTimeline(const QStringList &paths, const QString &trackId, double at)
{
    if (paths.isEmpty()) return;
    pushUndo(tr("Add to timeline"));
    QStringList newIds;
    double cursor = at;
    for (const QString &path : paths) {
        const MediaInfo info = MediaUtils::probe(path);
        if (!info.error.isEmpty()) {
            announce(tr("Could not read %1").arg(QFileInfo(path).fileName()));
            continue;
        }
        ClipKind kind = ClipKind::Video;
        if (info.isImage()) kind = ClipKind::Image;
        else if (!info.hasVideo && info.hasAudio) kind = ClipKind::Audio;
        else if (!info.hasVideo) continue;

        Track *track = trackId.isEmpty() ? nullptr : m_project.track(trackId);
        if (!track || (track->kind == ClipKind::Audio && kind != ClipKind::Audio)
            || (track->kind == ClipKind::Video && kind == ClipKind::Audio))
            track = pickTrack(kind);
        if (!track) continue;

        double duration = info.isImage() ? 4.0 : info.duration;
        if (duration <= 0.05) duration = 5.0;
        const double start = cursor >= 0 ? cursor : appendStart(track->id);

        Clip clip;
        clip.track = track->id;
        clip.kind = kind;
        clip.path = path;
        clip.name = QFileInfo(path).fileName();
        clip.start = start;
        clip.duration = duration;
        clip.srcDur = info.duration;
        clip.fontSize = qMax(24, int(m_project.height * 0.07));
        m_project.clips.append(clip);
        newIds << clip.id;
        if (cursor >= 0) cursor = clip.end();
    }
    if (newIds.isEmpty()) return;
    m_project.sortClips();
    const bool firstClips = m_project.clips.size() == newIds.size();
    m_timeline->selectIds(newIds);
    onProjectChanged();
    if (firstClips) m_timeline->zoomFit();
    announce(tr("Added %1 clip(s)").arg(newIds.size()));
}

void MainWindow::addText()
{
    pushUndo(tr("Add text"));
    Track *track = pickTrack(ClipKind::Text);
    if (!track) return;
    Clip clip;
    clip.track = track->id;
    clip.kind = ClipKind::Text;
    clip.text = tr("Your text here");
    clip.start = m_timeline->playhead();
    clip.duration = 4.0;
    clip.fontSize = qMax(24, int(m_project.height * 0.07));
    m_project.clips.append(clip);
    m_timeline->selectIds({clip.id});
    onProjectChanged();
    announce(tr("Text clip added. Edit it in the properties panel."));
}

// ----------------------------------------------------------------- editing

void MainWindow::splitClip()
{
    if (textFieldFocused()) return;
    const int before = m_project.clips.size();
    m_timeline->splitSelected();
    if (m_project.clips.size() == before)
        announce(tr("Move the playhead over a selected clip to split it"));
}

void MainWindow::deleteClips(bool ripple)
{
    if (textFieldFocused()) return;
    m_timeline->deleteSelected(ripple);
}

void MainWindow::duplicateClips()
{
    m_timeline->duplicateSelected();
}

void MainWindow::selectAll()
{
    if (textFieldFocused()) return;
    QStringList ids;
    for (const Clip &c : m_project.clips) ids << c.id;
    m_timeline->selectIds(ids);
}

void MainWindow::copyClips()
{
    m_clipboard.clear();
    for (Clip *c : m_timeline->selectedClips()) m_clipboard << c->id;
    if (!m_clipboard.isEmpty())
        announce(tr("Copied %1 clip(s)").arg(m_clipboard.size()));
}

void MainWindow::cutClips()
{
    copyClips();
    deleteClips(false);
}

void MainWindow::pasteClips()
{
    if (m_clipboard.isEmpty()) return;
    QVector<Clip> source;
    double base = 1e9;
    for (const QString &id : m_clipboard) {
        if (Clip *c = m_project.clip(id)) {
            source << *c;
            base = qMin(base, c->start);
        }
    }
    if (source.isEmpty()) return;
    pushUndo(tr("Paste"));
    QStringList ids;
    for (Clip copy : source) {
        copy.id = newId("clip");
        copy.start = qMax(0.0, m_timeline->playhead() + (copy.start - base));
        m_project.clips.append(copy);
        ids << copy.id;
    }
    m_project.sortClips();
    m_timeline->selectIds(ids);
    onProjectChanged();
    announce(tr("Pasted %1 clip(s)").arg(ids.size()));
}

void MainWindow::undo()
{
    if (!m_history.canUndo()) return;
    const QString label = m_history.undo();
    afterHistory(label, tr("Undo"));
}

void MainWindow::redo()
{
    if (!m_history.canRedo()) return;
    const QString label = m_history.redo();
    afterHistory(label, tr("Redo"));
}

void MainWindow::afterHistory(const QString &label, const QString &verb)
{
    m_properties->setProject(&m_project);
    m_mediaBin->setProject(&m_project);
    m_timeline->clearSelection();
    m_timeline->refresh();
    updateStatus();
    updateTitle();
    updateActions();
    if (m_clipList && m_clipList->isVisible()) m_clipList->setProject(m_project);
    announce(QString("%1: %2").arg(verb, label));
    m_preview->engine()->invalidate();
    m_preview->setDuration(m_project.duration());
}

// ------------------------------------------------------------------ project

bool MainWindow::maybeSave()
{
    if (!m_dirty) return true;
    const auto answer = QMessageBox::question(
        this, tr("Unsaved changes"), tr("Save changes to this project?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (answer == QMessageBox::Save) return saveProject();
    return answer == QMessageBox::Discard;
}

void MainWindow::newProject()
{
    if (!maybeSave()) return;
    loadProject(Project::createDefault(), QString());
}

void MainWindow::loadProject(Project project, const QString &path)
{
    m_project = project;
    m_projectPath = path;
    m_history.setProject(&m_project);
    m_dirty = false;
    m_timeline->setProject(&m_project);
    m_timeline->zoomFit();
    m_mediaBin->setProject(&m_project);
    m_properties->setProject(&m_project);
    m_preview->engine()->setProject(&m_project);
    m_preview->refreshSize();
    m_preview->setDuration(m_project.duration());
    m_preview->setTimeLabel(0);
    if (m_project.clips.isEmpty()) m_preview->showMessage(tr("Nothing to preview yet"));
    if (m_clipList) m_clipList->setProject(m_project);
    updateTitle();
    updateStatus();
    updateActions();
}

void MainWindow::openProject()
{
    if (!maybeSave()) return;
    QSettings settings("VideoCut", "VideoCutStudio");
    const QString start = settings.value("project_dir", MediaUtils::defaultExportDir()).toString();
    const QString path = QFileDialog::getOpenFileName(this, tr("Open project"), start, kProjectFilter);
    if (path.isEmpty()) return;
    settings.setValue("project_dir", QFileInfo(path).absolutePath());
    openProjectFile(path);
}

bool MainWindow::openProjectFile(const QString &path)
{
    bool ok = false;
    Project project = Project::load(path, &ok);
    if (!ok) {
        QMessageBox::critical(this, tr("Open project"), tr("Could not open that project file."));
        return false;
    }
    loadProject(project, path);
    announce(tr("Opened %1").arg(QFileInfo(path).fileName()));
    return true;
}

bool MainWindow::saveProject()
{
    if (m_projectPath.isEmpty()) return saveProjectAs();
    if (!m_project.save(m_projectPath)) {
        QMessageBox::critical(this, tr("Save project"), tr("Could not save the project."));
        return false;
    }
    m_dirty = false;
    updateTitle();
    announce(tr("Saved %1").arg(QFileInfo(m_projectPath).fileName()));
    return true;
}

bool MainWindow::saveProjectAs()
{
    QSettings settings("VideoCut", "VideoCutStudio");
    const QString start = settings.value("project_dir", MediaUtils::defaultExportDir()).toString();
    QString stem = m_project.name;
    stem.replace(QRegularExpression("[^A-Za-z0-9 _-]"), "");
    if (stem.trimmed().isEmpty()) stem = "project";
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save project"), start + "/" + stem + ".vcs", kProjectFilter);
    if (path.isEmpty()) return false;
    if (!path.endsWith(".vcs", Qt::CaseInsensitive)) path += ".vcs";
    m_projectPath = path;
    settings.setValue("project_dir", QFileInfo(path).absolutePath());
    m_project.name = QFileInfo(path).completeBaseName();
    return saveProject();
}

void MainWindow::exportVideo()
{
    if (m_project.duration() <= 0) {
        QMessageBox::information(this, tr("Export"), tr("Add some clips to the timeline first."));
        return;
    }
    if (m_playing) m_preview->engine()->pause();
    ExportDialog dialog(m_project, this);
    dialog.exec();
}

// -------------------------------------------------------------------- view

void MainWindow::setInterfaceScale(double scale)
{
    m_scale = qBound(0.8, scale, 1.6);
    QSettings("VideoCut", "VideoCutStudio").setValue("ui_scale", m_scale);
    Theme::apply(qApp, m_scale, m_contrast);
    m_timeline->refresh();
    m_properties->setProject(&m_project);
    m_properties->setClips(m_timeline->selectedClips());
    m_mediaBin->rebuild();
    announce(tr("Interface size %1 percent").arg(int(m_scale * 100)));
}

void MainWindow::toggleHighContrast()
{
    m_contrast = !m_contrast;
    QSettings("VideoCut", "VideoCutStudio").setValue("high_contrast", m_contrast);
    Theme::apply(qApp, m_scale, m_contrast);
    m_timeline->refresh();
    m_properties->setProject(&m_project);
    m_properties->setClips(m_timeline->selectedClips());
    m_mediaBin->rebuild();
    announce(m_contrast ? tr("High contrast mode on") : tr("High contrast mode off"));
}

void MainWindow::showClipList()
{
    if (!m_clipList) {
        m_clipList = new ClipListDialog(this);
        connect(m_clipList, &ClipListDialog::selectionChanged, this, [this](const QStringList &ids) {
            m_timeline->selectIds(ids);
        });
        connect(m_clipList, &ClipListDialog::activated, this, [this](const QString &id) {
            if (Clip *c = m_project.clip(id)) {
                m_timeline->setPlayhead(c->start);
                m_timeline->selectIds({id});
                onSeek(c->start);
            }
        });
    }
    m_clipList->setProject(m_project);
    m_clipList->setSelection(m_timeline->selectedIds());
    m_clipList->show();
    m_clipList->raise();
    m_clipList->activateWindow();
}

void MainWindow::showShortcuts()
{
    QMessageBox::information(
        this, tr("Keyboard shortcuts"),
        tr("<b>Playback</b><br>"
           "Space — play / pause<br>"
           "← / → — step one frame (Shift = one second)<br>"
           "Home / End — jump to start / end<br><br>"
           "<b>Editing</b><br>"
           "S — split selected clip at playhead<br>"
           "Delete — delete · Shift+Delete — ripple delete<br>"
           "Ctrl+D — duplicate · Ctrl+C / Ctrl+V — copy / paste<br>"
           "T — add text · N — toggle snapping<br>"
           "Ctrl+Z / Ctrl+Y — undo / redo<br><br>"
           "<b>Timeline</b><br>"
           "Ctrl+Wheel — zoom · Wheel — scroll · Middle drag — pan<br>"
           "+ / - / F — zoom in / out / fit<br><br>"
           "<b>Accessibility</b><br>"
           "Alt+L — accessible clip list<br>"
           "Alt+H — high contrast · Alt+Plus / Alt+Minus — interface size<br>"
           "Tab / Shift+Tab — move focus · Enter — activate"));
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, tr("About VideoCut Studio"),
        tr("<h3>VideoCut Studio %1</h3>"
           "<p>A native, accessible video editor built with Qt 6, C++ and ffmpeg.</p>"
           "<p>Multi-track timeline, transitions, text animations, looks, "
           "real-time preview and H.264 / VP9 / GIF export.</p>"
           "<p style='color:#97a0b3'>ffmpeg: %2<br>Updates: %3</p>")
            .arg(Updater::version(), MediaUtils::ffmpegPath(),
                 Updater::configured() ? Updater::repo() : tr("not configured")));
}

void MainWindow::checkForUpdates(bool interactive)
{
    m_checkInteractive = interactive;
    if (interactive) announce(tr("Checking for updates…"));
    m_updater->check(interactive);
}

void MainWindow::startUpdateDownload()
{
    if (m_updateUrl.isEmpty()) return;
    auto *progress = new QProgressDialog(tr("Downloading VideoCut Studio %1…").arg(m_updateVersion),
                                         QString(), 0, 100, this);
    progress->setWindowTitle(tr("Update"));
    progress->setWindowModality(Qt::WindowModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    progress->setCancelButton(nullptr);
    connect(m_updater, &Updater::downloadProgress, progress, [progress](int percent) {
        progress->setValue(percent);
    });
    connect(m_updater, &Updater::failed, progress, &QProgressDialog::close);
    progress->show();
    m_updater->download(m_updateUrl, m_updateVersion);
}

void MainWindow::openLegal(const QString &fileName)
{
    const QString path = QCoreApplication::applicationDirPath() + "/legal/" + fileName;
    if (QFileInfo::exists(path)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        return;
    }
    QMessageBox::information(
        this, tr("Documentation"),
        tr("%1 is not installed next to the application.\n\n"
           "It ships in the installer's legal folder.").arg(fileName));
}

// ------------------------------------------------------------------ input

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (textFieldFocused()) {
        QMainWindow::keyPressEvent(event);
        return;
    }
    const int key = event->key();
    const auto mods = event->modifiers();
    if (key == Qt::Key_Space) togglePlay();
    else if (key == Qt::Key_S) splitClip();
    else if (key == Qt::Key_T) addText();
    else if (key == Qt::Key_N) m_btnSnap->toggle();
    else if (key == Qt::Key_F) m_timeline->zoomFit();
    else if (key == Qt::Key_Plus || key == Qt::Key_Equal) m_timeline->zoomIn();
    else if (key == Qt::Key_Minus) m_timeline->zoomOut();
    else if (key == Qt::Key_Left) onSeek(m_timeline->playhead() - (mods & Qt::ShiftModifier ? 1.0 : 1.0 / m_project.fps));
    else if (key == Qt::Key_Right) onSeek(m_timeline->playhead() + (mods & Qt::ShiftModifier ? 1.0 : 1.0 / m_project.fps));
    else if (key == Qt::Key_Home) onSeek(0.0);
    else if (key == Qt::Key_End) onSeek(m_project.duration());
    else if (key == Qt::Key_Delete && !(mods & Qt::ShiftModifier)) deleteClips(false);
    else if (key == Qt::Key_Escape) m_timeline->clearSelection();
    else QMainWindow::keyPressEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!maybeSave()) {
        event->ignore();
        return;
    }
    m_preview->engine()->pause();
    saveSettings();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    for (const QUrl &url : event->mimeData()->urls()) {
        const QString local = url.toLocalFile();
        if (QFileInfo::exists(local) && MediaUtils::isMediaFile(local)) paths << local;
    }
    if (paths.isEmpty()) return;
    pushUndo(tr("Import media"));
    m_mediaBin->addPaths(paths);
    addMediaToTimeline(paths);
    event->acceptProposedAction();
}

// ---------------------------------------------------------------- settings

void MainWindow::restoreSettings()
{
    QSettings settings("VideoCut", "VideoCutStudio");
    const QByteArray geometry = settings.value("geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    const QByteArray mainState = settings.value("splitter_main").toByteArray();
    if (!mainState.isEmpty()) m_mainSplitter->restoreState(mainState);
    const QByteArray upperState = settings.value("splitter_upper").toByteArray();
    if (!upperState.isEmpty()) m_upperSplitter->restoreState(upperState);
    const QByteArray verticalState = settings.value("splitter_vertical").toByteArray();
    if (!verticalState.isEmpty()) m_verticalSplitter->restoreState(verticalState);
}

void MainWindow::saveSettings()
{
    QSettings settings("VideoCut", "VideoCutStudio");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("splitter_main", m_mainSplitter->saveState());
    settings.setValue("splitter_upper", m_upperSplitter->saveState());
    settings.setValue("splitter_vertical", m_verticalSplitter->saveState());
}

void MainWindow::openFiles(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    pushUndo(tr("Import media"));
    m_mediaBin->addPaths(paths);
    addMediaToTimeline(paths);
}

// ---------------------------------------------------------------- selftest

void MainWindow::runSelfTest(const QString &outDir)
{
    QDir().mkpath(outDir);
    auto lines = std::make_shared<QStringList>();
    auto log = [lines, outDir](const QString &line) {
        qInfo().noquote() << line;
        lines->append(line);
        QFile file(outDir + "/selftest.log");
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write(lines->join('\n').toUtf8());
    };

    const QString mediaDir = outDir + "/media";
    QDir().mkpath(mediaDir);
    auto ensure = [&](const QString &name, const QStringList &args) {
        const QString path = mediaDir + "/" + name;
        if (QFileInfo::exists(path)) return path;
        QStringList cmd{MediaUtils::ffmpegPath(), "-v", "error", "-y"};
        cmd << args << path;
        MediaUtils::run(cmd, 180000);
        return path;
    };

    const QString clipA = ensure("clipA.mp4", {"-f", "lavfi", "-i", "testsrc2=size=1280x720:rate=30:duration=8",
                                               "-f", "lavfi", "-i", "sine=frequency=440:duration=8",
                                               "-c:v", "libx264", "-preset", "ultrafast",
                                               "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest"});
    const QString clipB = ensure("clipB.mp4", {"-f", "lavfi", "-i", "smptebars=size=1280x720:rate=30:duration=6",
                                               "-f", "lavfi", "-i", "sine=frequency=660:duration=6",
                                               "-c:v", "libx264", "-preset", "ultrafast",
                                               "-pix_fmt", "yuv420p", "-c:a", "aac", "-shortest"});
    const QString music = ensure("music.mp3", {"-f", "lavfi", "-i", "sine=frequency=220:duration=12",
                                               "-c:a", "libmp3lame"});
    const QString logo = ensure("logo.png", {"-f", "lavfi", "-i", "color=c=orange:size=800x600:duration=1",
                                             "-frames:v", "1"});

    Project demo = Project::createDefault();
    demo.name = "Demo project";
    demo.width = 1280;
    demo.height = 720;
    demo.fps = 30;
    demo.media = {clipA, clipB, music, logo};

    const QString v2 = demo.tracks[0].id;
    const QString v1 = demo.tracks[1].id;
    const QString a1 = demo.tracks[2].id;

    Clip a;
    a.track = v1;
    a.kind = ClipKind::Video;
    a.path = clipA;
    a.name = "clipA.mp4";
    a.start = 0;
    a.duration = 5;
    a.srcIn = 1;
    a.srcDur = 8;
    a.fadeIn = 0.5;
    a.fadeOut = 0.5;
    a.contrast = 1.08;
    a.saturation = 1.15;
    demo.clips << a;

    Clip b;
    b.track = v1;
    b.kind = ClipKind::Video;
    b.path = clipB;
    b.name = "clipB.mp4";
    b.start = 5;
    b.duration = 4;
    b.srcDur = 6;
    b.speed = 1.5;
    b.fit = "cover";
    b.fadeIn = 0.4;
    demo.clips << b;

    Clip img;
    img.track = v2;
    img.kind = ClipKind::Image;
    img.path = logo;
    img.name = "logo.png";
    img.start = 2;
    img.duration = 4;
    img.scale = 0.32;
    img.x = 310;
    img.y = -150;
    img.rotation = 8;
    img.opacity = 0.9;
    demo.clips << img;

    Clip txt;
    txt.track = v2;
    txt.kind = ClipKind::Text;
    txt.text = "Made with VideoCut Studio";
    txt.start = 0.5;
    txt.duration = 5;
    txt.fontSize = 58;
    txt.color = "#ffe066";
    txt.posY = 0.16;
    demo.clips << txt;

    Clip musicClip;
    musicClip.track = a1;
    musicClip.kind = ClipKind::Audio;
    musicClip.path = music;
    musicClip.name = "music.mp3";
    musicClip.start = 0;
    musicClip.duration = 9;
    musicClip.srcDur = 12;
    musicClip.volume = 0.55;
    musicClip.fadeIn = 1.0;
    musicClip.fadeOut = 1.5;
    demo.clips << musicClip;

    demo.sortClips();
    loadProject(demo, QString());
    {
        auto tracks = m_project.videoTracks();
        if (!tracks.isEmpty()) {
            auto clips = m_project.clipsOn(tracks.last()->id);
            if (clips.size() >= 2) {
                m_timeline->addTransitionBetween(clips[0]->id, clips[1]->id, "dissolve", 0.5);
                log(QString("transition added: %1 transition(s), %2 clips, timeline %3s")
                        .arg(m_project.transitions.size())
                        .arg(m_project.clips.size())
                        .arg(m_project.duration(), 0, 'f', 2));
            }
        }
        for (Clip &c : m_project.clips) {
            if (c.kind == ClipKind::Text) {
                c.fontFamily = "Segoe UI";
                c.italic = true;
                c.textAnim = "typewriter";
            }
        }
    }
    m_timeline->selectIds({m_project.clips[1].id});
    m_timeline->setPlayhead(2.5);
    log(QString("project loaded: %1 clips, %2s").arg(m_project.clips.size())
            .arg(m_project.duration(), 0, 'f', 2));

    QTimer::singleShot(1200, this, [this, log]() {
        m_timeline->setPlayhead(2.5);
        onSeek(2.5);
        log(QString("preview frame ready at 2.5s, size %1x%2")
                .arg(m_preview->frameSize().width()).arg(m_preview->frameSize().height()));
    });
    QTimer::singleShot(3000, this, [this, outDir, log]() {
        grab().save(outDir + "/screenshot_paused.png");
        log("screenshot_paused.png saved");
    });
    QTimer::singleShot(4000, this, [this, log]() {
        togglePlay();
        log(QString("playing: %1").arg(m_preview->engine()->isPlaying() ? "yes" : "no"));
    });
    QTimer::singleShot(7500, this, [this, outDir, log]() {
        log(QString("position after playback: %1s, frames: %2")
                .arg(m_preview->engine()->position(), 0, 'f', 2)
                .arg(m_preview->engine()->frameCount()));
        grab().save(outDir + "/screenshot_play.png");
        log("screenshot_play.png saved");
        if (m_preview->engine()->isPlaying()) m_preview->engine()->pause();
        grab().save(outDir + "/screenshot.png");
        log("screenshot.png saved");
        onSeek(4.72);
        QCoreApplication::processEvents();
        grab().save(outDir + "/screenshot_transition.png");
        log("screenshot_transition.png saved");
        onSeek(0.78);
        QCoreApplication::processEvents();
        grab().save(outDir + "/screenshot_typewriter.png");
        log("screenshot_typewriter.png saved");

        RenderOptions opts;
        opts.width = 1280;
        opts.height = 720;
        opts.fps = 30;
        opts.crf = 26;
        opts.preset = "veryfast";
        auto *renderer = new Renderer(this);
        connect(renderer, &Renderer::finished, this, [this, log](const QString &file) {
            log(QString("export: %1 %2 bytes").arg(file).arg(QFileInfo(file).size()));
            log("SELFTEST OK");
            log("closing window");
            m_dirty = false;
            close();
        });
        connect(renderer, &Renderer::failed, this, [this, log](const QString &message) {
            log("SELFTEST FAILED: " + message);
            m_dirty = false;
            close();
        });
        renderer->start(m_project, outDir + "/selftest_export.mp4", opts);
        log("export started");
    });
}

#include "MainWindow.moc"
