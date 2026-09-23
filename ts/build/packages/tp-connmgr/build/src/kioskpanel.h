// The kiosk sidebar -- ThinPro's KioskWindow.
//
// In Smart Zero the connection manager is not a window you open, it is a
// panel down the right edge of the screen that is always there. Its binary
// carries both classes, MainWindow and KioskWindow, and the strings that
// belong to this one: "Show all connections", "Settings", "Volume",
// "Information", "Shut Down".
//
// What it has to do:
//
//   * list the connections and start one on a click
//   * let an administrator in, which is the only way to reach the settings
//     from a locked-down client
//   * leave no way out that a user should not have -- no close button, no
//     Alt-F4, nothing behind it but the session
//
// The registry keys are ThinPro's own:
//
//   root/users/<user>/kioskMode         this panel instead of the window
//   root/users/<user>/hideDesktopPanel  hide it until a hot corner is hit
//   root/users/<user>/adminMode         the user has authenticated as admin

#ifndef TP_KIOSKPANEL_H
#define TP_KIOSKPANEL_H

#include <QWidget>

#include "model.h"

class Registry;
class QLabel;
class QListWidget;
class QListWidgetItem;
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
    void refresh();
    void onConnect();
    void onItemActivated(QListWidgetItem *item);
    void onSettings();
    void onAdminMode();
    void onInformation();
    void onShutDown();
    void onVolume();

private:
    void placeOnRightEdge();
    void applyAdminState();
    QString selectedUuid() const;

    Registry    *m_reg;
    Model       *m_model;
    bool         m_admin;

    QLabel      *m_heading;
    QListWidget *m_list;
    QLabel      *m_status;
    QPushButton *m_connectBtn;
    QPushButton *m_adminBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_volumeBtn;
    QPushButton *m_infoBtn;
    QPushButton *m_powerBtn;
};

#endif
