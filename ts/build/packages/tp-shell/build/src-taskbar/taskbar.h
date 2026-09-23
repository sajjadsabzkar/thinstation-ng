// The taskbar, ThinPro's lxqt-panel in miniature.
//
// This exists because of a bug the first bootable image had: closing the
// connection manager left a bare root window with no way to get anything
// back. ThinPro never has that problem because its standard configuration
// always has a panel with a start menu. So does this now.
//
// Deliberately not a window list: on a thin client there is usually one
// full-screen session and nothing to switch between. What it carries is the
// start menu, the connection manager, the clock, and the way out.

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
    static void launch(const QString &command);

    Registry    *m_reg;
    QPushButton *m_start;
    QPushButton *m_connections;
    QPushButton *m_controlPanel;
    QLabel      *m_clock;
    QLabel      *m_network;
    QMenu       *m_menu;
};

#endif
