// Loads the shared ThinPro stylesheet.
//
// Every one of our Qt programs calls this at startup so that the connection
// manager, the control panel, the panels, the taskbar and the setup wizard
// look like one product. The sheet itself is data in the image, at
// /etc/tp/style/thinpro.qss, so the look can be changed without recompiling
// anything.

#ifndef TP_STYLE_H
#define TP_STYLE_H

class QApplication;

namespace TpStyle {

// Applies the stylesheet and the default palette. $TP_STYLE overrides the
// path, which is how the apps get tested outside an image. A missing sheet
// is not an error: the program still runs, it just looks like plain Qt.
void apply(QApplication *app);

}

#endif
