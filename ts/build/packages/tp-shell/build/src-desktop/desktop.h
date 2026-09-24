// The desktop behind everything in the standard configuration: the
// wallpaper and one icon per connection, the way ThinPro's
// `hptc-file-mgr --desktop` draws them.
//
// ThinPro's 25desktop hook sets the background with hptc-set-background and
// then starts its file manager in desktop mode, which shows a launcher for
// every connection down the left edge. A double click starts the
// connection. That is all this does, too: no files, no drag and drop, no
// right-click menu -- the connection manager is where connections are
// edited, and a thin-client desktop is not a file browser.
//
// It is one full-screen window typed _NET_WM_WINDOW_TYPE_DESKTOP, so the
// window manager keeps it below everything and out of the task switcher.
// Painting the wallpaper here rather than on the root window is also what
// lets the Background Manager show images: the image carries xsetroot,
// which only does colours.
//
// Smart Zero has no desktop; tp-session starts this in standard mode only.

#ifndef TP_DESKTOP_H
#define TP_DESKTOP_H

#include <QPixmap>
#include <QVector>
#include <QWidget>

class Registry;
class QFileSystemWatcher;

struct DesktopIcon {
    QString uuid;
    QString type;
    QString label;
    QRect   rect;       // where it is drawn, in widget coordinates
};

class Desktop : public QWidget
{
    Q_OBJECT

public:
    explicit Desktop(Registry *reg, QWidget *parent = 0);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void reload();
    void fitToScreen();

private:
    void loadWallpaper();
    void loadConnections();
    void layoutIcons();
    void launch(int index);
    int  iconAt(const QPoint &pos) const;

    static void drawTypeIcon(QPainter &p, const QRect &r, const QString &type);

    Registry            *m_reg;
    QFileSystemWatcher  *m_watcher;
    QPixmap              m_wallpaper;   // already scaled to the widget
    QString              m_wallpaperKey;
    QVector<DesktopIcon> m_icons;
    QString              m_connectionsKey;
    int                  m_selected;
};

#endif
