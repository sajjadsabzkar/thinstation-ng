// tp-connmgr - the ThinStation connection manager.
//
// Deliberately Qt Widgets only: QtCore, QtGui and QtWidgets, no QML, no KDE
// Frameworks. ThinPro's own Connection Manager needs the same four libraries,
// but its image also carries the whole KDE stack; this one does not.

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QStringList>

#include "kioskpanel.h"
#include "mainwindow.h"
#include "model.h"
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
    app.setApplicationName(QLatin1String("tp-connmgr"));
    app.setApplicationDisplayName(QObject::tr("Connection Manager"));
    app.setWindowIcon(QIcon::fromTheme(QLatin1String("network-workgroup")));
    TpStyle::apply(&app);

    // Same two paths the tpreg shell tool uses, overridable the same way so
    // the app can be run against a test registry without touching /etc.
    const QString defaults = envOr("TP_DEFAULTS",
                                   QLatin1String("/etc/tp/registry.defaults"));
    const QString userFile = envOr("TP_REGISTRY",
                                   QLatin1String("/var/lib/tp/registry.conf"));

    Registry reg(defaults, userFile);
    if (!reg.load()) {
        qWarning("tp-connmgr: %s", qPrintable(reg.lastError()));
        return 1;
    }

    Model model(&reg);
    model.reload();

    // Normally the registry decides, through the same key tp-session reads:
    // zero configuration is a kiosk. This used to be
    // root/users/$USER/kioskMode, but the session runs as tsuser while the
    // control panel wrote the key for ThinPro's "user", so the two never
    // met and kiosk mode could not be turned on at all. The flags are for
    // the session script, which passes one explicitly, and for testing.
    const QStringList args = app.arguments();
    bool kiosk = reg.value(QLatin1String("root/product/config"))
                 == QLatin1String("zero");
    if (args.contains(QLatin1String("--kiosk")))
        kiosk = true;
    if (args.contains(QLatin1String("--window")))
        kiosk = false;

    // Two presentations of the same model, the way ThinPro's hptc-kiosk
    // carries both MainWindow and KioskWindow.
    if (kiosk) {
        KioskPanel panel(&reg, &model);
        panel.show();
        return app.exec();
    }

    MainWindow w(&reg, &model, false);
    w.show();

    return app.exec();
}
