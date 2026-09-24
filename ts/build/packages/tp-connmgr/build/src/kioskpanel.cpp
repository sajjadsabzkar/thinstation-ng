#include "kioskpanel.h"
#include "netwait.h"
#include "registry.h"
#include "tpstyle.h"

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <unistd.h>

// 60px is what ThinPro gives the strip. Wide enough for a glyph and a
// hh:mm above a date, narrow enough that nobody resents it.
static const int kWidth = 60;

KioskPanel::KioskPanel(Registry *reg, Model *model, QWidget *parent)
    : QWidget(parent), m_reg(reg), m_model(model), m_admin(false),
      m_menu(0), m_toolsMenu(0), m_powerMenu(0), m_modeAction(0),
      m_connectionsSeparator(0)
{
    setWindowTitle(tr("Connection Manager"));
    setObjectName(QLatin1String("tpKioskPanel"));

    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool);
    setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    static const char *menuGlyphs[] = { "\xe2\x98\xb0", "\xe2\x89\xa1", "=", 0 };
    m_menuButton = new QPushButton(TpStyle::glyph(font(), menuGlyphs), this);
    m_menuButton->setObjectName(QLatin1String("tpKioskMenuButton"));
    m_menuButton->setToolTip(tr("Menu"));
    v->addWidget(m_menuButton);

    // Everything between the hamburger and the tray is empty space in
    // ThinPro. Leaving it empty is the design, not an oversight.
    v->addStretch(1);

    // The tray glyphs, each with fallbacks: the speaker pictograph is in
    // neither font this image ships, and an empty box in a system tray is
    // worse than a plainer symbol.
    static const char *networkGlyphs[]  = { "\xe2\x87\xb5", "\xe2\x87\x84", "N", 0 };
    static const char *volumeGlyphs[]   = { "\xf0\x9f\x94\x8a", "\xe2\x99\xaa", "V", 0 };
    static const char *keyboardGlyphs[] = { "\xe2\x8c\xa8", "\xe2\x8c\xa7", "K", 0 };
    static const char *displayGlyphs[]  = { "\xe2\x96\xa3", "\xe2\x96\xa1", "D", 0 };

    struct { const char *const *glyphs; const char *tip; const char *slot; } tray[] = {
        { networkGlyphs,  "Network",  SLOT(onNetwork())  },
        { volumeGlyphs,   "Volume",   SLOT(onVolume())   },
        { keyboardGlyphs, "Keyboard", SLOT(onKeyboard()) },
        { displayGlyphs,  "Display",  SLOT(onDisplay())  }
    };
    for (int i = 0; i < 4; ++i) {
        QPushButton *b = new QPushButton(TpStyle::glyph(font(), tray[i].glyphs), this);
        b->setObjectName(QLatin1String("tpTrayButton"));
        b->setToolTip(tr(tray[i].tip));
        connect(b, SIGNAL(clicked()), this, tray[i].slot);
        v->addWidget(b);
    }

    m_clock = new QLabel(this);
    m_clock->setObjectName(QLatin1String("tpKioskClock"));
    v->addWidget(m_clock);

    const QString user = QString::fromLocal8Bit(qgetenv("USER"));
    m_admin = (geteuid() == 0)
           || m_reg->boolValue(QLatin1String("root/users/")
                             + (user.isEmpty() ? QLatin1String("user") : user)
                             + QLatin1String("/adminMode"));

    buildMenu();
    connect(m_menuButton, SIGNAL(clicked()), this, SLOT(onMenuButton()));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTick()));
    timer->start(20000);
    onTick();

    placeOnRightEdge();
}

void KioskPanel::placeOnRightEdge()
{
    QRect screen;
    if (QApplication::primaryScreen())
        screen = QApplication::primaryScreen()->geometry();
    else
        screen = QRect(0, 0, 1024, 768);

    setGeometry(screen.right() - kWidth + 1, screen.top(),
                kWidth, screen.height());
    setFixedWidth(kWidth);
}

void KioskPanel::buildMenu()
{
    m_menu = new QMenu(this);
    m_menu->setObjectName(QLatin1String("tpKioskMenu"));

    m_menu->addAction(tr("Create a Connection"), this, SLOT(onCreateConnection()));
    m_menu->addAction(tr("Edit Connection Settings"), this, SLOT(onEditConnections()));

    // The connections themselves go between here and the separator, and are
    // rebuilt every time the menu opens.
    m_connectionsSeparator = m_menu->addSeparator();

    m_modeAction = m_menu->addAction(QString(), this, SLOT(onSwitchMode()));
    m_menu->addAction(tr("System Information"), this, SLOT(onSystemInformation()));
    m_menu->addAction(tr("Control Panel"), this, SLOT(onControlPanel()));

    m_toolsMenu = m_menu->addMenu(tr("Tools"));
    m_toolsMenu->setObjectName(QLatin1String("tpKioskMenu"));

    // The same eight entries ThinPro puts under Tools, in its order.
    struct { const char *label; const char *command; bool adminOnly; } tools[] = {
        { "X Terminal",           "tp-xterm",               true  },
        { "Wireless Statistics",  "tp-panel wlsstat",       false },
        { "Text Editor",          "tp-texteditor",          true  },
        { "Task Manager",         "tp-panel taskmgr",       false },
        { "Snipping Tool",        "tp-snip",                false },
        { "Registry Editor",      "tp-panel regedit",       true  },
        { "Initial Setup Wizard", "tp-wizard --rerun",      true  },
        { "Compatibility Check",  "tp-panel compat",        false }
    };
    for (int i = 0; i < 8; ++i) {
        QAction *a = m_toolsMenu->addAction(tr(tools[i].label));
        const QString command = QLatin1String(tools[i].command);
        const bool adminOnly = tools[i].adminOnly;
        connect(a, &QAction::triggered, this, [this, command, adminOnly]() {
            if (adminOnly && !requireAdmin())
                return;
            launch(command);
        });
    }

    m_powerMenu = m_menu->addMenu(tr("Power"));
    m_powerMenu->setObjectName(QLatin1String("tpKioskMenu"));

    QAction *shutdown = m_powerMenu->addAction(tr("Shut Down"));
    connect(shutdown, &QAction::triggered, [this]() {
        if (QMessageBox::question(this, tr("Shut Down"),
                tr("Shut this client down?")) == QMessageBox::Yes)
            launch(QLatin1String("systemctl poweroff || poweroff"));
    });

    QAction *restart = m_powerMenu->addAction(tr("Restart"));
    connect(restart, &QAction::triggered, [this]() {
        if (QMessageBox::question(this, tr("Restart"),
                tr("Restart this client?")) == QMessageBox::Yes)
            launch(QLatin1String("systemctl reboot || reboot"));
    });

    // The search box is a widget pinned to the bottom of the menu, the way
    // ThinPro does it. Typing filters the actions above it.
    QLineEdit *search = new QLineEdit(m_menu);
    search->setObjectName(QLatin1String("tpMenuSearch"));
    search->setPlaceholderText(tr("Search"));
    QWidgetAction *searchAction = new QWidgetAction(m_menu);
    searchAction->setDefaultWidget(search);
    m_menu->addAction(searchAction);

    connect(search, &QLineEdit::textChanged, this, [this](const QString &text) {
        const QList<QAction *> actions = m_menu->actions();
        for (int i = 0; i < actions.size(); ++i) {
            QAction *a = actions.at(i);
            if (a->isSeparator() || qobject_cast<QWidgetAction *>(a))
                continue;
            a->setVisible(text.isEmpty()
                          || a->text().contains(text, Qt::CaseInsensitive));
        }
    });

    m_modeAction->setText(m_admin ? tr("Switch to User")
                                  : tr("Switch to Administrator"));
}

void KioskPanel::rebuildConnectionActions()
{
    // Drop what was there last time: connections can appear and disappear
    // while the panel is up. A connection action is the only kind carrying a
    // uuid in data(), which is a surer test than counting positions.
    const QList<QAction *> existing = m_menu->actions();
    for (int i = 0; i < existing.size(); ++i) {
        QAction *a = existing.at(i);
        if (a == m_connectionsSeparator)
            break;
        if (a->data().toString().isEmpty())
            continue;
        m_menu->removeAction(a);
        delete a;
    }

    m_model->reload();
    const QVector<Connection> conns = m_model->connections();
    for (int i = 0; i < conns.size(); ++i) {
        const ConnectionType type = m_model->type(conns.at(i).typeId);

        QAction *a = new QAction(conns.at(i).label, m_menu);
        a->setData(conns.at(i).uuid);
        if (!type.available) {
            // Shown, but not startable: a connection whose package is
            // missing would hit pkg's no_package path and sleep forever.
            a->setEnabled(false);
            a->setText(tr("%1  (%2 not installed)")
                           .arg(conns.at(i).label, type.label));
        }
        connect(a, SIGNAL(triggered()), this, SLOT(onConnectUuid()));
        m_menu->insertAction(m_connectionsSeparator, a);
    }
}

void KioskPanel::onMenuButton()
{
    rebuildConnectionActions();
    m_modeAction->setText(m_admin ? tr("Switch to User")
                                  : tr("Switch to Administrator"));

    // Open to the left of the strip, aligned with the button.
    const QSize hint = m_menu->sizeHint();
    QPoint origin = m_menuButton->mapToGlobal(QPoint(0, 0));
    m_menu->popup(QPoint(origin.x() - hint.width(), origin.y()));
}

void KioskPanel::onTick()
{
    const bool ampm = m_reg->value(QLatin1String("root/time/clockFormat"))
                      != QLatin1String("24h");
    const QDateTime now = QDateTime::currentDateTime();
    m_clock->setText(now.toString(ampm ? QLatin1String("h:mm AP")
                                       : QLatin1String("HH:mm"))
                     + QLatin1Char('\n')
                     + now.toString(QLatin1String("M/d/yyyy")));
}

void KioskPanel::onConnectUuid()
{
    QAction *a = qobject_cast<QAction *>(sender());
    if (!a)
        return;
    const QString uuid = a->data().toString();
    if (uuid.isEmpty())
        return;

    const Connection conn = m_model->connection(uuid);

    if (m_model->fieldValue(conn, QLatin1String("waitForNetwork"))
            != QLatin1String("0")) {
        if (!NetWait::waitFor(this))
            return;
    }

    QProcess::startDetached(QLatin1String("tp-launch"), QStringList() << uuid);
}

bool KioskPanel::requireAdmin()
{
    if (m_admin)
        return true;
    onSwitchMode();
    return m_admin;
}

void KioskPanel::onSwitchMode()
{
    const QString user = QString::fromLocal8Bit(qgetenv("USER"));
    const QString key = QLatin1String("root/users/")
                      + (user.isEmpty() ? QLatin1String("user") : user)
                      + QLatin1String("/adminMode");

    if (m_admin) {
        m_admin = false;
        m_reg->setValue(key, QLatin1String("0"));
        m_reg->save();
        m_modeAction->setText(tr("Switch to Administrator"));
        return;
    }

    bool ok = false;
    const QString password = QInputDialog::getText(this,
        tr("Switch to Administrator"),
        tr("Administrator password:"), QLineEdit::Password, QString(), &ok);
    if (!ok)
        return;

    // Checked against the real account, not against anything in the
    // registry: a password stored in a settings file is not a password.
    // tp-switch-admin does the checking for the control panel too, so the
    // two cannot disagree; "check" leaves the registry to us.
    QProcess p;
    p.start(QLatin1String("tp-switch-admin"), QStringList() << QLatin1String("check"));
    if (!p.waitForStarted(3000)) {
        QMessageBox::warning(this, tr("Switch to Administrator"),
                             tr("Could not verify the password."));
        return;
    }
    p.write(password.toLocal8Bit() + "\n");
    p.closeWriteChannel();
    // su waits a few seconds before it says no.
    p.waitForFinished(15000);

    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QMessageBox::warning(this, tr("Switch to Administrator"),
                             tr("That password was not accepted."));
        return;
    }

    m_admin = true;
    m_reg->setValue(key, QLatin1String("1"));
    m_reg->save();
    m_modeAction->setText(tr("Switch to User"));
}

void KioskPanel::onCreateConnection()
{
    if (!requireAdmin())
        return;
    launch(QLatin1String("tp-connmgr --new"));
}

void KioskPanel::onEditConnections()
{
    if (!requireAdmin())
        return;
    launch(QLatin1String("tp-connmgr"));
}

void KioskPanel::onSystemInformation() { launch(QLatin1String("tp-panel sysinfo")); }
void KioskPanel::onControlPanel()
{
    if (!requireAdmin())
        return;
    launch(QLatin1String("tp-controlpanel"));
}

void KioskPanel::onNetwork()  { launch(QLatin1String("tp-panel network")); }
void KioskPanel::onVolume()   { launch(QLatin1String("tp-panel sound")); }
void KioskPanel::onKeyboard() { launch(QLatin1String("tp-panel keyboard")); }
void KioskPanel::onDisplay()  { launch(QLatin1String("tp-panel display")); }

void KioskPanel::launch(const QString &command)
{
    QProcess::startDetached(QLatin1String("/bin/sh"),
                            QStringList() << QLatin1String("-c") << command);
}

void KioskPanel::closeEvent(QCloseEvent *event)
{
    // The whole point of a kiosk is that there is nothing behind this panel.
    // Only the session gets to end it, by killing the process.
    event->ignore();
}
