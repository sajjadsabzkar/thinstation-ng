// tp-controlpanel - the ThinPro-style settings shell.
//
// Admin mode is what decides how much is on show. ThinPro keeps the full set
// in /etc/hptc-control-panel/applications and a smaller user-visible set in
// users/<name>/applications; we do the same under /etc/tp/control-panel.
// Running as root, or with root/users/<user>/adminMode set, shows everything.

#include <QApplication>
#include <QIcon>
#include <QString>

#include <unistd.h>

#include "cpwindow.h"
#include "panelentry.h"
#include "registry.h"
#include "tpstyle.h"

static QString envOr(const char *name, const QString &fallback)
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? fallback : QString::fromLocal8Bit(v);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String("tp-controlpanel"));
    app.setWindowIcon(QIcon::fromTheme(QLatin1String("preferences-system")));
    TpStyle::apply(&app);

    const QString appsDir = envOr("TP_CP_APPS",
                                  QLatin1String("/etc/tp/control-panel/applications"));
    const QString usersDir = envOr("TP_CP_USERS",
                                   QLatin1String("/etc/tp/control-panel/users"));

    Registry reg(envOr("TP_DEFAULTS", QLatin1String("/etc/tp/registry.defaults")),
                 envOr("TP_REGISTRY", QLatin1String("/var/lib/tp/registry.conf")));
    reg.load();

    const QString user = envOr("USER", QLatin1String("user"));

    bool admin = (geteuid() == 0);
    if (!admin)
        admin = reg.boolValue(QLatin1String("root/users/") + user
                              + QLatin1String("/adminMode"));

    // --admin and --user override the detection, so the same binary can be
    // wired to two .desktop entries.
    const QStringList args = app.arguments();
    if (args.contains(QLatin1String("--admin")))
        admin = true;
    if (args.contains(QLatin1String("--user")))
        admin = false;

    PanelIndex index;
    index.scan(appsDir, admin ? QString()
                              : usersDir + QLatin1Char('/') + user
                                + QLatin1String("/applications"));

    CpWindow w(index, admin);
    w.show();

    return app.exec();
}
