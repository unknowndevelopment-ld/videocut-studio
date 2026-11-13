#pragma once

#include <QNetworkAccessManager>
#include <QObject>

class QNetworkReply;
class QFile;

/*  Checks a GitHub repository's latest release and installs its MSI.
 *
 *  The repository is baked in at build time (VIDEOCUT_UPDATE_REPO) and can be
 *  overridden in QSettings ("update_repo").  Everything is opt-in: the app only
 *  contacts api.github.com when you ask it to check (or once, quietly, on
 *  startup, if a repository is configured).                              */
class Updater : public QObject {
    Q_OBJECT
public:
    explicit Updater(QObject *parent = nullptr);

    static QString version();
    static QString repo();
    static bool configured();

    void check(bool interactive);
    void download(const QString &url, const QString &version);

signals:
    void upToDate();
    void updateAvailable(const QString &version, const QString &url, const QString &notes);
    void downloadProgress(int percent);
    void downloadFinished(const QString &file, const QString &version);
    void failed(const QString &message);

private:
    QNetworkAccessManager m_net;
    QNetworkReply *m_reply = nullptr;
    QFile *m_file = nullptr;
    bool m_interactive = false;
    QString m_pendingVersion;
};
