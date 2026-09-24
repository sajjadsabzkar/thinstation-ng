// The taskbar, ThinPro's lxqt-panel in miniature.
//
// This exists because of a bug the first bootable image had: closing the
// connection manager left a bare root window with no way to get anything
// back. ThinPro never has that problem because its standard configuration
// always has a panel with a menu. So does this now.
//
// Drawn the way ThinPro 8.1's panel looks: a light strip along the bottom,
// the hamburger at the left, and at the right the tray -- network,
// keyboard, volume, display -- then the time over the date. Deliberately
// not a window list: on a thin client there is usually one full-screen
// session and nothing to switch between.

#ifndef TP_TASKBAR_H
#define TP_TASKBAR_H

#include <QWidget>

class Registry;
class QLabel;
class QMenu;
class QPushButton;

class Taskbar : public QWidget
{
    Q_OBJECT

public:
    explicit Taskbar(Registry *reg, QWidget *parent = 0);

private slots:
    void onTick();
    void onStart();
    void onConnections();
    void onControlPanel();

private:
    void reserveSpace();
    void buildMenu();
    QPushButton *trayButton(const char *const *glyphs, const QString &tip,
                            const QString &command);
    static void launch(const QString &command);

    Registry    *m_reg;
    QPushButton *m_start;
    QPushButton *m_network;
    QPushButton *m_keyboard;
    QLabel      *m_clock;
    QMenu       *m_menu;
};

#endif
