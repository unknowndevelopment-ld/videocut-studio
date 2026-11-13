#include "MediaBin.h"

#include "MediaUtils.h"
#include "Project.h"
#include "Theme.h"
#include "util.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

namespace {

class MediaList : public QListWidget {
public:
    explicit MediaList(QWidget *parent = nullptr) : QListWidget(parent) {}

protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        auto *data = new QMimeData;
        QStringList paths;
        for (const auto *item : items) {
            const QString path = item->data(Qt::UserRole).toString();
            if (!path.isEmpty()) paths << path;
        }
        data->setData("application/x-videocut-media", paths.join('\n').toUtf8());
        QList<QUrl> urls;
        for (const auto &p : paths) urls << QUrl::fromLocalFile(p);
        data->setUrls(urls);
        return data;
    }
};

} // namespace

MediaBin::MediaBin(QWidget *parent) : QWidget(parent)
{
    setObjectName("Panel");
    setAccessibleName(tr("Media bin"));

    auto *title = new QLabel(tr("MEDIA"));
    title->setObjectName("PanelTitle");

    auto *import = new QPushButton(tr("Import"));
    import->setIcon(Theme::icon("plus", Theme::text(), Theme::px(15)));
    import->setIconSize(QSize(Theme::px(15), Theme::px(15)));
    import->setToolTip(tr("Import video, audio or images (Ctrl+I)"));
    import->setAccessibleName(tr("Import media"));
    connect(import, &QPushButton::clicked, this, &MediaBin::importRequested);

    auto *add = new QPushButton(tr("Add to timeline"));
    add->setObjectName("Ghost");
    add->setToolTip(tr("Insert the selected media at the playhead"));
    add->setAccessibleName(tr("Add selected media to the timeline"));
    connect(add, &QPushButton::clicked, this, [this]() {
        const QStringList paths = selectedPaths();
        if (!paths.isEmpty()) emit addRequested(paths);
    });

    m_list = new MediaList;
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(Theme::px(140), Theme::px(78)));
    m_list->setGridSize(QSize(Theme::px(158), Theme::px(124)));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setWordWrap(true);
    m_list->setUniformItemSizes(true);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setAccessibleName(tr("Media files"));
    m_list->setAccessibleDescription(tr("Press Enter to add the selected file to the timeline."));
    connect(m_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) {
        const QStringList paths = selectedPaths();
        if (!paths.isEmpty()) emit addRequested(paths);
    });
    connect(m_list, &QListWidget::customContextMenuRequested, this, &MediaBin::showContextMenu);

    m_hint = new QLabel(tr("Import video, audio or images"));
    m_hint->setObjectName("Hint");
    m_hint->setAlignment(Qt::AlignCenter);

    auto *row = new QHBoxLayout;
    row->setContentsMargins(Theme::px(8), Theme::px(2), Theme::px(8), Theme::px(2));
    row->setSpacing(Theme::px(6));
    row->addWidget(import, 1);
    row->addWidget(add, 1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Theme::px(8), Theme::px(8), Theme::px(8), Theme::px(8));
    layout->setSpacing(Theme::px(6));
    layout->addWidget(title);
    layout->addLayout(row);
    layout->addWidget(m_list, 1);
    layout->addWidget(m_hint);
}

void MediaBin::setProject(Project *project)
{
    m_project = project;
    rebuild();
}

void MediaBin::rebuild()
{
    m_list->clear();
    if (!m_project) return;
    for (const QString &path : m_project->media) {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        const MediaInfo info = MediaUtils::probe(path);
        item->setToolTip(QString("%1\n%2 x %3  %4")
                             .arg(path).arg(info.width).arg(info.height)
                             .arg(humanDuration(info.duration)));
        item->setIcon(iconFor(path));
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        m_list->addItem(item);
    }
    m_hint->setText(m_list->count() ? tr("Drag clips onto the timeline")
                                    : tr("Import video, audio or images"));
}

QIcon MediaBin::iconFor(const QString &path)
{
    const int w = Theme::px(140);
    const int h = Theme::px(78);
    QPixmap canvas(w * 2, h * 2);
    canvas.setDevicePixelRatio(2);
    canvas.fill(Qt::transparent);
    QPainter p(&canvas);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    const QString poster = MediaUtils::poster(path, 160);
    if (!poster.isEmpty()) {
        QPixmap pm(poster);
        if (!pm.isNull()) {
            pm = pm.scaled(w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            p.drawPixmap((w - pm.width()) / 2, (h - pm.height()) / 2, pm);
            p.setPen(QPen(Theme::border(), 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 6, 6);
            p.end();
            return QIcon(canvas);
        }
    }
    p.setPen(Qt::NoPen);
    p.setBrush(Theme::panelHi());
    p.drawRoundedRect(QRectF(0, 0, w, h), 6, 6);
    const MediaInfo info = MediaUtils::probe(path);
    p.drawPixmap(w / 2 - Theme::px(11), h / 2 - Theme::px(11),
                 Theme::icon(info.hasVideo ? "image" : "music", Theme::muted(), Theme::px(22))
                     .pixmap(Theme::px(22), Theme::px(22)));
    p.end();
    return QIcon(canvas);
}

void MediaBin::addPaths(const QStringList &paths)
{
    if (!m_project) return;
    QStringList added;
    for (const QString &raw : paths) {
        const QString path = QDir::cleanPath(QFileInfo(raw).absoluteFilePath());
        if (!QFileInfo::exists(path) || !MediaUtils::isMediaFile(path)) continue;
        if (m_project->media.contains(path)) continue;
        m_project->media.append(path);
        added << path;
    }
    if (added.isEmpty()) return;
    rebuild();
    refreshIcons();
    emit mediaAdded(added);
}

void MediaBin::refreshIcons()
{
    for (int i = 0; i < m_list->count(); ++i) {
        auto *item = m_list->item(i);
        item->setIcon(iconFor(item->data(Qt::UserRole).toString()));
    }
}

QStringList MediaBin::selectedPaths() const
{
    QStringList out;
    for (auto *item : m_list->selectedItems()) out << item->data(Qt::UserRole).toString();
    return out;
}

int MediaBin::count() const
{
    return m_list->count();
}

void MediaBin::showContextMenu(const QPoint &pos)
{
    auto *item = m_list->itemAt(pos);
    QMenu menu(this);
    if (item) {
        const QString path = item->data(Qt::UserRole).toString();
        menu.addAction(tr("Add to timeline"), this, [this, path]() { emit addRequested({path}); });
        menu.addAction(tr("Reveal in Explorer"), this, [path]() {
            QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
        });
        menu.addSeparator();
        menu.addAction(tr("Remove from media"), this, [this, path]() {
            if (!m_project) return;
            m_project->media.removeAll(path);
            rebuild();
        });
        menu.addSeparator();
    }
    menu.addAction(tr("Import files…"), this, &MediaBin::importRequested);
    menu.exec(m_list->mapToGlobal(pos));
}
