#include "Updater.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>

namespace {

QVector<int> parseVersion(const QString &text)
{
    QVector<int> parts;
    static const QRegularExpression digits("\\d+");
    auto it = digits.globalMatch(text);
    while (it.hasNext()) parts.append(it.next().captured().toInt());
    while (parts.size() < 3) parts.append(0);
    return parts;
}

bool isNewer(const QString &candidate, const QString &current)
{
    const QVector<int> a = parseVersion(candidate);
    const QVector<int> b = parseVersion(current);
    for (int i = 0; i < qMax(a.size(), b.size()); ++i) {
        const int av = a.value(i, 0);
        const int bv = b.value(i, 0);
        if (av != bv) return av > bv;
    }
    return false;
}

} // namespace

Updater::Updater(QObject *parent) : QObject(parent) {}

QString Updater::version()
{
#ifdef VIDEOCUT_VERSION
    return QStringLiteral(VIDEOCUT_VERSION);
#else
    return QStringLiteral("0.0.0");
#endif
}

QString Updater::repo()
{
    const QString override = QSettings("VideoCut", "VideoCutStudio")
                                 .value("update_repo").toString().trimmed();
    if (!override.isEmpty()) return override;
#ifdef VIDEOCUT_UPDATE_REPO
    return QStringLiteral(VIDEOCUT_UPDATE_REPO);
#else
    return QString();
#endif
}

bool Updater::configured()
{
    const QString r = repo();
    return !r.isEmpty() && r.contains('/') && !r.startsWith("OWNER");
}

void Updater::check(bool interactive)
{
    if (!configured()) {
        if (interactive)
            emit failed(tr("No update feed is configured for this build.\n\n"
                           "Set the GitHub repository in Settings → update_repo, "
                           "or rebuild with -DVIDEOCUT_UPDATE_REPO=owner/repo."));
        return;
    }
    m_interactive = interactive;

    QNetworkRequest request(QUrl("https://api.github.com/repos/" + repo() + "/releases/latest"));
    request.setHeader(QNetworkRequest::UserAgentHeader, "VideoCutStudio/" + version());
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    m_reply = m_net.get(request);
    connect(m_reply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (!reply) return;
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            if (m_interactive)
                emit failed(tr("Could not reach GitHub: %1").arg(reply->errorString()));
            return;
        }
        const QJsonObject release = QJsonDocument::fromJson(reply->readAll()).object();
        const QString tag = release.value("tag_name").toString();
        const QString body = release.value("body").toString();
        if (tag.isEmpty()) {
            if (m_interactive) emit failed(tr("Unexpected response from GitHub."));
            return;
        }

        QString url;
        for (const auto &value : release.value("assets").toArray()) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value("name").toString();
            if (!name.endsWith(".msi", Qt::CaseInsensitive)) continue;
            if (url.isEmpty() || name.contains("x64", Qt::CaseInsensitive)) {
                url = asset.value("browser_download_url").toString();
                if (name.contains("x64", Qt::CaseInsensitive)) break;
            }
        }

        if (!isNewer(tag, version())) {
            if (m_interactive) emit upToDate();
            return;
        }
        if (url.isEmpty()) {
            if (m_interactive)
                emit failed(tr("Version %1 is out but it has no .msi attached.").arg(tag));
            return;
        }
        emit updateAvailable(tag, url, body);
    });
}

void Updater::download(const QString &url, const QString &version)
{
    m_pendingVersion = version;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QDir().mkpath(dir);
    const QString path = dir + "/VideoCutStudio-" + version + "-update.msi";
    m_file = new QFile(path);
    if (!m_file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit failed(tr("Could not write to %1").arg(path));
        delete m_file;
        m_file = nullptr;
        return;
    }

    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, "VideoCutStudio/" + this->version());
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_net.get(request);

    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_reply && m_file) m_file->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
        if (total > 0) emit downloadProgress(int(got * 100 / total));
    });
    connect(m_reply, &QNetworkReply::finished, this, [this, path]() {
        QNetworkReply *reply = m_reply;
        m_reply = nullptr;
        if (m_file) {
            if (reply && reply->bytesAvailable() > 0) m_file->write(reply->readAll());
            m_file->close();
            delete m_file;
            m_file = nullptr;
        }
        if (!reply) return;
        const bool ok = reply->error() == QNetworkReply::NoError;
        const QString message = reply->errorString();
        reply->deleteLater();
        if (!ok) {
            QFile::remove(path);
            emit failed(tr("Download failed: %1").arg(message));
            return;
        }
        emit downloadFinished(path, m_pendingVersion);
    });
}
