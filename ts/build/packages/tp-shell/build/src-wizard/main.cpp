// tp-wizard - the first-boot setup wizard.
//
// tp-session runs this once, before anything else starts, when
// root/setup/completed is not set. It is deliberately a plain program with
// no --first-run flag: whether it should appear is a property of the
// machine, recorded in the registry, not of how it was invoked.

#include <QApplication>

#include "registry.h"
#include "tpstyle.h"
#include "wizard.h"

static QString envOr(const char *name, const QString &fallback)
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? fallback : QString::fromLocal8Bit(v);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String("tp-wizard"));
    TpStyle::apply(&app);

    Registry reg(envOr("TP_DEFAULTS", QLatin1String("/etc/tp/registry.defaults")),
                 envOr("TP_REGISTRY", QLatin1String("/var/lib/tp/registry.conf")));
    reg.load();

    Wizard w(&reg);
    w.show();

    return app.exec();
}
