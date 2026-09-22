// tp-panel <panel-id> - renders one settings panel out of the registry.
//
// Every panel in the control panel is this same binary with a different
// argument. What a panel contains, and what applying it does, is registry
// data plus a shell hook, so adding a panel means adding a .desktop file,
// a block of defaults and an .apply script -- no C++.

#include <QApplication>
#include <QMessageBox>

#include <stdio.h>

#include "panelform.h"
#include "registry.h"

static QString envOr(const char *name, const QString &fallback)
{
    const QByteArray v = qgetenv(name);
    return v.isEmpty() ? fallback : QString::fromLocal8Bit(v);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QLatin1String("tp-panel"));

    const QStringList args = app.arguments();
    if (args.size() < 2) {
        fprintf(stderr, "usage: tp-panel <panel-id>\n");
        return 2;
    }

    Registry reg(envOr("TP_DEFAULTS", QLatin1String("/etc/tp/registry.defaults")),
                 envOr("TP_REGISTRY", QLatin1String("/etc/tp/registry.conf")));
    if (!reg.load()) {
        QMessageBox::critical(0, QLatin1String("tp-panel"),
                              QObject::tr("Could not read the registry: %1")
                                  .arg(reg.lastError()));
        return 1;
    }

    PanelForm form(&reg, args.at(1));
    if (!form.isValid()) {
        QMessageBox::critical(0, QLatin1String("tp-panel"), form.errorString());
        return 1;
    }

    form.show();
    return app.exec();
}
