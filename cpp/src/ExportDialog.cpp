#include "ExportDialog.h"

#include "MediaUtils.h"
#include "Theme.h"
#include "util.h"

#include <QAccessible>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSettings>
#include <QSlider>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

ExportDialog::ExportDialog(const Project &project, QWidget *parent)
    : QDialog(parent), m_project(project)
{
    setWindowTitle(tr("Export video"));
    setMinimumWidth(Theme::px(580));
    setModal(true);
    setAccessibleName(tr("Export video dialog"));

    QSettings settings("VideoCut", "VideoCutStudio");
    const QString lastDir = settings.value("export_dir", MediaUtils::defaultExportDir()).toString();
    QString stem = m_project.name;
    stem.replace(QRegularExpression("[^A-Za-z0-9 _-]"), "");
    if (stem.trimmed().isEmpty()) stem = "export";
    m_output = MediaUtils::uniquePath(lastDir, stem.trimmed(), ".mp4");

    auto *title = new QLabel(tr("Export video"));
    title->setStyleSheet(QString("font-size:%1px; font-weight:700;")
                             .arg(int(std::lround(18 * Theme::scale))));

    m_path = new QLineEdit(m_output);
    m_path->setAccessibleName(tr("Output file path"));
    auto *browseButton = new QPushButton(tr("Browse…"));
    browseButton->setAccessibleName(tr("Choose output file"));
    connect(browseButton, &QPushButton::clicked, this, &ExportDialog::browse);
    auto *pathRow = new QHBoxLayout;
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(browseButton);

    m_resolution = new QComboBox;
    m_resolution->setAccessibleName(tr("Output resolution"));
    m_resolution->addItem(QString("Project (%1 x %2)").arg(m_project.width).arg(m_project.height),
                          QSize(m_project.width, m_project.height));
    const QVector<QPair<QString, QSize>> presets = {
        {tr("1080p  (1920 x 1080)"), {1920, 1080}},
        {tr("720p  (1280 x 720)"), {1280, 720}},
        {tr("480p  (854 x 480)"), {854, 480}},
        {tr("4K  (3840 x 2160)"), {3840, 2160}},
        {tr("Vertical 1080 x 1920"), {1080, 1920}},
        {tr("Square 1080 x 1080"), {1080, 1080}},
    };
    for (const auto &p : presets)
        if (p.second != QSize(m_project.width, m_project.height))
            m_resolution->addItem(p.first, p.second);

    m_fps = new QComboBox;
    m_fps->setAccessibleName(tr("Output frame rate"));
    for (double r : QVector<double>{23.976, 24, 25, 30, 50, 60})
        m_fps->addItem(QString("%1").arg(r, 0, 'g', 5), r);
    int best = 0;
    double bestDelta = 1e9;
    for (int i = 0; i < m_fps->count(); ++i) {
        const double d = qAbs(m_fps->itemData(i).toDouble() - m_project.fps);
        if (d < bestDelta) { bestDelta = d; best = i; }
    }
    m_fps->setCurrentIndex(best);

    m_format = new QComboBox;
    m_format->setAccessibleName(tr("Output format"));
    m_format->addItem(tr("MP4  ·  H.264  (best compatibility)"), "mp4");
    m_format->addItem(tr("WebM  ·  VP9  (smaller files)"), "webm");
    m_format->addItem(tr("MKV  ·  H.264"), "mkv");
    m_format->addItem(tr("GIF  ·  animated image"), "gif");
    connect(m_format, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::onFormatChanged);

    m_quality = new QSlider(Qt::Horizontal);
    m_quality->setRange(1, 10);
    m_quality->setValue(8);
    m_quality->setAccessibleName(tr("Quality"));
    m_qualityLabel = new QLabel(tr("High"));
    m_qualityLabel->setFixedWidth(Theme::px(64));
    auto *qualityRow = new QHBoxLayout;
    qualityRow->addWidget(m_quality, 1);
    qualityRow->addWidget(m_qualityLabel);

    m_summary = new QLabel;
    m_summary->setObjectName("Hint");
    m_summary->setWordWrap(true);

    m_progress = new QProgressBar;
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_progress->hide();
    m_progress->setAccessibleName(tr("Export progress"));

    m_status = new QLabel;
    m_status->setObjectName("Hint");

    auto *form = new QFormLayout;
    form->setSpacing(Theme::px(10));
    form->addRow(tr("File"), pathRow);
    form->addRow(tr("Resolution"), m_resolution);
    form->addRow(tr("Frame rate"), m_fps);
    form->addRow(tr("Format"), m_format);
    form->addRow(tr("Quality"), qualityRow);

    m_export = new QPushButton(tr("Export video"));
    m_export->setObjectName("Primary");
    m_export->setAccessibleName(tr("Start export"));
    connect(m_export, &QPushButton::clicked, this, &ExportDialog::startExport);

    m_close = new QPushButton(tr("Cancel"));
    connect(m_close, &QPushButton::clicked, this, [this]() {
        if (m_renderer && m_renderer->isRunning()) {
            m_renderer->cancel();
            QTimer::singleShot(300, this, &QDialog::reject);
        } else {
            reject();
        }
    });

    m_open = new QPushButton(tr("Open folder"));
    m_open->setObjectName("Ghost");
    m_open->hide();
    connect(m_open, &QPushButton::clicked, this, &ExportDialog::openFolder);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_open);
    buttons->addStretch(1);
    buttons->addWidget(m_close);
    buttons->addWidget(m_export);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Theme::px(20), Theme::px(18), Theme::px(20), Theme::px(16));
    layout->setSpacing(Theme::px(12));
    layout->addWidget(title);
    layout->addLayout(form);
    layout->addWidget(m_summary);
    layout->addWidget(m_progress);
    layout->addWidget(m_status);
    layout->addLayout(buttons);

    connect(m_resolution, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::updateSummary);
    connect(m_fps, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::updateSummary);
    connect(m_quality, &QSlider::valueChanged, this, &ExportDialog::updateSummary);
    connect(m_path, &QLineEdit::textChanged, this, &ExportDialog::updateSummary);
    updateSummary();
}

RenderOptions ExportDialog::options() const
{
    RenderOptions o;
    const QSize size = m_resolution->currentData().toSize();
    o.width = size.width();
    o.height = size.height();
    o.fps = (m_format->currentData().toString() == "gif") ? 15.0 : m_fps->currentData().toDouble();
    const int q = m_quality->value();
    o.crf = qBound(12, int(std::lround(34 - q * 2.2)), 34);
    // x264's slower presets buy very little at these CRFs but cost a lot of time.
    o.preset = q >= 9 ? "medium" : q >= 6 ? "faster" : "veryfast";
    o.format = m_format->currentData().toString();
    return o;
}

void ExportDialog::updateSummary()
{
    const RenderOptions o = options();
    static const QStringList labels = {"Tiny", "Tiny", "Small", "Small", "Medium",
                                       "Medium", "Good", "High", "High", "Best"};
    m_qualityLabel->setText(labels.value(m_quality->value() - 1, "High"));
    m_summary->setText(tr("%1 · %2x%3 · %4 fps · %5 · estimated %6")
                           .arg(humanDuration(m_project.duration()))
                           .arg(o.width).arg(o.height)
                           .arg(o.fps, 0, 'g', 5)
                           .arg(o.format.toUpper())
                           .arg(Renderer::estimateSize(m_project, o)));
}

void ExportDialog::onFormatChanged()
{
    const QString fmt = m_format->currentData().toString();
    QFileInfo info(m_path->text());
    m_path->setText(info.absolutePath() + "/" + info.completeBaseName() + "." + fmt);
    m_fps->setEnabled(fmt != "gif");
    updateSummary();
}

void ExportDialog::browse()
{
    const QString fmt = m_format->currentData().toString();
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export video"), m_path->text(),
        QString("%1 files (*.%2);;All files (*.*)").arg(fmt.toUpper(), fmt));
    if (path.isEmpty()) return;
    m_path->setText(path.endsWith("." + fmt, Qt::CaseInsensitive) ? path : path + "." + fmt);
}

void ExportDialog::startExport()
{
    const QString path = m_path->text().trimmed();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Export"), tr("Choose an output file first."));
        return;
    }
    if (m_project.duration() <= 0) {
        QMessageBox::warning(this, tr("Export"), tr("The timeline is empty — add some clips first."));
        return;
    }
    QSettings("VideoCut", "VideoCutStudio").setValue("export_dir", QFileInfo(path).absolutePath());
    m_output = path;

    setBusy(true);
    m_progress->show();
    m_progress->setValue(0);
    m_status->setText(tr("Preparing timeline…"));

    m_renderer = new Renderer(this);
    connect(m_renderer, &Renderer::progress, this, [this](double done, double total) {
        const double ratio = total <= 0 ? 0 : qBound(0.0, done / total, 1.0);
        m_progress->setValue(int(ratio * 1000));
        m_status->setText(tr("Rendering… %1%   (%2 / %3)")
                              .arg(ratio * 100, 0, 'f', 1)
                              .arg(timecode(done), timecode(total)));
    });
    connect(m_renderer, &Renderer::finished, this, [this](const QString &file) {
        m_progress->setValue(1000);
        m_status->setText(tr("Done — %1  %2").arg(QFileInfo(file).fileName(),
                                                  humanSize(QFileInfo(file).size())));
        m_export->setText(tr("Export again"));
        m_open->show();
        m_close->setText(tr("Close"));
        setBusy(false);
        QAccessibleEvent ev(m_status, QAccessible::Alert);
        QAccessible::updateAccessibility(&ev);
    });
    connect(m_renderer, &Renderer::failed, this, [this](const QString &message) {
        m_progress->hide();
        m_status->setText(tr("Export failed"));
        setBusy(false);
        if (message != "Cancelled")
            QMessageBox::critical(this, tr("Export failed"), message.right(2000));
    });
    // Let the dialog repaint before the (short) graph build and process launch.
    QTimer::singleShot(0, this, [this]() {
        if (!m_renderer) return;
        m_status->setText(tr("Starting ffmpeg…"));
        m_renderer->start(m_project, m_output, options());
    });
}

void ExportDialog::setBusy(bool busy)
{
    m_export->setEnabled(!busy);
    m_path->setEnabled(!busy);
    m_resolution->setEnabled(!busy);
    m_fps->setEnabled(!busy && m_format->currentData().toString() != "gif");
    m_format->setEnabled(!busy);
    m_quality->setEnabled(!busy);
    if (!busy) m_close->setText(tr("Close"));
}

void ExportDialog::openFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_output).absolutePath()));
}
