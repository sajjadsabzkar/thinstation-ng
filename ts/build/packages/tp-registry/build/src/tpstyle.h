// Loads the shared ThinPro stylesheet.
//
// Every one of our Qt programs calls this at startup so that the connection
// manager, the control panel, the panels, the taskbar and the setup wizard
// look like one product. The sheet itself is data in the image, at
// /etc/tp/style/thinpro.qss, so the look can be changed without recompiling
// anything.

#ifndef TP_STYLE_H
#define TP_STYLE_H

#include <QString>

class QApplication;
class QFont;

namespace TpStyle {

// Applies the stylesheet and the default palette. $TP_STYLE overrides the
// path, which is how the apps get tested outside an image. A missing sheet
// is not an error: the program still runs, it just looks like plain Qt.
void apply(QApplication *app);

// Returns the first candidate the font can actually draw.
//
// The image ships no icon theme and only a couple of font packages, so the
// interface leans on Unicode symbols. Most of the obvious ones -- the emoji
// pictographs for a key, a lock, a speaker, a paintbrush -- are in neither
// Liberation nor DejaVu, and asking for them gets a row of empty boxes. So
// every place that wants a symbol offers a list, best first, ending in
// something plain that every font has.
//
// candidates is a null-terminated array of UTF-8 strings. If the font can
// draw none of them, the result is empty, and the caller should leave the
// space blank rather than draw a box.
QString glyph(const QFont &font, const char *const *candidates);

}

#endif
