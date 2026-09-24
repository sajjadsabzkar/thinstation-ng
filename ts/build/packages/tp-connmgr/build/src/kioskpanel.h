// The kiosk sidebar -- ThinPro's KioskWindow.
//
// Redrawn from screenshots of the real product. The first version of this
// file was a 320px drawer with six buttons down it, which is not what ThinPro
// does at all. In Smart Zero the sidebar is a 60px strip pinned to the right
// edge, and it holds almost nothing:
//
//   * a hamburger at the top, which opens the menu
//   * a column of tray glyphs at the bottom: network, volume, keyboard,
//     display
//   * the time above the date, in small blue text, at the very bottom
//
// Everything else lives in the menu that hamburger opens:
//
//   Create a Connection
//   Edit Connection Settings
//   New <type> Connection
//   ----
//   Switch to User / Switch to Administrator
//   System Information
//   Control Panel
//   Tools        >  X Terminal, Wireless Statistics, Text Editor,
//                   Task Manager, Snipping Tool, Registry Editor,
//                   Initial Setup Wizard, Compatibility Check
//   Power        >  Shut Down, Restart, Log Off
//   [ Search                                    ]
//
// The strip is deliberately thin because in Smart Zero it is the whole
// shell: there is no taskbar and no desktop behind it, and every pixel it
// takes is a pixel the session does not get.
//
// Registry keys, ThinPro's own:
//
//   root/product/config = zero          this panel instead of the window
//   root/users/<user>/hideDesktopPanel  hide it until a hot corner is hit
//   root/users/<user>/adminMode         the user has authenticated as admin

#ifndef TP_KIOSKPANEL_H
#define TP_KIOSKPANEL_H

#include <QWidget>

#include "model.h"

class Registry;
class QLabel;
class QMenu;
class QPushButton;

class KioskPanel : public QWidget
{
    Q_OBJECT

public:
    KioskPanel(Registry *reg, Model *model, QWidget *parent = 0);

protected:
    // A kiosk panel that can be closed is not a kiosk. Refuse it unless the
    // session is shutting us down.
    void closeEvent(QCloseEvent *event);

private slots:
    void onMenuButton();
    void onTick();
    void onConnectUuid();
    void onCreateConnection();
    void onEditConnections();
    void onSwitchMode();
    void onSystemInformation();
    void onControlPanel();
    void onNetwork();
    void onVolume();
    void onKeyboard();
    void onDisplay();

private:
    void placeOnRightEdge();
    void buildMenu();
    void rebuildConnectionActions();
    bool requireAdmin();
    static void launch(const QString &command);

    Registry    *m_reg;
    Model       *m_model;
    bool         m_admin;

    QPushButton *m_menuButton;
    QMenu       *m_menu;
    QMenu       *m_toolsMenu;
    QMenu       *m_powerMenu;
    QAction     *m_modeAction;
    QAction     *m_connectionsSeparator;
    QLabel      *m_clock;
};

#endif
