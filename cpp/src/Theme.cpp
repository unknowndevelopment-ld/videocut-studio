#include "Theme.h"

#include <QApplication>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QtMath>

namespace Theme {

double scale = 1.0;
bool contrast = false;

int px(double v)
{
    return int(std::lround(v * scale));
}

QColor bg()        { return contrast ? QColor("#000000") : QColor("#050506"); }
QColor panel()     { return contrast ? QColor("#0a0000") : QColor("#0c0c0f"); }
QColor panelAlt()  { return contrast ? QColor("#160000") : QColor("#141417"); }
QColor panelHi()   { return contrast ? QColor("#260000") : QColor("#1e1e22"); }
QColor border()    { return contrast ? QColor("#ffffff") : QColor("#2e1519"); }
QColor text()      { return contrast ? QColor("#ffffff") : QColor("#f5f3f4"); }
QColor muted()     { return contrast ? QColor("#ffd9dc") : QColor("#a89a9c"); }
QColor accent()    { return contrast ? QColor("#ff3b46") : QColor("#ff3b46"); }
QColor accentSoft(){ return contrast ? QColor("#5a0a11") : QColor("#3d0d12"); }
QColor danger()    { return contrast ? QColor("#ff8080") : QColor("#ff7b7b"); }
QColor warn()      { return contrast ? QColor("#ffd166") : QColor("#ffb84d"); }
QColor good()      { return contrast ? QColor("#ff9aa4") : QColor("#ff8a95"); }
QColor playhead()  { return contrast ? QColor("#ffffff") : QColor("#ff2d3a"); }

QString shortcutHint(const QString &key)
{
    return QStringLiteral("  (") + key + QStringLiteral(")");
}

void apply(QApplication *app, double scaleFactor, bool highContrast)
{
    scale = qBound(0.8, scaleFactor, 1.8);
    contrast = highContrast;

    QString family = "Segoe UI";
    const QStringList families = QFontDatabase::families();
    if (families.contains("Segoe UI Variable Text")) family = "Segoe UI Variable Text";
    else if (families.contains("Inter")) family = "Inter";
    else if (!families.contains("Segoe UI") && !families.isEmpty()) family = families.first();

    QFont font(family);
    font.setPointSizeF(9.0 * scale);
    font.setHintingPreference(QFont::PreferFullHinting);
    app->setFont(font);
    app->setStyleSheet(styleSheet());
}

QString styleSheet()
{
    const QString S = QStringLiteral(
        "QWidget { background: @BG@; color: @TEXT@; font-size: %1px; }"
        "QMainWindow, QDialog, QMessageBox { background: @BG@; }"
        "QToolTip { background: @PANELHI@; color: @TEXT@; border: 1px solid @BORDER@;"
        "  padding: 6px 8px; border-radius: 6px; }"

        "QFrame#Panel, QWidget#Panel { background: @PANEL@; border: 1px solid @BORDER@;"
        "  border-radius: 12px; }"
        "QFrame#Card { background: @PANELALT@; border: 1px solid @BORDER@; border-radius: 10px; }"
        "QLabel#PanelTitle { color: @MUTED@; font-size: %2px; font-weight: 700;"
        "  letter-spacing: 1.2px; padding: 2px 2px 6px 2px; }"
        "QLabel#Section { color: @MUTED@; font-size: %2px; font-weight: 700;"
        "  letter-spacing: 1px; padding-top: 12px; }"
        "QLabel#Hint { color: @MUTED@; }"
        "QLabel#Timecode { font-family: 'Consolas','Cascadia Mono',monospace; font-size: %3px;"
        "  background: @PANELALT@; border: 1px solid @BORDER@; border-radius: 7px; padding: 4px 10px; }"

        "QPushButton { background: @PANELALT@; border: 1px solid @BORDER@; border-radius: 8px;"
        "  padding: 7px 14px; color: @TEXT@; }"
        "QPushButton:hover { background: @PANELHI@; border-color: @BORDER@; }"
        "QPushButton:pressed { background: @PANEL@; }"
        "QPushButton:focus { border: 2px solid @ACCENT@; }"
        "QPushButton:disabled { color: @MUTED@; background: transparent; }"
        "QPushButton:checked { background: @ACCENTSOFT@; border-color: @ACCENT@; color: @TEXT@; }"
        "QPushButton#Primary { background: @ACCENT@; color: @ON_ACCENT@; border: 1px solid @ACCENT@;"
        "  font-weight: 700; padding: 8px 18px; border-radius: 8px; }"
        "QPushButton#Primary:hover { background: @ACCENTHOVER@; }"
        "QPushButton#Primary:disabled { background: @BORDER@; color: @MUTED@; }"
        "QPushButton#Tool { background: transparent; border: 1px solid transparent; padding: 6px 10px; }"
        "QPushButton#Tool:hover { background: @PANELHI@; border-color: @BORDER@; }"
        "QPushButton#Tool:checked { background: @ACCENTSOFT@; border-color: @ACCENT@; }"
        "QPushButton#Ghost { background: transparent; border: 1px solid transparent; padding: 4px 8px;"
        "  color: @MUTED@; }"
        "QPushButton#Ghost:hover { background: @PANELHI@; color: @TEXT@; }"

        "QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox, QPlainTextEdit {"
        "  background: @PANELALT@; border: 1px solid @BORDER@; border-radius: 7px; padding: 5px 8px;"
        "  selection-background-color: @ACCENT@; selection-color: @ON_ACCENT@; }"
        "QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus {"
        "  border: 2px solid @ACCENT@; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background: @PANELALT@; border: 1px solid @BORDER@;"
        "  selection-background-color: @ACCENTSOFT@; outline: none; padding: 4px; }"
        "QSpinBox::up-button, QSpinBox::down-button, QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {"
        "  width: 16px; background: @PANELHI@; border: none; }"

        "QCheckBox { spacing: 8px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; border-radius: 5px;"
        "  border: 1px solid @BORDER@; background: @PANELALT@; }"
        "QCheckBox::indicator:checked { background: @ACCENT@; border-color: @ACCENT@; }"
        "QCheckBox:focus { color: @ACCENT@; }"

        "QSlider::groove:horizontal { height: 5px; background: @BORDER@; border-radius: 3px; }"
        "QSlider::sub-page:horizontal { background: @ACCENT@; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: @TEXT@; width: 14px; height: 14px; margin: -5px 0;"
        "  border-radius: 7px; border: 2px solid @PANEL@; }"
        "QSlider::handle:horizontal:hover { background: @ACCENT@; }"
        "QSlider:focus { border: none; }"

        "QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: @BORDER@; min-height: 30px; border-radius: 4px; }"
        "QScrollBar::handle:vertical:hover { background: @MUTED@; }"
        "QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }"
        "QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }"
        "QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }"
        "QScrollBar::handle:horizontal { background: @BORDER@; min-width: 30px; border-radius: 4px; }"
        "QScrollBar::handle:horizontal:hover { background: @MUTED@; }"

        "QListWidget { background: transparent; border: none; outline: none; }"
        "QListWidget::item { border-radius: 9px; margin: 3px; padding: 3px; }"
        "QListWidget::item:selected { background: @ACCENTSOFT@; border: 1px solid @ACCENT@; }"
        "QListWidget::item:hover { background: @PANELHI@; }"
        "QListWidget:focus { border: 1px solid @ACCENT@; border-radius: 8px; }"

        "QMenuBar { background: @PANEL@; border-bottom: 1px solid @BORDER@; padding: 2px; }"
        "QMenuBar::item { padding: 6px 12px; background: transparent; border-radius: 6px; }"
        "QMenuBar::item:selected { background: @PANELHI@; }"
        "QMenu { background: @PANELALT@; border: 1px solid @BORDER@; padding: 6px; border-radius: 10px; }"
        "QMenu::item { padding: 7px 30px 7px 24px; border-radius: 6px; }"
        "QMenu::item:selected { background: @ACCENTSOFT@; }"
        "QMenu::separator { height: 1px; background: @BORDER@; margin: 5px 10px; }"

        "QSplitter::handle { background: transparent; }"
        "QSplitter::handle:hover { background: @ACCENTSOFT@; }"
        "QSplitter::handle:horizontal { width: 6px; }"
        "QSplitter::handle:vertical { height: 6px; }"
        "QStatusBar { background: @PANEL@; border-top: 1px solid @BORDER@; color: @MUTED@; }"
        "QStatusBar::item { border: none; }"
        "QProgressBar { background: @PANELALT@; border: 1px solid @BORDER@; border-radius: 7px;"
        "  text-align: center; color: @TEXT@; height: 18px; }"
        "QProgressBar::chunk { background: @ACCENT@; border-radius: 6px; }"
        "QScrollArea { border: none; background: transparent; }"
        "QGroupBox { border: 1px solid @BORDER@; border-radius: 8px; margin-top: 18px; padding-top: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; color: @MUTED@; }"
        "QToolBar { border: none; background: @PANEL@; }"
        "QMessageBox QLabel { background: transparent; }"
        "QFrame#HLine { background: @BORDER@; max-height: 1px; border: none; }"
        "QFrame#VLine { background: @BORDER@; max-width: 1px; border: none; }");

    QString sheet = S.arg(int(std::lround(12 * scale)))
                         .arg(int(std::lround(10 * scale)))
                         .arg(int(std::lround(13 * scale)));

    auto rep = [&](const char *token, const QColor &c) {
        sheet.replace(QLatin1String(token), c.name(QColor::HexRgb));
    };
    rep("@BG@", bg());
    rep("@PANEL@", panel());
    rep("@PANELALT@", panelAlt());
    rep("@PANELHI@", panelHi());
    rep("@BORDER@", border());
    rep("@TEXT@", text());
    rep("@MUTED@", muted());
    rep("@ACCENT@", accent());
    rep("@ACCENTSOFT@", accentSoft());
    rep("@ON_ACCENT@", QColor("#ffffff"));
    rep("@ACCENTHOVER@", contrast ? QColor("#ff6069") : QColor("#ff5962"));
    return sheet;
}

// ---------------------------------------------------------------- icons

static void drawIcon(const QString &name, QPainter &p, const QRectF &r, const QColor &color)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(color, 2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.translate(r.topLeft());
    const double s = r.width() / 24.0;
    p.scale(s, s);

    if (name == "play") {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF({QPointF(8, 5), QPointF(19, 12), QPointF(8, 19)}));
    } else if (name == "pause") {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(7, 5, 3.6, 14), 1.2, 1.2);
        p.drawRoundedRect(QRectF(13.4, 5, 3.6, 14), 1.2, 1.2);
    } else if (name == "stop") {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(6.5, 6.5, 11, 11), 2, 2);
    } else if (name == "split") {
        QPen dash(color, 2.0);
        dash.setStyle(Qt::DashLine);
        p.setPen(dash);
        p.drawLine(QPointF(12, 3), QPointF(12, 21));
        p.setPen(Qt::NoPen);
        p.setBrush(color);
        p.drawPolygon(QPolygonF({QPointF(9, 8), QPointF(4, 12), QPointF(9, 16)}));
        p.drawPolygon(QPolygonF({QPointF(15, 8), QPointF(20, 12), QPointF(15, 16)}));
    } else if (name == "trash") {
        p.drawLine(QPointF(4, 6.5), QPointF(20, 6.5));
        p.drawLine(QPointF(9.5, 6.5), QPointF(9.5, 4));
        p.drawLine(QPointF(9.5, 4), QPointF(14.5, 4));
        p.drawLine(QPointF(14.5, 4), QPointF(14.5, 6.5));
        p.drawRoundedRect(QRectF(6.2, 6.5, 11.6, 14), 2, 2);
        p.drawLine(QPointF(10, 10), QPointF(10, 17));
        p.drawLine(QPointF(14, 10), QPointF(14, 17));
    } else if (name == "undo" || name == "redo") {
        if (name == "redo") {
            p.translate(24, 0);
            p.scale(-1, 1);
        }
        QPainterPath path;
        path.moveTo(QPointF(6, 9));
        path.cubicTo(QPointF(12, 4), QPointF(20, 6), QPointF(19.5, 13));
        path.cubicTo(QPointF(19.2, 17), QPointF(16, 19), QPointF(12, 19));
        p.drawPath(path);
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawPolygon(QPolygonF({QPointF(6.5, 4), QPointF(6.5, 13), QPointF(2, 8.5)}));
    } else if (name == "plus") {
        p.drawLine(QPointF(12, 5), QPointF(12, 19));
        p.drawLine(QPointF(5, 12), QPointF(19, 12));
    } else if (name == "magnet") {
        p.drawArc(QRectF(5, 4, 14, 14), 180 * 16, 180 * 16);
        p.drawLine(QPointF(5, 11), QPointF(5, 19));
        p.drawLine(QPointF(19, 11), QPointF(19, 19));
        p.setPen(QPen(color, 4.5));
        p.drawLine(QPointF(5, 19), QPointF(5, 20.5));
        p.drawLine(QPointF(19, 19), QPointF(19, 20.5));
    } else if (name == "zoomIn" || name == "zoomOut") {
        p.drawEllipse(QRectF(4, 4, 12.5, 12.5));
        p.drawLine(QPointF(15.5, 15.5), QPointF(20, 20));
        p.drawLine(QPointF(7.5, 10.2), QPointF(13, 10.2));
        if (name == "zoomIn") p.drawLine(QPointF(10.2, 7.5), QPointF(10.2, 13));
    } else if (name == "fit") {
        p.drawPolyline(QPolygonF({QPointF(6, 2), QPointF(6, 18), QPointF(22, 18)}));
        p.drawPolyline(QPolygonF({QPointF(2, 6), QPointF(18, 6), QPointF(18, 22)}));
    } else if (name == "text") {
        p.drawLine(QPointF(4, 6), QPointF(20, 6));
        p.drawLine(QPointF(12, 6), QPointF(12, 20));
        p.drawLine(QPointF(8, 20), QPointF(16, 20));
    } else if (name == "export") {
        p.drawLine(QPointF(12, 15), QPointF(12, 4));
        p.drawLine(QPointF(7.5, 8), QPointF(12, 3.5));
        p.drawLine(QPointF(16.5, 8), QPointF(12, 3.5));
        p.drawPolyline(QPolygonF({QPointF(4, 14), QPointF(4, 20), QPointF(20, 20), QPointF(20, 14)}));
    } else if (name == "folder") {
        QPainterPath path;
        path.moveTo(QPointF(3, 6));
        path.lineTo(QPointF(9.5, 6));
        path.lineTo(QPointF(11.5, 8.5));
        path.lineTo(QPointF(21, 8.5));
        path.lineTo(QPointF(21, 19));
        path.lineTo(QPointF(3, 19));
        path.closeSubpath();
        p.drawPath(path);
    } else if (name == "save") {
        p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
        p.drawRect(QRectF(8, 4, 8, 6));
        p.drawRoundedRect(QRectF(7.5, 13.5, 9, 6.5), 1, 1);
    } else if (name == "eye" || name == "eyeOff") {
        QPainterPath path;
        path.moveTo(QPointF(2.5, 12));
        path.cubicTo(QPointF(7, 4.5), QPointF(17, 4.5), QPointF(21.5, 12));
        path.cubicTo(QPointF(17, 19.5), QPointF(7, 19.5), QPointF(2.5, 12));
        p.drawPath(path);
        p.setBrush(color);
        p.drawEllipse(QRectF(9.5, 9.5, 5, 5));
        if (name == "eyeOff") p.drawLine(QPointF(4, 20), QPointF(20, 4));
    } else if (name == "lock" || name == "unlock") {
        p.drawRoundedRect(QRectF(5.5, 11, 13, 9.5), 2, 2);
        p.drawArc(QRectF(8, 4.5, 8, 10), 0, 180 * 16);
        if (name == "unlock") p.drawLine(QPointF(16, 9.5), QPointF(16, 11));
    } else if (name == "speaker" || name == "speakerOff") {
        p.setBrush(color);
        p.drawPolygon(QPolygonF({QPointF(4, 9.5), QPointF(8, 9.5), QPointF(13, 5),
                                 QPointF(13, 19), QPointF(8, 14.5), QPointF(4, 14.5)}));
        p.setBrush(Qt::NoBrush);
        if (name == "speaker") {
            p.drawArc(QRectF(12, 8, 8, 8), -60 * 16, 120 * 16);
            p.drawArc(QRectF(12, 5.5, 12, 13), -55 * 16, 110 * 16);
        } else {
            p.drawLine(QPointF(16, 9), QPointF(21, 15));
            p.drawLine(QPointF(21, 9), QPointF(16, 15));
        }
    } else if (name == "music") {
        p.setBrush(color);
        p.drawEllipse(QRectF(5, 15.5, 5, 4));
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(10, 17.5), QPointF(10, 5));
        p.drawLine(QPointF(10, 5), QPointF(19, 7));
        p.drawLine(QPointF(19, 7), QPointF(19, 15));
        p.setBrush(color);
        p.drawEllipse(QRectF(14, 13.5, 5, 4));
    } else if (name == "image") {
        p.drawRoundedRect(QRectF(3.5, 5, 17, 14), 2, 2);
        p.setBrush(color);
        p.drawEllipse(QRectF(7, 8, 3.2, 3.2));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(QPolygonF({QPointF(4.5, 17.5), QPointF(10, 12), QPointF(14, 15.5),
                                  QPointF(17, 13), QPointF(20.5, 17.5)}));
    } else if (name == "close") {
        p.drawLine(QPointF(6, 6), QPointF(18, 18));
        p.drawLine(QPointF(18, 6), QPointF(6, 18));
    } else if (name == "layers") {
        p.drawPolygon(QPolygonF({QPointF(12, 3.5), QPointF(21, 8.5), QPointF(12, 13.5), QPointF(3, 8.5)}));
        p.drawPolyline(QPolygonF({QPointF(3, 12.5), QPointF(12, 17.5), QPointF(21, 12.5)}));
        p.drawPolyline(QPolygonF({QPointF(3, 16), QPointF(12, 21), QPointF(21, 16)}));
    } else if (name == "list") {
        p.drawLine(QPointF(4, 7), QPointF(20, 7));
        p.drawLine(QPointF(4, 12), QPointF(20, 12));
        p.drawLine(QPointF(4, 17), QPointF(20, 17));
        p.setBrush(color);
        p.drawEllipse(QRectF(1.5, 6, 2, 2));
        p.drawEllipse(QRectF(1.5, 11, 2, 2));
        p.drawEllipse(QRectF(1.5, 16, 2, 2));
    } else if (name == "settings") {
        p.drawEllipse(QRectF(9, 9, 6, 6));
        for (int i = 0; i < 8; ++i) {
            p.save();
            p.translate(12, 12);
            p.rotate(i * 45);
            p.drawLine(QPointF(0, -8.5), QPointF(0, -6));
            p.restore();
        }
    } else {
        p.drawEllipse(QRectF(5, 5, 14, 14));
    }
}

QIcon icon(const QString &name, const QColor &color, int size)
{
    const int device = 2;
    QPixmap pm(size * device, size * device);
    pm.setDevicePixelRatio(device);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    drawIcon(name, p, QRectF(0, 0, size, size), color);
    p.end();
    return QIcon(pm);
}

QIcon icon(const QString &name, int size)
{
    return icon(name, text(), size);
}

} // namespace Theme
