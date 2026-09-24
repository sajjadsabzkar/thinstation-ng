// tp-desktop - the wallpaper and the connection launchers of the standard
// configuration. See desktop.h.

#include <QApplication>

#include "desktop.h"
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
    app.setApplicationName(QLatin1String("tp-desktop"));
    TpStyle::apply(&app);

    Registry reg(envOr("TP_DEFAULTS", QLatin1String("/etc/tp/registry.defaults")),
                 envOr("TP_REGISTRY", QLatin1String("/var/lib/tp/registry.conf")));
    reg.load();

    Desktop desktop(&reg);
    desktop.show();

    return app.exec();
}
