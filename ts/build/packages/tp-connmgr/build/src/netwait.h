// "Waiting for networking..." -- the modal ThinPro shows before a session.
//
// ThinPro's connection-mgr watches the Manticore key tmp/network/status and
// dismisses this dialog when it flips; the string in that binary is
// literally "Waiting for networking...". We have no daemon to flip a key, so
// the readiness test is done here, directly against the kernel: an interface
// that is up, carrying, and holding a global IPv4 address.
//
// Cancel exists on purpose. A client on a bench with no cable still has to
// be reachable by the person configuring it.

#ifndef TP_NETWAIT_H
#define TP_NETWAIT_H

#include <QDialog>

class QLabel;
class QProgressBar;
class QTimer;

class NetWait : public QDialog
{
    Q_OBJECT

public:
    explicit NetWait(QWidget *parent = 0);

    // True when an interface is up with a global IPv4 address. Cheap enough
    // to call on a timer: it reads /sys and one ip invocation.
    static bool networkReady(QString *detail = 0);

    // Shows the dialog only if the network is not ready already, and returns
    // true when it became ready. The common case -- a client with a cable --
    // never sees a dialog at all.
    static bool waitFor(QWidget *parent, int timeoutSeconds = 0);

private slots:
    void poll();

private:
    QLabel       *m_message;
    QLabel       *m_detail;
    QProgressBar *m_bar;
    QTimer       *m_timer;
    int           m_elapsed;
    int           m_timeout;
};

#endif
