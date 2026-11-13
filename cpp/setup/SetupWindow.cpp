#include "SetupWindow.h"

#include "WinShortcuts.h"

#include <QApplication>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDate>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QVBoxLayout>

namespace {

const char *kAccent = "#ff3b46";
const char *kText = "#f5f3f4";
const char *kMuted = "#a89a9c";
const char *kBorder = "#2e1519";
const char *kAppId = "VideoCut.Studio";

QString versionString()
{
#ifdef VIDEOCUT_VERSION
    return QStringLiteral(VIDEOCUT_VERSION);
#else
    return QStringLiteral("1.0.0");
#endif
}

} // namespace

SetupWindow::SetupWindow(const QString &payloadPath, QWidget *parent)
    : QWidget(parent), m_payloadPath(payloadPath)
{
    setWindowTitle(tr("VideoCut Studio Setup"));
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setFixedSize(620, 400);
    setAttribute(Qt::WA_TranslucentBackground, false);

    m_version = versionString();

    QString error;
    if (!m_payload.open(m_payloadPath, &error)) {
        // The payload is missing: keep the UI but show the failure.
        m_version = versionString();
    } else {
        m_total = m_payload.totalBytes();
        if (!m_payload.version().isEmpty()) m_version = m_payload.version();
    }

    m_logo = new QLabel;
    m_logo->setFixedSize(56, 56);
    m_logo->setPixmap(QPixmap(":/logo.png").scaled(56, 56, Qt::KeepAspectRatio,
                                                   Qt::SmoothTransformation));
    m_logo->setAttribute(Qt::WA_TransparentForMouseEvents);

    m_title = new QLabel(tr("VideoCut Studio"));
    m_title->setStyleSheet(QString("color:%1; font-size:26px; font-weight:800;").arg(kText));

    m_subtitle = new QLabel(tr("Free offline video editor  ·  version %1").arg(m_version));
    m_subtitle->setStyleSheet(QString("color:%1; font-size:12px;").arg(kMuted));

    m_path = new QLabel;
    m_path->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_path->setStyleSheet(QString("color:%1; font-size:11px;").arg(kMuted));
    m_path->setWordWrap(true);

    m_license = new QLabel(tr(
        "By installing you agree to the "
        "<a style='color:%1' href='https://github.com/unknowndevelopment-ld/videocut-studio/blob/main/legal/TERMS.md'>Terms of Service</a>"
        " and "
        "<a style='color:%1' href='https://github.com/unknowndevelopment-ld/videocut-studio/blob/main/legal/PRIVACY.md'>Privacy Policy</a>."
        "  No account, no telemetry, your footage stays on this PC.").arg(kAccent));
    m_license->setOpenExternalLinks(true);
    m_license->setWordWrap(true);
    m_license->setStyleSheet(QString("color:%1; font-size:11px;").arg(kMuted));

    m_accept = new QCheckBox(tr("I accept the terms"));
    m_accept->setChecked(true);
    m_accept->setStyleSheet(QString("color:%1;").arg(kText));

    m_desktop = new QCheckBox(tr("Desktop shortcut"));
    m_desktop->setChecked(true);
    m_desktop->setStyleSheet(QString("color:%1;").arg(kText));

    m_progress = new QProgressBar;
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_progress->setTextVisible(false);
    m_progress->setFixedHeight(6);
    m_progress->hide();

    m_status = new QLabel;
    m_status->setStyleSheet(QString("color:%1; font-size:11px;").arg(kMuted));
    m_status->setWordWrap(true);

    m_install = new QPushButton(tr("Install"));
    m_install->setObjectName("Primary");
    m_install->setFixedSize(150, 40);
    m_install->setCursor(Qt::PointingHandCursor);
    m_install->setStyleSheet(QString(
        "QPushButton { background:%1; color:#fff; border:none; border-radius:10px;"
        "  font-size:14px; font-weight:700; }"
        "QPushButton:hover { background:#ff5962; }"
        "QPushButton:disabled { background:#3a1a1e; color:#8a6a6d; }").arg(kAccent));
    connect(m_install, &QPushButton::clicked, this, &SetupWindow::install);

    m_launch = new QPushButton(tr("Launch VideoCut Studio"));
    m_launch->setFixedSize(190, 40);
    m_launch->setCursor(Qt::PointingHandCursor);
    m_launch->setStyleSheet(QString(
        "QPushButton { background:%1; color:#fff; border:none; border-radius:10px;"
        "  font-size:13px; font-weight:700; }"
        "QPushButton:hover { background:#ff5962; }").arg(kAccent));
    m_launch->hide();
    connect(m_launch, &QPushButton::clicked, this, &SetupWindow::launchApp);

    m_close = new QPushButton(QString::fromUtf8("\u00d7"));
    m_close->setFixedSize(28, 28);
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setStyleSheet(QString(
        "QPushButton { background:transparent; color:%1; font-size:18px; border:none;"
        "  border-radius:14px; }"
        "QPushButton:hover { background:#2a1114; color:#fff; }").arg(kMuted));
    connect(m_close, &QPushButton::clicked, this, &QWidget::close);

    auto *headerText = new QVBoxLayout;
    headerText->setSpacing(2);
    headerText->addWidget(m_title);
    headerText->addWidget(m_subtitle);
    headerText->addStretch(1);

    auto *header = new QHBoxLayout;
    header->setSpacing(14);
    header->addWidget(m_logo, 0, Qt::AlignTop);
    header->addLayout(headerText, 1);
    header->addWidget(m_close, 0, Qt::AlignTop);

    auto *checks = new QHBoxLayout;
    checks->setSpacing(18);
    checks->addWidget(m_accept);
    checks->addWidget(m_desktop);
    checks->addStretch(1);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_launch);
    buttons->addStretch(1);
    buttons->addWidget(m_install);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(28, 24, 28, 22);
    layout->setSpacing(12);
    layout->addLayout(header);
    layout->addSpacing(6);
    layout->addWidget(m_path);
    layout->addWidget(m_license);
    layout->addLayout(checks);
    layout->addStretch(1);
    layout->addWidget(m_status);
    layout->addWidget(m_progress);
    layout->addLayout(buttons);

    m_path->setText(tr("Installs to %1").arg(QDir::toNativeSeparators(installDir())));
    setState(State::Welcome);

    if (!m_payload.isOpen()) {
        m_install->setEnabled(false);
        m_status->setText(tr("This file is not a complete installer (payload missing)."));
        m_status->setStyleSheet("color:#ff7b7b; font-size:11px;");
    }
}

SetupWindow::~SetupWindow() = default;

QString SetupWindow::installDir() const
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    return base + "/Programs/VideoCut Studio";
}

void SetupWindow::setState(State state)
{
    switch (state) {
    case State::Welcome:
        m_progress->hide();
        m_install->show();
        m_install->setText(tr("Install"));
        m_install->setEnabled(true);
        m_launch->hide();
        m_accept->show();
        m_desktop->show();
        break;
    case State::Working:
        m_working = true;
        m_progress->show();
        m_install->setEnabled(false);
        m_install->setText(tr("Installing…"));
        m_accept->hide();
        m_desktop->hide();
        m_launch->hide();
        break;
    case State::Done:
        m_working = false;
        m_progress->hide();
        m_install->hide();
        m_launch->show();
        m_status->setText(tr("VideoCut Studio %1 is installed.").arg(m_version));
        m_status->setStyleSheet("color:#8fe3b0; font-size:12px;");
        break;
    case State::Failed:
        m_working = false;
        m_progress->hide();
        m_install->setEnabled(true);
        m_install->setText(tr("Retry"));
        break;
    }
}

void SetupWindow::setProgress(qint64 done, qint64 total)
{
    if (total <= 0) return;
    m_progress->setValue(int(done * 1000 / total));
    m_status->setText(tr("Installing… %1%  (%2 of %3 files)")
                          .arg(done * 100 / qMax<qint64>(1, total))
                          .arg(m_payload.fileCount())
                          .arg(m_payload.fileCount()));
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
}

void SetupWindow::showError(const QString &message)
{
    m_working = false;
    setState(State::Failed);
    m_status->setText(message);
    m_status->setStyleSheet("color:#ff7b7b; font-size:12px;");
}

void SetupWindow::install()
{
    if (m_working) return;
    if (!m_accept->isChecked()) {
        m_status->setText(tr("Please accept the terms to continue."));
        m_status->setStyleSheet("color:#ffb84d; font-size:11px;");
        return;
    }
    setState(State::Working);
    m_status->setStyleSheet(QString("color:%1; font-size:11px;").arg(kMuted));
    m_status->setText(tr("Preparing…"));
    QCoreApplication::processEvents();

    const QString dir = installDir();
    QDir target(dir);
    if (target.exists() && !target.removeRecursively()) {
        showError(tr("Could not replace the existing installation in %1.\n"
                     "Close VideoCut Studio and try again.")
                      .arg(QDir::toNativeSeparators(dir)));
        return;
    }
    if (!QDir().mkpath(dir)) {
        showError(tr("Could not create %1").arg(QDir::toNativeSeparators(dir)));
        return;
    }

    qint64 done = 0;
    for (const Payload::Entry &entry : m_payload.entries()) {
        const QString path = dir + "/" + entry.path;
        if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
            showError(tr("Could not create %1").arg(QFileInfo(path).absolutePath()));
            return;
        }
        QString error;
        const QByteArray data = m_payload.read(entry, &error);
        if (data.isEmpty() && entry.size > 0) {
            showError(error);
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
            showError(tr("Could not write %1").arg(QDir::toNativeSeparators(path)));
            return;
        }
        file.close();
        done += entry.size;
        setProgress(done, m_total);
    }

    makeShortcuts();
    writeRegistry();

    setState(State::Done);
    m_status->setText(tr("VideoCut Studio %1 is installed.  %2 files, %3 MB.")
                          .arg(m_version)
                          .arg(m_payload.fileCount())
                          .arg(qRound64(m_total / 1024.0 / 1024.0)));
    m_status->setStyleSheet("color:#8fe3b0; font-size:12px;");
}

void SetupWindow::makeShortcuts()
{
    const QString dir = installDir();
    const QString exe = dir + "/VideoCutStudio.exe";
    const QString root = WinShortcuts::startMenuDir();
    const QString folder = root + "/VideoCut Studio";
    QDir().mkpath(folder);

    // Start Menu folder, the way a normal Windows app installs itself: the app
    // plus its own uninstall entry.
    WinShortcuts::createShortcut(folder + "/VideoCut Studio.lnk", exe, QString(), dir,
                                 tr("Free offline video editor"), exe, kAppId);
    // Distinct AppUserModelID: sharing the app's ID would make Windows treat
    // the two entries as one app and hide the launcher from the Start Menu.
    WinShortcuts::createShortcut(folder + "/Uninstall VideoCut Studio.lnk",
                                 dir + "/Uninstall.exe", QString(), dir,
                                 tr("Remove VideoCut Studio from this PC"), exe,
                                 QStringLiteral("VideoCut.Studio.Uninstall"));
    // Single top-level shortcut from earlier versions.
    QFile::remove(root + "/VideoCut Studio.lnk");

    if (m_desktop && m_desktop->isChecked()) {
        const QString desktop = WinShortcuts::desktopDir() + "/VideoCut Studio.lnk";
        WinShortcuts::createShortcut(desktop, exe, QString(), dir,
                                     tr("Free offline video editor"), exe, kAppId);
    } else {
        QFile::remove(WinShortcuts::desktopDir() + "/VideoCut Studio.lnk");
    }
}

void SetupWindow::writeRegistry()
{
    const QString dir = installDir();
    const QString exe = dir + "/VideoCutStudio.exe";
    const qint64 sizeKb = m_total / 1024;

    {
        QSettings uninstall("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\VideoCutStudio",
                            QSettings::NativeFormat);
        uninstall.setValue("DisplayName", "VideoCut Studio");
        uninstall.setValue("DisplayVersion", m_version);
        uninstall.setValue("Publisher", "VideoCut");
        uninstall.setValue("Comments", "Free offline video editor");
        uninstall.setValue("DisplayIcon", "\"" + QDir::toNativeSeparators(exe) + "\",0");
        uninstall.setValue("InstallLocation", QDir::toNativeSeparators(dir));
        uninstall.setValue("UninstallString",
                           "\"" + QDir::toNativeSeparators(dir + "/Uninstall.exe") + "\"");
        uninstall.setValue("QuietUninstallString",
                           "\"" + QDir::toNativeSeparators(dir + "/Uninstall.exe") + "\" /S");
        uninstall.setValue("InstallDate", QDate::currentDate().toString("yyyyMMdd"));
        uninstall.setValue("EstimatedSize", int(sizeKb));
        uninstall.setValue("NoModify", 1);
        uninstall.setValue("NoRepair", 1);
#ifdef VIDEOCUT_UPDATE_REPO
        const QString repo = QStringLiteral(VIDEOCUT_UPDATE_REPO);
        if (repo.contains('/') && repo != QStringLiteral("OWNER/REPO")) {
            const QString url = "https://github.com/" + repo;
            uninstall.setValue("URLInfoAbout", url);
            uninstall.setValue("HelpLink", url + "/issues");
        }
#endif
        uninstall.sync();
    }
    {
        // Lets Windows treat VideoCut Studio as a normal installed application
        // (Start Menu search, "Apps & features", Default apps, Open with).
        QSettings caps("HKEY_CURRENT_USER\\Software\\VideoCut\\VideoCutStudio\\Capabilities",
                       QSettings::NativeFormat);
        caps.setValue("ApplicationName", "VideoCut Studio");
        caps.setValue("ApplicationDescription", "Free offline video editor");
        caps.setValue("ApplicationIcon", "\"" + QDir::toNativeSeparators(exe) + "\",0");
        caps.setValue("FileAssociations/.vcs", "VideoCutStudio.Project");
        caps.sync();

        QSettings registered("HKEY_CURRENT_USER\\Software\\RegisteredApplications",
                             QSettings::NativeFormat);
        registered.setValue("VideoCut Studio", "Software\\VideoCut\\VideoCutStudio\\Capabilities");
        registered.sync();

        QSettings app("HKEY_CURRENT_USER\\Software\\Classes\\Applications\\VideoCutStudio.exe",
                      QSettings::NativeFormat);
        app.setValue("FriendlyAppName", "VideoCut Studio");
        app.setValue("ApplicationCompany", "VideoCut");
        app.setValue("SupportedTypes/.vcs", "");
        app.sync();

        // Right-click a video file -> Edit with VideoCut Studio.
        QSettings shell("HKEY_CURRENT_USER\\Software\\Classes\\SystemFileAssociations\\video\\shell\\VideoCutStudio",
                        QSettings::NativeFormat);
        shell.setValue(".", "Edit with VideoCut Studio");
        shell.setValue("Icon", "\"" + QDir::toNativeSeparators(exe) + "\",0");
        shell.setValue("command/.", "\"" + QDir::toNativeSeparators(exe) + "\" \"%1\"");
        shell.sync();
    }
    {
        QSettings ext("HKEY_CURRENT_USER\\Software\\Classes\\.vcs", QSettings::NativeFormat);
        ext.setValue(".", "VideoCutStudio.Project");
        ext.sync();

        QSettings prog("HKEY_CURRENT_USER\\Software\\Classes\\VideoCutStudio.Project",
                       QSettings::NativeFormat);
        prog.setValue(".", "VideoCut Studio project");
        prog.setValue("DefaultIcon/.", "\"" + QDir::toNativeSeparators(exe) + "\",0");
        prog.setValue("shell/open/command/.",
                      "\"" + QDir::toNativeSeparators(exe) + "\" \"%1\"");
        prog.sync();

        QSettings appPath("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\VideoCutStudio.exe",
                          QSettings::NativeFormat);
        appPath.setValue(".", QDir::toNativeSeparators(exe));
        appPath.setValue("Path", QDir::toNativeSeparators(dir));
        appPath.sync();
    }
}

void SetupWindow::launchApp()
{
    QProcess::startDetached(installDir() + "/VideoCutStudio.exe", {}, installDir());
    close();
}

int SetupWindow::runSilent()
{
    // Used by the updater: no UI, install over the existing copy.
    if (!m_payload.isOpen()) return 1;
    const QString dir = installDir();
    QDir(dir).removeRecursively();
    QDir().mkpath(dir);
    for (const Payload::Entry &entry : m_payload.entries()) {
        const QString path = dir + "/" + entry.path;
        QDir().mkpath(QFileInfo(path).absolutePath());
        QString err;
        const QByteArray data = m_payload.read(entry, &err);
        if (data.isEmpty() && entry.size > 0) return 1;
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) return 1;
    }
    makeShortcuts();
    writeRegistry();
    return 0;
}

void SetupWindow::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0.0, QColor("#0c0a0b"));
    background.setColorAt(1.0, QColor("#050405"));
    p.fillRect(rect(), background);

    // soft red glow behind the header
    QRadialGradient glow(QPointF(width() * 0.5, -60), width() * 0.9);
    glow.setColorAt(0.0, QColor(255, 59, 70, 60));
    glow.setColorAt(1.0, QColor(255, 59, 70, 0));
    p.fillRect(rect(), glow);

    p.setPen(QPen(QColor(kBorder), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(rect().adjusted(0, 0, -1, -1));

    // top accent line
    QLinearGradient line(0, 0, width(), 0);
    line.setColorAt(0.0, QColor(255, 59, 70, 0));
    line.setColorAt(0.5, QColor(255, 59, 70, 220));
    line.setColorAt(1.0, QColor(255, 59, 70, 0));
    p.fillRect(QRect(0, 0, width(), 2), line);
}

void SetupWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->position().y() < 90)
        m_dragOrigin = event->globalPosition().toPoint() - frameGeometry().topLeft();
    QWidget::mousePressEvent(event);
}

void SetupWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton && !m_dragOrigin.isNull())
        move(event->globalPosition().toPoint() - m_dragOrigin);
    QWidget::mouseMoveEvent(event);
}

void SetupWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && !m_working) close();
    QWidget::keyPressEvent(event);
}
