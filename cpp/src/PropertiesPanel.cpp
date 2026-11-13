#include "PropertiesPanel.h"

#include "Theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QVBoxLayout>
#include <cmath>

namespace {

struct LookPreset {
    const char *id;
    const char *name;
    double brightness;
    double contrast;
    double saturation;
    double hue;
    bool vignette;
};

const LookPreset kLooks[] = {
    {"none", QT_TR_NOOP("None"), 0.0, 1.0, 1.0, 0.0, false},
    {"cinematic", QT_TR_NOOP("Cinematic"), -0.02, 1.16, 0.90, -4.0, true},
    {"vivid", QT_TR_NOOP("Vivid"), 0.01, 1.10, 1.35, 0.0, false},
    {"bw", QT_TR_NOOP("Black & white"), 0.0, 1.12, 0.0, 0.0, true},
    {"warm", QT_TR_NOOP("Warm"), 0.02, 1.05, 1.10, -8.0, false},
    {"cool", QT_TR_NOOP("Cool"), 0.0, 1.05, 1.05, 10.0, false},
    {"vintage", QT_TR_NOOP("Vintage"), 0.05, 0.92, 0.72, 2.0, true},
};

constexpr int kLookCount = int(sizeof(kLooks) / sizeof(kLooks[0]));

} // namespace

// ------------------------------------------------------------------ SliderRow

SliderRow::SliderRow(double lo, double hi, int decimals, const QString &suffix, QWidget *parent)
    : QWidget(parent), m_decimals(decimals), m_factor(std::pow(10.0, decimals))
{
    m_slider = new QSlider(Qt::Horizontal);
    m_slider->setRange(int(std::lround(lo * m_factor)), int(std::lround(hi * m_factor)));
    m_slider->setMinimumWidth(Theme::px(70));

    m_spin = new QDoubleSpinBox;
    m_spin->setDecimals(decimals);
    m_spin->setRange(lo, hi);
    m_spin->setSingleStep(1.0 / m_factor);
    m_spin->setSuffix(suffix);
    m_spin->setFixedWidth(Theme::px(78));
    m_spin->setAlignment(Qt::AlignRight);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(Theme::px(8));
    layout->addWidget(m_slider, 1);
    layout->addWidget(m_spin);

    connect(m_slider, &QSlider::valueChanged, this, [this](int v) {
        if (m_guard) return;
        m_guard = true;
        const double val = v / m_factor;
        m_spin->setValue(val);
        m_guard = false;
        emit valueChanged(val);
    });
    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        if (m_guard) return;
        m_guard = true;
        m_slider->setValue(int(std::lround(v * m_factor)));
        m_guard = false;
        emit valueChanged(v);
    });
}

double SliderRow::value() const
{
    return m_spin->value();
}

void SliderRow::setValue(double value)
{
    if (m_guard) return;
    m_guard = true;
    value = qBound(m_spin->minimum(), value, m_spin->maximum());
    m_spin->setValue(value);
    m_slider->setValue(int(std::lround(value * m_factor)));
    m_guard = false;
}

// ------------------------------------------------------------ PropertiesPanel

PropertiesPanel::PropertiesPanel(QWidget *parent) : QFrame(parent)
{
    setObjectName("Panel");
    setAccessibleName(tr("Properties"));

    m_title = new QLabel(tr("PROPERTIES"));
    m_title->setObjectName("PanelTitle");

    m_content = new QWidget;
    m_content->setAccessibleName(tr("Properties form"));
    m_form = new QVBoxLayout(m_content);
    m_form->setContentsMargins(Theme::px(12), 0, Theme::px(12), Theme::px(14));
    m_form->setSpacing(Theme::px(6));

    m_scroll = new QScrollArea;
    m_scroll->setWidgetResizable(true);
    m_scroll->setWidget(m_content);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(Theme::px(8), Theme::px(8), Theme::px(8), Theme::px(8));
    layout->setSpacing(0);
    layout->addWidget(m_title);
    layout->addWidget(m_scroll, 1);
}

void PropertiesPanel::setProject(Project *project)
{
    m_project = project;
    m_clips.clear();
    rebuild();
}

void PropertiesPanel::setClips(const QVector<Clip *> &clips)
{
    QStringList before, after;
    for (Clip *c : m_clips) before << c->id;
    for (Clip *c : clips) after << c->id;
    m_clips = clips;
    if (before == after) {
        syncValues();
        return;
    }
    rebuild();
}

QString PropertiesPanel::idsKey() const
{
    QStringList ids;
    for (Clip *c : m_clips) ids << c->id;
    ids.sort();
    return ids.join(',');
}

void PropertiesPanel::clearForm()
{
    m_syncers.clear();
    while (QLayoutItem *item = m_form->takeAt(0)) {
        if (QWidget *w = item->widget()) w->deleteLater();
        delete item;
    }
}

void PropertiesPanel::rebuild()
{
    clearForm();
    if (!m_project) return;
    if (m_clips.isEmpty()) buildProjectForm();
    else buildClipForm();
    m_form->addStretch(1);
}

void PropertiesPanel::addHeader(const QString &text)
{
    auto *label = new QLabel(text);
    label->setStyleSheet(QString("color:%1; font-weight:700; font-size:%2px; padding-top:2px;")
                             .arg(Theme::accent().name()).arg(int(std::lround(11 * Theme::scale))));
    m_form->addWidget(label);
}

void PropertiesPanel::addSection(const QString &text)
{
    auto *label = new QLabel(text.toUpper());
    label->setObjectName("Section");
    m_form->addWidget(label);
}

void PropertiesPanel::addRow(const QString &label, QWidget *field)
{
    auto *host = new QWidget;
    auto *h = new QHBoxLayout(host);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(Theme::px(8));
    auto *l = new QLabel(label);
    l->setFixedWidth(Theme::px(80));
    l->setStyleSheet(QString("color:%1;").arg(Theme::muted().name()));
    l->setBuddy(field);
    h->addWidget(l);
    h->addWidget(field, 1);
    m_form->addWidget(host);
}

void PropertiesPanel::addWidgetRow(QWidget *widget)
{
    m_form->addWidget(widget);
}

void PropertiesPanel::applyToClips(const QString &label, const QString &key,
                                   const std::function<void(Clip &)> &fn)
{
    if (m_syncing || m_clips.isEmpty()) return;
    emit undoPoint(label, key + ":" + idsKey());
    for (Clip *c : m_clips) fn(*c);
    emit changed();
}

void PropertiesPanel::syncValues()
{
    if (m_syncing) return;
    m_syncing = true;
    for (auto &fn : m_syncers) {
        try { fn(); } catch (...) {}
    }
    m_syncing = false;
}

// --------------------------------------------------------------- project form

void PropertiesPanel::buildProjectForm()
{
    if (!m_project) return;
    Project *p = m_project;
    addHeader(tr("Project settings"));

    auto *name = new QLineEdit(p->name);
    name->setAccessibleName(tr("Project name"));
    connect(name, &QLineEdit::editingFinished, this, [this, name]() {
        if (m_syncing) return;
        emit undoPoint(tr("Project settings"), "proj:name");
        m_project->name = name->text();
        emit changed();
    });
    addRow(tr("Name"), name);

    auto *preset = new QComboBox;
    preset->setAccessibleName(tr("Project resolution"));
    const QVector<QPair<QString, QSize>> presets = {
        {tr("1920 x 1080  (16:9)"), {1920, 1080}},
        {tr("1280 x 720  (16:9)"), {1280, 720}},
        {tr("1080 x 1920  (vertical)"), {1080, 1920}},
        {tr("1080 x 1080  (square)"), {1080, 1080}},
        {tr("3840 x 2160  (4K)"), {3840, 2160}},
        {tr("854 x 480"), {854, 480}},
    };
    int current = -1;
    for (int i = 0; i < presets.size(); ++i) {
        preset->addItem(presets[i].first, presets[i].second);
        if (presets[i].second == QSize(p->width, p->height)) current = i;
    }
    if (current < 0) {
        preset->addItem(QString("%1 x %2  (custom)").arg(p->width).arg(p->height),
                        QSize(p->width, p->height));
        current = preset->count() - 1;
    }
    preset->setCurrentIndex(current);
    connect(preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, preset](int) {
        if (m_syncing || !m_project) return;
        const QSize size = preset->currentData().toSize();
        emit undoPoint(tr("Change resolution"), "proj:resolution");
        m_project->width = size.width();
        m_project->height = size.height();
        emit changed();
    });
    addRow(tr("Resolution"), preset);

    auto *fps = new QComboBox;
    fps->setAccessibleName(tr("Frame rate"));
    const QVector<double> rates = {23.976, 24, 25, 29.97, 30, 50, 60};
    for (double r : rates) fps->addItem(QString("%1 fps").arg(r, 0, 'g', 5), r);
    int fpsIndex = -1;
    for (int i = 0; i < rates.size(); ++i)
        if (qAbs(rates[i] - p->fps) < 1e-3) fpsIndex = i;
    if (fpsIndex < 0) {
        fps->addItem(QString("%1 fps").arg(p->fps, 0, 'g', 5), p->fps);
        fpsIndex = fps->count() - 1;
    }
    fps->setCurrentIndex(fpsIndex);
    connect(fps, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, fps](int) {
        if (m_syncing || !m_project) return;
        emit undoPoint(tr("Change frame rate"), "proj:fps");
        m_project->fps = fps->currentData().toDouble();
        emit changed();
    });
    addRow(tr("Frame rate"), fps);

    auto *bg = new QPushButton(p->background);
    bg->setAccessibleName(tr("Background colour"));
    auto styleBg = [bg](const QString &c) {
        bg->setText(c);
        bg->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3; border-radius:7px; padding:5px;")
                              .arg(c, QColor(c).lightness() > 150 ? "#000" : "#fff",
                                   Theme::border().name()));
    };
    styleBg(p->background);
    connect(bg, &QPushButton::clicked, this, [this, bg, styleBg]() {
        const QColor chosen = QColorDialog::getColor(QColor(m_project->background), this,
                                                     tr("Background colour"));
        if (!chosen.isValid()) return;
        emit undoPoint(tr("Background colour"), "proj:bg");
        m_project->background = chosen.name();
        styleBg(chosen.name());
        emit changed();
    });
    addRow(tr("Background"), bg);

    addSection(tr("Overview"));
    auto *stats = new QLabel(QString("%1 clips · %2 tracks\n%3 x %4 @ %5 fps")
                                 .arg(m_project->clips.size())
                                 .arg(m_project->tracks.size())
                                 .arg(m_project->width)
                                 .arg(m_project->height)
                                 .arg(m_project->fps, 0, 'g', 5));
    stats->setStyleSheet(QString("color:%1;").arg(Theme::muted().name()));
    addWidgetRow(stats);

    auto *hint = new QLabel(tr("Select a clip to edit it.\nPress Alt+L for a keyboard-friendly clip list."));
    hint->setWordWrap(true);
    hint->setStyleSheet(QString("color:%1; padding-top:12px;").arg(Theme::muted().name()));
    addWidgetRow(hint);
}

// ------------------------------------------------------------------ clip form

void PropertiesPanel::buildClipForm()
{
    Clip *clip = first();
    if (!clip) return;

    addHeader(m_clips.size() > 1
                  ? tr("%1 clips selected").arg(m_clips.size())
                  : tr("%1 clip").arg(clip->kind == ClipKind::Video ? tr("Video")
                                         : clip->kind == ClipKind::Audio ? tr("Audio")
                                         : clip->kind == ClipKind::Image ? tr("Image")
                                         : clip->kind == ClipKind::Shape ? tr("Shape")
                                                                         : tr("Text")));

    if (clip->kind == ClipKind::Text) buildTextForm(clip);
    else if (clip->kind == ClipKind::Shape) buildShapeForm(clip);
    else {
        auto *name = new QLineEdit(clip->name);
        name->setAccessibleName(tr("Clip name"));
        connect(name, &QLineEdit::editingFinished, this, [this, name]() {
            const QString value = name->text();
            applyToClips(tr("Rename clip"), "name", [value](Clip &c) { c.name = value; });
        });
        addRow(tr("Name"), name);
    }

    addSection(tr("Timing"));
    auto *start = new QDoubleSpinBox;
    start->setRange(0, 360000);
    start->setDecimals(2);
    start->setSuffix(" s");
    start->setValue(clip->start);
    start->setAccessibleName(tr("Start time"));
    connect(start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        applyToClips(tr("Change start"), "start", [v](Clip &c) { c.start = v; });
    });
    addRow(tr("Start"), start);
    m_syncers.append([start, this]() { if (first()) start->setValue(first()->start); });

    auto *dur = new QDoubleSpinBox;
    dur->setRange(0.04, 360000);
    dur->setDecimals(2);
    dur->setSuffix(" s");
    dur->setValue(clip->duration);
    dur->setAccessibleName(tr("Duration"));
    connect(dur, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double v) {
        applyToClips(tr("Change duration"), "duration", [v](Clip &c) { c.duration = v; });
    });
    addRow(tr("Duration"), dur);
    m_syncers.append([dur, this]() { if (first()) dur->setValue(first()->duration); });

    if (clip->kind != ClipKind::Image && clip->kind != ClipKind::Text
        && clip->kind != ClipKind::Shape) {
        auto *speed = new SliderRow(0.1, 8.0, 2, "x");
        speed->setValue(clip->speed);
        connect(speed, &SliderRow::valueChanged, this, [this](double v) {
            applyToClips(tr("Change speed"), "speed", [v](Clip &c) { c.speed = v; });
        });
        addRow(tr("Speed"), speed);
        m_syncers.append([speed, this]() { if (first()) speed->setValue(first()->speed); });
    }

    if (clip->kind != ClipKind::Audio) {
        addSection(tr("Transform"));
        auto *fit = new QComboBox;
        fit->addItem(tr("Contain"), "contain");
        fit->addItem(tr("Cover"), "cover");
        fit->addItem(tr("Stretch"), "stretch");
        fit->setCurrentIndex(qMax(0, fit->findData(clip->fit)));
        fit->setAccessibleName(tr("Scaling mode"));
        connect(fit, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, fit](int) {
            const QString value = fit->currentData().toString();
            applyToClips(tr("Change fit"), "fit", [value](Clip &c) { c.fit = value; });
        });
        addRow(tr("Fit"), fit);

        struct Field { const char *label; double lo; double hi; int dec; const char *suffix; double Clip::*member; };
        const QVector<Field> fields = {
            {QT_TR_NOOP("Scale"), 0.05, 4.0, 2, "x", &Clip::scale},
            {QT_TR_NOOP("Position X"), -2000, 2000, 0, " px", &Clip::x},
            {QT_TR_NOOP("Position Y"), -2000, 2000, 0, " px", &Clip::y},
            {QT_TR_NOOP("Rotation"), -180, 180, 0, "°", &Clip::rotation},
            {QT_TR_NOOP("Opacity"), 0.0, 1.0, 2, "", &Clip::opacity},
        };
        for (const Field &f : fields) {
            auto *row = new SliderRow(f.lo, f.hi, f.dec, f.suffix);
            row->setValue(clip->*f.member);
            auto member = f.member;
            const QString label = tr(f.label);
            connect(row, &SliderRow::valueChanged, this, [this, member, label](double v) {
                applyToClips(tr("Change ") + label.toLower(), label, [member, v](Clip &c) { c.*member = v; });
            });
            addRow(label, row);
            m_syncers.append([row, this, member]() { if (first()) row->setValue(first()->*member); });
        }
    }

    if (clip->kind != ClipKind::Audio && clip->kind != ClipKind::Shape) {
        addSection(tr("Colour & effects"));

        auto *look = new QComboBox;
        look->setAccessibleName(tr("Look preset"));
        for (const LookPreset &preset : kLooks)
            look->addItem(tr(preset.name), QString::fromLatin1(preset.id));
        int lookIndex = 0;
        for (int i = 0; i < kLookCount; ++i) {
            const LookPreset &p = kLooks[i];
            if (qAbs(clip->brightness - p.brightness) < 0.005
                && qAbs(clip->contrast - p.contrast) < 0.005
                && qAbs(clip->saturation - p.saturation) < 0.005
                && qAbs(clip->hue - p.hue) < 0.5
                && clip->vignette == p.vignette) {
                lookIndex = i;
                break;
            }
        }
        look->setCurrentIndex(lookIndex);
        connect(look, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, look](int index) {
            if (m_syncing || index < 0 || index >= kLookCount) return;
            const LookPreset &p = kLooks[index];
            emit undoPoint(tr("Apply look"), "look");
            for (Clip *c : m_clips) {
                c->brightness = p.brightness;
                c->contrast = p.contrast;
                c->saturation = p.saturation;
                c->hue = p.hue;
                c->vignette = p.vignette;
            }
            emit changed();
        });
        addRow(tr("Look"), look);

        struct Field2 { const char *label; double lo; double hi; int dec; double Clip::*member; };
        const QVector<Field2> looks = {
            {QT_TR_NOOP("Brightness"), -0.5, 0.5, 2, &Clip::brightness},
            {QT_TR_NOOP("Contrast"), 0.2, 2.5, 2, &Clip::contrast},
            {QT_TR_NOOP("Saturation"), 0.0, 2.5, 2, &Clip::saturation},
            {QT_TR_NOOP("Hue"), -180, 180, 0, &Clip::hue},
            {QT_TR_NOOP("Blur"), 0.0, 30.0, 1, &Clip::blur},
            {QT_TR_NOOP("Sharpen"), 0.0, 2.0, 2, &Clip::sharpen},
        };
        for (const Field2 &f : looks) {
            auto *row = new SliderRow(f.lo, f.hi, f.dec, "");
            row->setValue(clip->*f.member);
            auto member = f.member;
            const QString label = tr(f.label);
            connect(row, &SliderRow::valueChanged, this,
                    [this, member, label](double v) {
                        applyToClips(tr("Change ") + label.toLower(), label, [member, v](Clip &c) { c.*member = v; });
                    });
            addRow(label, row);
            m_syncers.append([row, this, member]() { if (first()) row->setValue(first()->*member); });
        }

        auto *vignette = new QCheckBox(tr("Vignette"));
        vignette->setChecked(clip->vignette);
        connect(vignette, &QCheckBox::toggled, this, [this](bool on) {
            applyToClips(tr("Toggle vignette"), "vignette", [on](Clip &c) { c.vignette = on; });
        });
        addRow(tr("Vignette"), vignette);
    }

    if (clip->kind != ClipKind::Image && clip->kind != ClipKind::Text) {
        addSection(tr("Audio"));
        auto *volume = new SliderRow(0.0, 2.0, 2, "");
        volume->setValue(clip->volume);
        connect(volume, &SliderRow::valueChanged, this, [this](double v) {
            applyToClips(tr("Change volume"), "volume", [v](Clip &c) { c.volume = v; });
        });
        addRow(tr("Volume"), volume);
        m_syncers.append([volume, this]() { if (first()) volume->setValue(first()->volume); });

        auto *mute = new QCheckBox(tr("Mute"));
        mute->setChecked(clip->mute);
        connect(mute, &QCheckBox::toggled, this, [this](bool on) {
            applyToClips(tr("Toggle mute"), "mute", [on](Clip &c) { c.mute = on; });
        });
        addRow(tr("Mute"), mute);
    }

    if (clip->kind != ClipKind::Audio) {
        addSection(tr("Fade"));
        auto *fadeIn = new SliderRow(0.0, 10.0, 2, " s");
        fadeIn->setValue(clip->fadeIn);
        connect(fadeIn, &SliderRow::valueChanged, this, [this](double v) {
            applyToClips(tr("Change fade in"), "fadeIn", [v](Clip &c) { c.fadeIn = v; });
        });
        addRow(tr("Fade in"), fadeIn);
        m_syncers.append([fadeIn, this]() { if (first()) fadeIn->setValue(first()->fadeIn); });

        auto *fadeOut = new SliderRow(0.0, 10.0, 2, " s");
        fadeOut->setValue(clip->fadeOut);
        connect(fadeOut, &SliderRow::valueChanged, this, [this](double v) {
            applyToClips(tr("Change fade out"), "fadeOut", [v](Clip &c) { c.fadeOut = v; });
        });
        addRow(tr("Fade out"), fadeOut);
        m_syncers.append([fadeOut, this]() { if (first()) fadeOut->setValue(first()->fadeOut); });
    }
}

void PropertiesPanel::buildShapeForm(Clip *clip)
{
    addSection(tr("Shape"));
    auto *type = new QComboBox;
    type->setAccessibleName(tr("Shape type"));
    const QVector<QPair<QString, QString>> kinds = {
        {"arrow", tr("Arrow")},     {"circle", tr("Circle")}, {"rect", tr("Rectangle")},
        {"line", tr("Line")},       {"triangle", tr("Triangle")}, {"star", tr("Star")},
        {"check", tr("Check mark")}, {"cross", tr("Cross")},
    };
    for (const auto &k : kinds) {
        type->addItem(Theme::icon(k.first, Theme::text(), Theme::px(16)), k.second, k.first);
    }
    type->setCurrentIndex(qMax(0, type->findData(clip->shapeType)));
    connect(type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, type](int) {
        if (m_syncing) return;
        const QString value = type->currentData().toString();
        applyToClips(tr("Change shape"), "shapeType", [value](Clip &c) { c.shapeType = value; });
    });
    addRow(tr("Type"), type);
    m_syncers.append([type, this]() {
        if (!first()) return;
        const int index = type->findData(first()->shapeType);
        if (index >= 0 && index != type->currentIndex()) type->setCurrentIndex(index);
    });

    auto *color = new QPushButton(clip->shapeColor);
    color->setAccessibleName(tr("Shape colour"));
    auto styleColor = [color](const QString &c) {
        color->setText(c);
        color->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3;"
                                     " border-radius:7px; padding:5px;")
                                 .arg(c, QColor(c).lightness() > 150 ? "#000" : "#fff",
                                      Theme::border().name()));
    };
    styleColor(clip->shapeColor);
    connect(color, &QPushButton::clicked, this, [this, color, styleColor]() {
        const QColor chosen = QColorDialog::getColor(QColor(first()->shapeColor), this,
                                                     tr("Shape colour"));
        if (!chosen.isValid()) return;
        const QString value = chosen.name();
        styleColor(value);
        applyToClips(tr("Change shape colour"), "shapeColor", [value](Clip &c) { c.shapeColor = value; });
    });
    addRow(tr("Colour"), color);

    auto *thickness = new SliderRow(2, 64, 0, " px");
    thickness->setValue(clip->shapeStroke);
    connect(thickness, &SliderRow::valueChanged, this, [this](double v) {
        applyToClips(tr("Change thickness"), "shapeStroke", [v](Clip &c) { c.shapeStroke = int(v); });
    });
    addRow(tr("Thickness"), thickness);
    m_syncers.append([thickness, this]() { if (first()) thickness->setValue(first()->shapeStroke); });

    auto *fill = new QCheckBox(tr("Fill"));
    fill->setChecked(clip->shapeFill);
    connect(fill, &QCheckBox::toggled, this, [this](bool on) {
        applyToClips(tr("Toggle shape fill"), "shapeFill", [on](Clip &c) { c.shapeFill = on; });
    });
    addRow(tr("Fill"), fill);
    m_syncers.append([fill, this]() { if (first()) fill->setChecked(first()->shapeFill); });

    auto *shadow = new QCheckBox(tr("Shadow"));
    shadow->setToolTip(tr("Dark edge so the shape stays readable on any footage"));
    shadow->setChecked(clip->shapeShadow);
    connect(shadow, &QCheckBox::toggled, this, [this](bool on) {
        applyToClips(tr("Toggle shape shadow"), "shapeShadow", [on](Clip &c) { c.shapeShadow = on; });
    });
    addRow(tr("Edge"), shadow);
    m_syncers.append([shadow, this]() { if (first()) shadow->setChecked(first()->shapeShadow); });
}

void PropertiesPanel::buildTextForm(Clip *clip)
{
    addSection(tr("Text"));
    auto *content = new QPlainTextEdit(clip->text);
    content->setFixedHeight(Theme::px(72));
    content->setAccessibleName(tr("Text content"));
    connect(content, &QPlainTextEdit::textChanged, this, [this, content]() {
        const QString value = content->toPlainText();
        applyToClips(tr("Edit text"), "text", [value](Clip &c) { c.text = value; });
    });
    addWidgetRow(content);

    auto *font = new QFontComboBox;
    font->setAccessibleName(tr("Font family"));
    const QString currentFamily = clip->fontFamily.isEmpty()
            ? QApplication::font().family() : clip->fontFamily;
    font->setCurrentFont(QFont(currentFamily));
    connect(font, &QFontComboBox::currentFontChanged, this, [this](const QFont &f) {
        if (m_syncing) return;
        const QString family = f.family();
        applyToClips(tr("Change font"), "fontFamily", [family](Clip &c) { c.fontFamily = family; });
    });
    addRow(tr("Font"), font);
    m_syncers.append([font, this]() {
        if (!first()) return;
        const QString family = first()->fontFamily.isEmpty()
                ? QApplication::font().family() : first()->fontFamily;
        if (font->currentFont().family() != family) font->setCurrentFont(QFont(family));
    });

    auto *loadFont = new QPushButton(tr("Load font file…"));
    loadFont->setObjectName("Ghost");
    loadFont->setToolTip(tr("Add a .ttf or .otf font to the editor"));
    connect(loadFont, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, tr("Load font"), QString(),
                                                          tr("Fonts (*.ttf *.otf *.ttc)"));
        if (file.isEmpty()) return;
        if (QFontDatabase::addApplicationFont(file) < 0) return;
        QSettings settings("VideoCut", "VideoCutStudio");
        QStringList fonts = settings.value("custom_fonts").toStringList();
        if (!fonts.contains(file)) fonts << file;
        settings.setValue("custom_fonts", fonts);
        rebuild();
    });
    addRow(QString(), loadFont);

    auto *size = new SliderRow(8, 300, 0, " px");
    size->setValue(clip->fontSize);
    connect(size, &SliderRow::valueChanged, this, [this](double v) {
        applyToClips(tr("Change text size"), "fontSize", [v](Clip &c) { c.fontSize = int(v); });
    });
    addRow(tr("Size"), size);
    m_syncers.append([size, this]() { if (first()) size->setValue(first()->fontSize); });

    auto *color = new QPushButton(clip->color);
    color->setAccessibleName(tr("Text colour"));
    auto styleColor = [color](const QString &c) {
        color->setText(c);
        color->setStyleSheet(QString("background:%1; color:%2; border:1px solid %3;"
                                     " border-radius:7px; padding:5px;")
                                 .arg(c, QColor(c).lightness() > 150 ? "#000" : "#fff",
                                      Theme::border().name()));
    };
    styleColor(clip->color);
    connect(color, &QPushButton::clicked, this, [this, color, styleColor]() {
        const QColor chosen = QColorDialog::getColor(QColor(first()->color), this, tr("Text colour"));
        if (!chosen.isValid()) return;
        const QString value = chosen.name();
        styleColor(value);
        applyToClips(tr("Change text colour"), "color", [value](Clip &c) { c.color = value; });
    });
    addRow(tr("Colour"), color);

    auto *outline = new SliderRow(0, 16, 0, " px");
    outline->setValue(clip->outline);
    connect(outline, &SliderRow::valueChanged, this, [this](double v) {
        applyToClips(tr("Change outline"), "outline", [v](Clip &c) { c.outline = int(v); });
    });
    addRow(tr("Outline"), outline);
    m_syncers.append([outline, this]() { if (first()) outline->setValue(first()->outline); });

    auto *bold = new QCheckBox(tr("Bold"));
    bold->setChecked(clip->bold);
    connect(bold, &QCheckBox::toggled, this, [this](bool on) {
        applyToClips(tr("Toggle bold"), "bold", [on](Clip &c) { c.bold = on; });
    });
    addRow(tr("Weight"), bold);

    auto *italic = new QCheckBox(tr("Italic"));
    italic->setChecked(clip->italic);
    connect(italic, &QCheckBox::toggled, this, [this](bool on) {
        applyToClips(tr("Toggle italic"), "italic", [on](Clip &c) { c.italic = on; });
    });
    addRow(tr("Style"), italic);

    auto *anim = new QComboBox;
    anim->setAccessibleName(tr("Text animation"));
    anim->addItem(tr("None"), "none");
    anim->addItem(tr("Fade in"), "fade");
    anim->addItem(tr("Pop"), "pop");
    anim->addItem(tr("Slide up"), "slideUp");
    anim->addItem(tr("Slide down"), "slideDown");
    anim->addItem(tr("Slide left"), "slideLeft");
    anim->addItem(tr("Slide right"), "slideRight");
    anim->addItem(tr("Typewriter"), "typewriter");
    anim->setCurrentIndex(qMax(0, anim->findData(clip->textAnim)));
    connect(anim, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, anim](int) {
        if (m_syncing) return;
        const QString value = anim->currentData().toString();
        if (value == "fade") {
            applyToClips(tr("Text animation"), "textAnim",
                         [](Clip &c) { c.textAnim = "none"; c.fadeIn = 0.45; });
        } else {
            applyToClips(tr("Text animation"), "textAnim", [value](Clip &c) { c.textAnim = value; });
        }
    });
    addRow(tr("Animate"), anim);

    addSection(tr("Position"));
    auto *posX = new SliderRow(0.0, 1.0, 2, "");
    posX->setValue(clip->posX);
    connect(posX, &SliderRow::valueChanged, this, [this](double v) {
        applyToClips(tr("Move text"), "posX", [v](Clip &c) { c.posX = v; });
    });
    addRow(tr("Pos X"), posX);
    m_syncers.append([posX, this]() { if (first()) posX->setValue(first()->posX); });

    auto *posY = new SliderRow(0.0, 1.0, 2, "");
    posY->setValue(clip->posY);
    connect(posY, &SliderRow::valueChanged, this, [this](double v) {
        applyToClips(tr("Move text"), "posY", [v](Clip &c) { c.posY = v; });
    });
    addRow(tr("Pos Y"), posY);
    m_syncers.append([posY, this]() { if (first()) posY->setValue(first()->posY); });
}
