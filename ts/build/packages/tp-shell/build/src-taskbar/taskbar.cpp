#include "taskbar.h"
#include "registry.h"
#include "tpstyle.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTimer>

// ThinPro's panel is about this tall at 1080p; on the 1024x768 screens this
// image targets it still leaves the session nearly all of the height.
static const int kHeight = 46;

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
    // The stylesheet paints the strip; a plain QWidget ignores background
    // rules without this.
    setAttribute(Qt::WA_StyledBackground, true);

    QHBoxLayout *h = new QHBoxLayout(this);
    h->setContentsMargins(0, 0, 8, 0);
    h->setSpacing(2);

    static const char *menuGlyphs[] = { "\xe2\x98\xb0", "\xe2\x89\xa1", "=", 0 };
    m_start = new QPushButton(TpStyle::glyph(font(), menuGlyphs), this);
    m_start->setObjectName(QLatin1String("tpTaskbarMenu"));
    m_start->setToolTip(tr("Menu"));
    h->addWidget(m_start);
    h->addStretch(1);

    // The same glyphs, with the same fallbacks, as the kiosk sidebar.
    static const char *networkGlyphs[]  = { "\xe2\x87\xb5", "\xe2\x87\x84", "N", 0 };
    static const char *keyboardGlyphs[] = { "\xe2\x8c\xa8", "\xe2\x8c\xa7", "K", 0 };
    static const char *volumeGlyphs[]   = { "\xf0\x9f\x94\x8a", "\xe2\x99\xaa", "V", 0 };
    static const char *displayGlyphs[]  = { "\xe2\x96\xa3", "\xe2\x96\xa1", "D", 0 };

    m_network  = trayButton(networkGlyphs,  tr("Network"),  QLatin1String("tp-panel network"));
    m_keyboard = trayButton(keyboardGlyphs, tr("Keyboard"), QLatin1String("tp-panel keyboard"));
    h->addWidget(m_network);
    h->addWidget(m_keyboard);
    h->addWidget(trayButton(volumeGlyphs,  tr("Volume"),  QLatin1String("tp-panel sound")));
    h->addWidget(trayButton(displayGlyphs, tr("Display"), QLatin1String("tp-panel display")));

    // Time over date, as ThinPro stacks them.
    m_clock = new QLabel(this);
    m_clock->setObjectName(QLatin1String("tpTaskbarClock"));
    h->addWidget(m_clock);

    buildMenu();

    connect(m_start, SIGNAL(clicked()), this, SLOT(onStart()));

    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(onTick()));
    timer->start(5000);
    onTick();

    reserveSpace();
}

QPushButton *Taskbar::trayButton(const char *const *glyphs, const QString &tip,
                                 const QString &command)
{
    QPushButton *b = new QPushButton(TpStyle::glyph(font(), glyphs), this);
    b->setObjectName(QLatin1String("tpTaskbarTray"));
    b->setToolTip(tip);
    connect(b, &QPushButton::clicked, [command]() { launch(command); });
    return b;
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
    // The kiosk sidebar's menu styling: rows sized for a finger.
    m_menu = new QMenu(this);
    m_menu->setObjectName(QLatin1String("tpKioskMenu"));

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
    const QDateTime now = QDateTime::currentDateTime();
    const bool twelve = m_reg
        && m_reg->value(QLatin1String("root/time/clockFormat")) == QLatin1String("12h");
    // The locale's short date, but with the whole year: ThinPro shows
    // 9/24/2026, and en_US's short form would say 9/24/26.
    QString dateFormat = QLocale().dateFormat(QLocale::ShortFormat);
    if (!dateFormat.contains(QLatin1String("yyyy")))
        dateFormat.replace(QLatin1String("yy"), QLatin1String("yyyy"));
    m_clock->setText(now.toString(twelve ? QLatin1String("h:mm AP") : QLatin1String("HH:mm"))
                     + QLatin1Char('\n') + now.date().toString(dateFormat));

    // The network glyph carries the address in its tooltip and turns red
    // without one. On a thin client that is the single most useful status
    // there is: without an address, nothing else is going to work.
    QProcess p;
    p.start(QLatin1String("/bin/sh"), QStringList() << QLatin1String("-c")
            << QLatin1String("ip -4 -o addr show scope global 2>/dev/null "
                             "| awk '{print $4}' | head -1"));
    p.waitForFinished(2000);
    const QString addr = QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();

    m_network->setProperty("tpOffline", addr.isEmpty());
    m_network->setToolTip(addr.isEmpty() ? tr("No network") : tr("Network: %1").arg(addr));
    m_network->style()->unpolish(m_network);
    m_network->style()->polish(m_network);

    const QString layout = m_reg ? m_reg->value(QLatin1String("root/keyboard/layout")) : QString();
    m_keyboard->setToolTip(layout.isEmpty() ? tr("Keyboard") : tr("Keyboard: %1").arg(layout));
}
