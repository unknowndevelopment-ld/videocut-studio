#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

class QFile;

/*  Reads the payload appended to the setup executable.
 *
 *  Layout (see tools/make_payload.py):
 *      [setup.exe][uint32 indexLen][index json][packed blobs][56-byte trailer]
 *  Each blob is a qCompress-compatible zlib stream (4-byte BE size + deflate).
 */
class Payload {
public:
    struct Entry {
        QString path;
        qint64 offset = 0;
        qint64 size = 0;
        qint64 packed = 0;
    };

    ~Payload();

    bool open(const QString &executable, QString *error);
    bool isOpen() const { return m_open; }

    const QVector<Entry> &entries() const { return m_entries; }
    qint64 totalBytes() const { return m_total; }
    QString version() const { return m_version; }
    int fileCount() const { return m_entries.size(); }

    QByteArray read(const Entry &entry, QString *error);

private:
    QFile *m_file = nullptr;
    QVector<Entry> m_entries;
    qint64 m_total = 0;
    QString m_version;
    bool m_open = false;
};
