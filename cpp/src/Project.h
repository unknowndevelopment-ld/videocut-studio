#pragma once

#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector>
#include <algorithm>
#include <cmath>

enum class ClipKind { Video, Image, Audio, Text };

inline QString kindName(ClipKind k)
{
    switch (k) {
    case ClipKind::Video: return "video";
    case ClipKind::Image: return "image";
    case ClipKind::Audio: return "audio";
    case ClipKind::Text: return "text";
    }
    return "video";
}

inline ClipKind kindFromName(const QString &s)
{
    if (s == "image") return ClipKind::Image;
    if (s == "audio") return ClipKind::Audio;
    if (s == "text") return ClipKind::Text;
    return ClipKind::Video;
}

inline QString newId(const QString &prefix)
{
    return prefix + QUuid::createUuid().toString(QUuid::WithoutBraces).left(12);
}

struct Clip {
    QString id = newId("clip");
    QString track;
    ClipKind kind = ClipKind::Video;
    QString path;
    QString name;
    double start = 0.0;
    double duration = 5.0;
    double srcIn = 0.0;
    double srcDur = 0.0;

    double speed = 1.0;
    double volume = 1.0;
    bool mute = false;

    QString fit = "contain";
    double scale = 1.0;
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0;
    double opacity = 1.0;

    double brightness = 0.0;
    double contrast = 1.0;
    double saturation = 1.0;
    double hue = 0.0;
    double blur = 0.0;
    double sharpen = 0.0;
    bool vignette = false;

    double fadeIn = 0.0;
    double fadeOut = 0.0;

    QString text = QStringLiteral("Your text here");
    int fontSize = 72;
    QString color = QStringLiteral("#ffffff");
    int outline = 4;
    bool bold = true;
    bool italic = false;
    QString fontFamily;
    QString textAnim = "none";      // none | fade | slideUp | slideDown
    double posX = 0.5;
    double posY = 0.5;

    double end() const { return start + duration; }
    double srcOut() const { return srcIn + duration * std::max(0.01, speed); }

    QString label() const
    {
        if (kind == ClipKind::Text) {
            QString first = text.split('\n').value(0);
            return first.left(40);
        }
        return name.isEmpty() ? path.section('/', -1) : name;
    }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["id"] = id;
        o["track"] = track;
        o["kind"] = kindName(kind);
        o["path"] = path;
        o["name"] = name;
        o["start"] = start;
        o["duration"] = duration;
        o["srcIn"] = srcIn;
        o["srcDur"] = srcDur;
        o["speed"] = speed;
        o["volume"] = volume;
        o["mute"] = mute;
        o["fit"] = fit;
        o["scale"] = scale;
        o["x"] = x;
        o["y"] = y;
        o["rotation"] = rotation;
        o["opacity"] = opacity;
        o["brightness"] = brightness;
        o["contrast"] = contrast;
        o["saturation"] = saturation;
        o["hue"] = hue;
        o["blur"] = blur;
        o["sharpen"] = sharpen;
        o["vignette"] = vignette;
        o["fadeIn"] = fadeIn;
        o["fadeOut"] = fadeOut;
        o["text"] = text;
        o["fontSize"] = fontSize;
        o["color"] = color;
        o["outline"] = outline;
        o["bold"] = bold;
        o["italic"] = italic;
        o["fontFamily"] = fontFamily;
        o["textAnim"] = textAnim;
        o["posX"] = posX;
        o["posY"] = posY;
        return o;
    }

    static Clip fromJson(const QJsonObject &o)
    {
        Clip c;
        c.id = o.value("id").toString(c.id);
        c.track = o.value("track").toString();
        c.kind = kindFromName(o.value("kind").toString());
        c.path = o.value("path").toString();
        c.name = o.value("name").toString();
        c.start = o.value("start").toDouble(0);
        c.duration = o.value("duration").toDouble(5);
        c.srcIn = o.value("srcIn").toDouble(0);
        c.srcDur = o.value("srcDur").toDouble(0);
        c.speed = o.value("speed").toDouble(1);
        c.volume = o.value("volume").toDouble(1);
        c.mute = o.value("mute").toBool(false);
        c.fit = o.value("fit").toString("contain");
        c.scale = o.value("scale").toDouble(1);
        c.x = o.value("x").toDouble(0);
        c.y = o.value("y").toDouble(0);
        c.rotation = o.value("rotation").toDouble(0);
        c.opacity = o.value("opacity").toDouble(1);
        c.brightness = o.value("brightness").toDouble(0);
        c.contrast = o.value("contrast").toDouble(1);
        c.saturation = o.value("saturation").toDouble(1);
        c.hue = o.value("hue").toDouble(0);
        c.blur = o.value("blur").toDouble(0);
        c.sharpen = o.value("sharpen").toDouble(0);
        c.vignette = o.value("vignette").toBool(false);
        c.fadeIn = o.value("fadeIn").toDouble(0);
        c.fadeOut = o.value("fadeOut").toDouble(0);
        c.text = o.value("text").toString(c.text);
        c.fontSize = o.value("fontSize").toInt(72);
        c.color = o.value("color").toString("#ffffff");
        c.outline = o.value("outline").toInt(4);
        c.bold = o.value("bold").toBool(true);
        c.italic = o.value("italic").toBool(false);
        c.fontFamily = o.value("fontFamily").toString();
        c.textAnim = o.value("textAnim").toString("none");
        c.posX = o.value("posX").toDouble(0.5);
        c.posY = o.value("posY").toDouble(0.5);
        return c;
    }
};

struct Track {
    QString id = newId("trk");
    QString name;
    ClipKind kind = ClipKind::Video;
    bool muted = false;
    bool hidden = false;
    bool locked = false;

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["id"] = id;
        o["name"] = name;
        o["kind"] = kindName(kind);
        o["muted"] = muted;
        o["hidden"] = hidden;
        o["locked"] = locked;
        return o;
    }

    static Track fromJson(const QJsonObject &o)
    {
        Track t;
        t.id = o.value("id").toString(t.id);
        t.name = o.value("name").toString();
        t.kind = kindFromName(o.value("kind").toString());
        t.muted = o.value("muted").toBool(false);
        t.hidden = o.value("hidden").toBool(false);
        t.locked = o.value("locked").toBool(false);
        return t;
    }
};

/*  A transition sits at the junction of two clips on one track.  The incoming
 *  clip overlaps the outgoing one by `duration` seconds; the type decides how
 *  the incoming clip enters (alpha ramp and/or slide offset).               */
struct Transition {
    QString id = newId("tr");
    QString track;
    QString fromClip;
    QString toClip;
    QString type = "dissolve";   // dissolve | black | slideLeft | slideRight | slideUp | slideDown
    double duration = 0.5;

    bool isSlide() const { return type.startsWith("slide"); }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["id"] = id;
        o["track"] = track;
        o["fromClip"] = fromClip;
        o["toClip"] = toClip;
        o["type"] = type;
        o["duration"] = duration;
        return o;
    }

    static Transition fromJson(const QJsonObject &o)
    {
        Transition t;
        t.id = o.value("id").toString(t.id);
        t.track = o.value("track").toString();
        t.fromClip = o.value("fromClip").toString();
        t.toClip = o.value("toClip").toString();
        t.type = o.value("type").toString("dissolve");
        t.duration = o.value("duration").toDouble(0.5);
        return t;
    }
};

class Project {
public:
    QString name = "Untitled";
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    QString background = "#000000";
    QVector<Track> tracks;
    QVector<Clip> clips;
    QVector<Transition> transitions;
    QStringList media;

    static Project createDefault()
    {
        Project p;
        p.tracks = {
            Track{newId("trk"), "V2", ClipKind::Video},
            Track{newId("trk"), "V1", ClipKind::Video},
            Track{newId("trk"), "A1", ClipKind::Audio},
            Track{newId("trk"), "A2", ClipKind::Audio},
        };
        return p;
    }

    Track *track(const QString &id)
    {
        for (auto &t : tracks)
            if (t.id == id) return &t;
        return nullptr;
    }

    Clip *clip(const QString &id)
    {
        for (auto &c : clips)
            if (c.id == id) return &c;
        return nullptr;
    }

    QVector<Clip *> clipsOn(const QString &trackId)
    {
        QVector<Clip *> out;
        for (auto &c : clips)
            if (c.track == trackId) out.append(&c);
        std::sort(out.begin(), out.end(), [](const Clip *a, const Clip *b) { return a->start < b->start; });
        return out;
    }

    QVector<Track *> videoTracks()
    {
        QVector<Track *> out;
        for (auto &t : tracks)
            if (t.kind == ClipKind::Video) out.append(&t);
        return out;
    }

    QVector<Track *> audioTracks()
    {
        QVector<Track *> out;
        for (auto &t : tracks)
            if (t.kind == ClipKind::Audio) out.append(&t);
        return out;
    }

    double duration() const
    {
        double d = 0;
        for (const auto &c : clips) d = std::max(d, c.end());
        return d;
    }

    bool hasAudio() const
    {
        for (const auto &c : clips)
            if (!c.mute && (c.kind == ClipKind::Video || c.kind == ClipKind::Audio)) return true;
        return false;
    }

    void removeClip(const QString &id)
    {
        for (int i = clips.size() - 1; i >= 0; --i)
            if (clips[i].id == id) clips.removeAt(i);
        for (int i = transitions.size() - 1; i >= 0; --i)
            if (transitions[i].fromClip == id || transitions[i].toClip == id)
                transitions.removeAt(i);
    }

    Transition *transitionFor(const QString &toClipId)
    {
        for (auto &t : transitions)
            if (t.toClip == toClipId) return &t;
        return nullptr;
    }

    void removeTransition(const QString &id)
    {
        for (int i = transitions.size() - 1; i >= 0; --i)
            if (transitions[i].id == id) transitions.removeAt(i);
    }

    Track *addTrack(ClipKind kind)
    {
        int count = 0;
        for (auto &t : tracks)
            if (t.kind == kind) count++;
        Track t;
        t.kind = kind;
        t.name = (kind == ClipKind::Video ? "V" : "A") + QString::number(count + 1);
        if (kind == ClipKind::Video) {
            int idx = videoTracks().size();
            tracks.insert(idx, t);
            return &tracks[idx];
        }
        tracks.append(t);
        return &tracks.last();
    }

    void removeTrack(const QString &id)
    {
        for (int i = tracks.size() - 1; i >= 0; --i)
            if (tracks[i].id == id) tracks.removeAt(i);
        for (int i = clips.size() - 1; i >= 0; --i)
            if (clips[i].track == id) clips.removeAt(i);
    }

    void sortClips()
    {
        QHash<QString, int> order;
        for (int i = 0; i < tracks.size(); ++i) order[tracks[i].id] = i;
        std::stable_sort(clips.begin(), clips.end(), [&](const Clip &a, const Clip &b) {
            const int ta = order.value(a.track, 99);
            const int tb = order.value(b.track, 99);
            if (ta != tb) return ta < tb;
            return a.start < b.start;
        });
    }

    QJsonObject toJson() const
    {
        QJsonObject o;
        o["format"] = "videocut-studio";
        o["version"] = 1;
        o["name"] = name;
        o["width"] = width;
        o["height"] = height;
        o["fps"] = fps;
        o["background"] = background;
        QJsonArray tr;
        for (const auto &t : tracks) tr.append(t.toJson());
        QJsonArray cl;
        for (const auto &c : clips) cl.append(c.toJson());
        QJsonArray trs;
        for (const auto &t : transitions) trs.append(t.toJson());
        QJsonArray md;
        for (const auto &m : media) md.append(m);
        o["tracks"] = tr;
        o["clips"] = cl;
        o["transitions"] = trs;
        o["media"] = md;
        return o;
    }

    static Project fromJson(const QJsonObject &o)
    {
        Project p;
        p.name = o.value("name").toString("Untitled");
        p.width = o.value("width").toInt(1920);
        p.height = o.value("height").toInt(1080);
        p.fps = o.value("fps").toDouble(30);
        p.background = o.value("background").toString("#000000");
        for (const auto &v : o.value("tracks").toArray()) p.tracks.append(Track::fromJson(v.toObject()));
        for (const auto &v : o.value("clips").toArray()) p.clips.append(Clip::fromJson(v.toObject()));
        for (const auto &v : o.value("transitions").toArray())
            p.transitions.append(Transition::fromJson(v.toObject()));
        for (const auto &v : o.value("media").toArray()) p.media.append(v.toString());
        if (p.tracks.isEmpty()) p = Project::createDefault();
        return p;
    }

    bool save(const QString &path) const
    {
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
        f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
        return true;
    }

    static Project load(const QString &path, bool *ok = nullptr)
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (ok) *ok = false;
            return createDefault();
        }
        QJsonParseError err{};
        auto doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (ok) *ok = err.error == QJsonParseError::NoError;
        return fromJson(doc.object());
    }
};

class History {
public:
    explicit History(Project &p) { m_project = &p; }

    void setProject(Project *p)
    {
        m_project = p;
        m_undo.clear();
        m_redo.clear();
        m_lastKey.clear();
    }

    void push(const QString &label, const QString &key = QString())
    {
        if (!key.isEmpty() && key == m_lastKey) return;
        m_lastKey = key;
        m_undo.append({label, m_project->toJson()});
        if (m_undo.size() > 200) m_undo.removeFirst();
        m_redo.clear();
    }

    bool canUndo() const { return !m_undo.isEmpty(); }
    bool canRedo() const { return !m_redo.isEmpty(); }

    QString undo()
    {
        if (m_undo.isEmpty()) return {};
        const auto entry = m_undo.takeLast();
        m_redo.append({entry.first, m_project->toJson()});
        *m_project = Project::fromJson(entry.second);
        m_lastKey.clear();
        return entry.first;
    }

    QString redo()
    {
        if (m_redo.isEmpty()) return {};
        const auto entry = m_redo.takeLast();
        m_undo.append({entry.first, m_project->toJson()});
        *m_project = Project::fromJson(entry.second);
        m_lastKey.clear();
        return entry.first;
    }

private:
    Project *m_project = nullptr;
    QVector<QPair<QString, QJsonObject>> m_undo;
    QVector<QPair<QString, QJsonObject>> m_redo;
    QString m_lastKey;
};
