#include "Payload.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtEndian>

namespace {
constexpr int kTrailerSize = 72;
const char kMagic[6] = {'V', 'C', 'S', 'P', '1', '\0'};
}

Payload::~Payload()
{
    delete m_file;
}

bool Payload::open(const QString &executable, QString *error)
{
    m_file = new QFile(executable);
    if (!m_file->open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Could not open the installer file.");
        return false;
    }
    const qint64 size = m_file->size();
    if (size < kTrailerSize) {
        if (error) *error = QStringLiteral("The installer payload is missing.");
        return false;
    }
    m_file->seek(size - kTrailerSize);
    const QByteArray trailer = m_file->read(kTrailerSize);
    if (trailer.size() != kTrailerSize || trailer.left(5) != QByteArray(kMagic, 5)) {
        if (error) *error = QStringLiteral("The installer payload is corrupt (no trailer).");
        return false;
    }
    const quint64 payloadOffset = qFromBigEndian<quint64>(trailer.constData() + 24);
    const quint64 payloadSize = qFromBigEndian<quint64>(trailer.constData() + 32);
    if (qint64(payloadOffset + payloadSize) > size - kTrailerSize) {
        if (error) *error = QStringLiteral("The installer payload is truncated.");
        return false;
    }

    m_file->seek(qint64(payloadOffset));
    const QByteArray lengthBytes = m_file->read(4);
    if (lengthBytes.size() != 4) {
        if (error) *error = QStringLiteral("The installer index is missing.");
        return false;
    }
    const quint32 indexLength = qFromBigEndian<quint32>(lengthBytes.constData());
    const QByteArray indexBytes = m_file->read(indexLength);
    const QJsonObject root = QJsonDocument::fromJson(indexBytes).object();
    if (root.isEmpty()) {
        if (error) *error = QStringLiteral("The installer index is unreadable.");
        return false;
    }

    m_version = root.value("version").toString();
    m_entries.clear();
    m_total = 0;
    const qint64 blobBase = qint64(payloadOffset) + 4 + indexLength;
    for (const auto &value : root.value("files").toArray()) {
        const QJsonObject object = value.toObject();
        Entry entry;
        entry.path = object.value("p").toString();
        entry.offset = blobBase + qint64(object.value("o").toDouble());
        entry.size = qint64(object.value("s").toDouble());
        entry.packed = qint64(object.value("c").toDouble());
        m_total += entry.size;
        m_entries.append(entry);
    }
    m_open = true;
    return true;
}

QByteArray Payload::read(const Entry &entry, QString *error)
{
    if (!m_file || !m_file->seek(entry.offset)) {
        if (error) *error = QStringLiteral("Could not read %1").arg(entry.path);
        return {};
    }
    const QByteArray packed = m_file->read(entry.packed);
    if (packed.size() != entry.packed) {
        if (error) *error = QStringLiteral("Short read for %1").arg(entry.path);
        return {};
    }
    const QByteArray data = qUncompress(packed);
    if (data.isEmpty() && entry.size > 0) {
        if (error) *error = QStringLiteral("Could not decompress %1").arg(entry.path);
        return {};
    }
    return data;
}
