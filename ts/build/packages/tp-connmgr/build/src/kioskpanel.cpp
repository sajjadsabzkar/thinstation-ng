#include "kioskpanel.h"
#include "netwait.h"
#include "registry.h"

#include <QApplication>
#include <QCloseEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#include <unistd.h>

static const int kWidth = 320;

static bool runOk(const QString &command, int timeoutMs = 5000)
{
    QProcess p;
    p.start(QLatin1String("/bin/sh"),
            QStringList() << QLatin1String("-c") << command);
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(500);
        return false;
    }
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

KioskPanel::KioskPanel(Registry *reg, Model *model, QWidget *parent)
    : QWidget(parent), m_reg(reg), m_model(model), m_admin(false)
{
    setWindowTitle(tr("Connection Manager"));
    setObjectName(QLatin1String("tpKioskPanel"));

    // A dock down the right edge: no decorations, above the session, and
    // out of the window list so nothing can raise itself over it.
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool);
    setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // --- heading ---------------------------------------------------------
    QFrame *header = new QFrame(this);
    header->setObjectName(QLatin1String("tpHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(16, 0, 16, 0);

    m_heading = new QLabel(tr("Connections"), header);
    m_heading->setObjectName(QLatin1String("tpTitle"));
    hl->addWidget(m_heading);
    hl->addStretch(1);
    v->addWidget(header);

    // --- connections -----------------------------------------------------
    m_list = new QListWidget(this);
    v->addWidget(m_list, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setContentsMargins(16, 6, 16, 6);
    v->addWidget(m_status);

    // --- actions ---------------------------------------------------------
    QWidget *actions = new QWidget(this);
    QVBoxLayout *al = new QVBoxLayout(actions);
    al->setContentsMargins(12, 8, 12, 12);
    al->setSpacing(6);

    m_connectBtn  = new QPushButton(tr("Connect"), actions);
    m_connectBtn->setProperty("tpPrimary", true);
    m_adminBtn    = new QPushButton(tr("Administrator Mode"), actions);
    m_settingsBtn = new QPushButton(tr("Settings"), actions);
    m_volumeBtn   = new QPushButton(tr("Volume"), actions);
    m_infoBtn     = new QPushButton(tr("Information"), actions);
    m_powerBtn    = new QPushButton(tr("Shut Down"), actions);

    al->addWidget(m_connectBtn);
    al->addWidget(m_adminBtn);
    al->addWidget(m_settingsBtn);
    al->addWidget(m_volumeBtn);
    al->addWidget(m_infoBtn);
    al->addWidget(m_powerBtn);
    v->addWidget(actions);

    connect(m_connectBtn,  SIGNAL(clicked()), this, SLOT(onConnect()));
    connect(m_adminBtn,    SIGNAL(clicked()), this, SLOT(onAdminMode()));
    connect(m_settingsBtn, SIGNAL(clicked()), this, SLOT(onSettings()));
    connect(m_volumeBtn,   SIGNAL(clicked()), this, SLOT(onVolume()));
    connect(m_infoBtn,     SIGNAL(clicked()), this, SLOT(onInformation()));
    connect(m_powerBtn,    SIGNAL(clicked()), this, SLOT(onShutDown()));
    connect(m_list, SIGNAL(itemActivated(QListWidgetItem*)),
            this, SLOT(onItemActivated(QListWidgetItem*)));

    const QString user = QString::fromLocal8Bit(qgetenv("USER"));
    m_admin = (geteuid() == 0)
           || m_reg->boolValue(QLatin1String("root/users/")
                             + (user.isEmpty() ? QLatin1String("user") : user)
                             + QLatin1String("/adminMode"));

    applyAdminState();
    refresh();
    placeOnRightEdge();

    // Connections can be created by the autostart path or by an admin in
    // another window; keep the list honest without anyone asking.
    QTimer *poll = new QTimer(this);
    connect(poll, SIGNAL(timeout()), this, SLOT(refresh()));
    poll->start(10000);
}

void KioskPanel::placeOnRightEdge()
{
    QRect screen;
    if (QApplication::primaryScreen())
        screen = QApplication::primaryScreen()->geometry();
    else
        screen = QRect(0, 0, 1024, 768);

    // Leave room for a taskbar if one is running: it is a dock too, and two
    // docks claiming the same pixels look broken.
    int bottom = screen.bottom() + 1;
    if (m_reg->value(QLatin1String("root/desktop/showTaskbar")) != QLatin1String("0"))
        bottom -= 34;

    setGeometry(screen.right() - kWidth + 1, screen.top(),
                kWidth, bottom - screen.top());
}

void KioskPanel::applyAdminState()
{
    // In user mode the panel is a launcher and nothing else. Settings is
    // still visible, but it asks for the administrator password first --
    // hiding it entirely only makes a locked client look broken.
    m_adminBtn->setText(m_admin ? tr("Leave Administrator Mode")
                                : tr("Administrator Mode"));
    m_heading->setText(m_admin ? tr("Connections - Administrator")
                               : tr("Connections"));
}

void KioskPanel::closeEvent(QCloseEvent *event)
{
    // The whole point of a kiosk is that there is nothing behind this panel.
    // Only the session gets to end it, by killing the process.
    event->ignore();
}

void KioskPanel::refresh()
{
    const QString previous = selectedUuid();

    m_list->clear();
    m_model->reload();

    const QVector<Connection> conns = m_model->connections();
    for (int i = 0; i < conns.size(); ++i) {
        const ConnectionType type = m_model->type(conns.at(i).typeId);

        QListWidgetItem *item = new QListWidgetItem(conns.at(i).label, m_list);
        item->setData(Qt::UserRole, conns.at(i).uuid);
        item->setToolTip(type.label);

        // A connection whose package is missing would hang on pkg's
        // no_package path. Show it, greyed, rather than hiding the fact.
        if (!type.available) {
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
            item->setText(tr("%1  (%2 not installed)")
                              .arg(conns.at(i).label, type.label));
        }

        if (conns.at(i).uuid == previous)
            m_list->setCurrentItem(item);
    }

    if (m_list->count() == 0) {
        m_status->setText(m_admin
            ? tr("No connections. Open Settings to create one.")
            : tr("No connections have been set up on this client."));
    } else {
        if (!m_list->currentItem())
            m_list->setCurrentRow(0);
        m_status->clear();
    }

    m_connectBtn->setEnabled(m_list->count() > 0);
}

QString KioskPanel::selectedUuid() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void KioskPanel::onItemActivated(QListWidgetItem *item)
{
    if (item && (item->flags() & Qt::ItemIsEnabled))
        onConnect();
}

void KioskPanel::onConnect()
{
    const QString uuid = selectedUuid();
    if (uuid.isEmpty())
        return;

    const Connection conn = m_model->connection(uuid);
    const ConnectionType type = m_model->type(conn.typeId);

    if (!type.available) {
        m_status->setText(tr("%1 is not installed in this image.")
                              .arg(type.label));
        return;
    }

    if (m_model->fieldValue(conn, QLatin1String("waitForNetwork"))
            != QLatin1String("0")) {
        if (!NetWait::waitFor(this)) {
            m_status->setText(tr("Cancelled: the network is not ready."));
            return;
        }
    }

    if (!QProcess::startDetached(QLatin1String("tp-launch"),
                                 QStringList() << uuid)) {
        m_status->setText(tr("Could not start the connection."));
        return;
    }
    m_status->setText(tr("Starting %1 ...").arg(conn.label));
}

void KioskPanel::onAdminMode()
{
    const QString user = QString::fromLocal8Bit(qgetenv("USER"));
    const QString key = QLatin1String("root/users/")
                      + (user.isEmpty() ? QLatin1String("user") : user)
                      + QLatin1String("/adminMode");

    if (m_admin) {
        m_admin = false;
        m_reg->setValue(key, QLatin1String("0"));
        m_reg->save();
        applyAdminState();
        m_status->setText(tr("Back in user mode."));
        return;
    }

    bool ok = false;
    const QString password = QInputDialog::getText(this,
        tr("Administrator Mode"),
        tr("Administrator password:"), QLineEdit::Password, QString(), &ok);
    if (!ok)
        return;

    // Checked against the real account, not against anything in the
    // registry: a password stored in a settings file is not a password.
    // su reads the password on stdin only with a tty, so ask sudo instead,
    // which is how the settings panels already escalate.
    QProcess p;
    p.start(QLatin1String("/bin/sh"), QStringList() << QLatin1String("-c")
            << QLatin1String("sudo -k -S -p '' true"));
    if (!p.waitForStarted(3000)) {
        m_status->setText(tr("Could not verify the password."));
        return;
    }
    p.write(password.toLocal8Bit() + "\n");
    p.closeWriteChannel();
    p.waitForFinished(8000);

    if (p.exitCode() != 0) {
        QMessageBox::warning(this, tr("Administrator Mode"),
                             tr("That password was not accepted."));
        return;
    }

    m_admin = true;
    m_reg->setValue(key, QLatin1String("1"));
    m_reg->save();
    applyAdminState();
    refresh();
    m_status->setText(tr("Administrator mode. Settings are unlocked."));
}

void KioskPanel::onSettings()
{
    if (!m_admin) {
        m_status->setText(tr("Enter administrator mode to change settings."));
        onAdminMode();
        if (!m_admin)
            return;
    }
    QProcess::startDetached(QLatin1String("tp-controlpanel"),
                            QStringList() << QLatin1String("--admin"));
}

void KioskPanel::onVolume()
{
    QProcess::startDetached(QLatin1String("tp-panel"),
                            QStringList() << QLatin1String("sound"));
}

void KioskPanel::onInformation()
{
    QProcess::startDetached(QLatin1String("tp-panel"),
                            QStringList() << QLatin1String("sysinfo"));
}

void KioskPanel::onShutDown()
{
    if (QMessageBox::question(this, tr("Shut Down"),
            tr("Shut this client down?")) != QMessageBox::Yes)
        return;

    if (!runOk(QLatin1String("systemctl poweroff")))
        runOk(QLatin1String("poweroff"));
}
