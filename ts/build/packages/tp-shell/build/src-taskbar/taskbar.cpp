#include "taskbar.h"
#include "registry.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QTimer>

static const int kHeight = 34;

Taskbar::Taskbar(Registry *reg, QWidget *parent)
    : QWidget(parent), m_reg(reg)
{
    setObjectName(QLatin1String("tpTaskbar"));
    setWindowTitle(QLatin1String("tp-taskbar"));

    // A dock, not an ordinary window: no decorations, always on top, and
    // never in the task switcher.
    setWindowFlags(Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint
                 | Qt::Tool);
    setAttribute(Qt::WA_X11NetWmWindowTypeDock, true);

    QHBoxLayout *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    m_start = new QPushButton(tr("Start"), this);
    m_connections = new QPushButton(tr("Connections"), this);
    m_controlPanel = new QPushButton(tr("Control Panel"), this);

    m_network = new QLabel(this);
    m_clock = new QLabel(this);

    h->addWidget(m_start);
    h->addWidget(m_connections);
    h->addWidget(m_controlPanel);
    h->addStretch(1);
    h->addWidget(m_network);
    h->addWidget(m_clock);

    buildMenu();

    connect(m_start, SIGNAL(clicked()), this, SLOT(onStart()));
    connect(m_connections, SIGNAL(clicked()), this, SLOT(onConnections()));
    connect(m_controlPanel, SIGNAL(clicked()), this, SLOT(onControlPanel()));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTick()));
    timer->start(5000);
    onTick();

    reserveSpace();
}

void Taskbar::reserveSpace()
{
    QRect screen;
    if (QApplication::primaryScreen())
        screen = QApplication::primaryScreen()->geometry();
    else
        screen = QRect(0, 0, 1024, 768);

    setGeometry(screen.x(), screen.bottom() - kHeight + 1,
                screen.width(), kHeight);
    setFixedHeight(kHeight);
}

void Taskbar::buildMenu()
{
    m_menu = new QMenu(this);

    m_menu->addAction(tr("Connection Manager"), this, SLOT(onConnections()));
    m_menu->addAction(tr("Control Panel"), this, SLOT(onControlPanel()));
    m_menu->addSeparator();

    // Power actions go through the ordinary tools rather than through
    // systemd directly, so they work whether or not logind is running.
    QAction *lock = m_menu->addAction(tr("Lock Screen"));
    connect(lock, &QAction::triggered, []() {
        launch(QLatin1String("xdg-screensaver lock || xset s activate"));
    });

    QAction *reboot = m_menu->addAction(tr("Restart"));
    connect(reboot, &QAction::triggered, [this]() {
        if (QMessageBox::question(this, tr("Restart"),
                tr("Restart this client?")) == QMessageBox::Yes)
            launch(QLatin1String("systemctl reboot || reboot"));
    });

    QAction *off = m_menu->addAction(tr("Shut Down"));
    connect(off, &QAction::triggered, [this]() {
        if (QMessageBox::question(this, tr("Shut Down"),
                tr("Shut this client down?")) == QMessageBox::Yes)
            launch(QLatin1String("systemctl poweroff || poweroff"));
    });
}

void Taskbar::launch(const QString &command)
{
    QProcess::startDetached(QLatin1String("/bin/sh"),
                            QStringList() << QLatin1String("-c") << command);
}

void Taskbar::onStart()
{
    m_menu->popup(mapToGlobal(QPoint(m_start->x(),
                                     m_start->y() - m_menu->sizeHint().height())));
}

void Taskbar::onConnections()
{
    launch(QLatin1String("tp-connmgr"));
}

void Taskbar::onControlPanel()
{
    launch(QLatin1String("tp-controlpanel"));
}

void Taskbar::onTick()
{
    const QString format = m_reg
        && m_reg->value(QLatin1String("root/time/clockFormat")) == QLatin1String("12h")
        ? QLatin1String("ddd d MMM  h:mm AP")
        : QLatin1String("ddd d MMM  HH:mm");
    m_clock->setText(QDateTime::currentDateTime().toString(format));

    // A one-word network indicator. ThinPro puts the same thing in its
    // panel, and on a thin client it is the single most useful status
    // there is: without an address, nothing else is going to work.
    QProcess p;
    p.start(QLatin1String("/bin/sh"), QStringList() << QLatin1String("-c")
            << QLatin1String("ip -4 -o addr show scope global 2>/dev/null "
                             "| awk '{print $4}' | head -1"));
    p.waitForFinished(2000);
    const QString addr = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();

    if (addr.isEmpty()) {
        m_network->setObjectName(QLatin1String("tpStatusFail"));
        m_network->setText(tr("No network"));
    } else {
        m_network->setObjectName(QLatin1String("tpStatusOk"));
        m_network->setText(addr);
    }
    m_network->style()->unpolish(m_network);
    m_network->style()->polish(m_network);
}
