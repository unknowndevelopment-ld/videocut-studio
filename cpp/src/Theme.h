#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

class QApplication;

namespace Theme {

extern double scale;
extern bool contrast;

void apply(QApplication *app, double scaleFactor, bool highContrast);
int px(double v);

QColor bg();
QColor panel();
QColor panelAlt();
QColor panelHi();
QColor border();
QColor text();
QColor muted();
QColor accent();
QColor accentSoft();
QColor danger();
QColor warn();
QColor good();
QColor playhead();

QString styleSheet();

QIcon icon(const QString &name, const QColor &color, int size = 18);
QIcon icon(const QString &name, int size = 18);

QString shortcutHint(const QString &key);

} // namespace Theme
