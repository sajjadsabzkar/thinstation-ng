// tp-taskbar - the panel that keeps the desktop from being a dead end.

#include <QApplication>

#include "registry.h"
#include "taskbar.h"
#include "tpstyle.h"

static QString envOr(const char *name, const QString &fallback)
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? fallback : QString::fromLocal8Bit(v);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String("tp-taskbar"));
    app.setQuitOnLastWindowClosed(false);
    TpStyle::apply(&app);

    Registry reg(envOr("TP_DEFAULTS", QLatin1String("/etc/tp/registry.defaults")),
                 envOr("TP_REGISTRY", QLatin1String("/var/lib/tp/registry.conf")));
    reg.load();

    Taskbar bar(&reg);
    bar.show();

    return app.exec();
}
