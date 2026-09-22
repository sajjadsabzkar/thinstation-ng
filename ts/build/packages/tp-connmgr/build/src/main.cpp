// tp-connmgr - the ThinStation connection manager.
//
// Deliberately Qt Widgets only: QtCore, QtGui and QtWidgets, no QML, no KDE
// Frameworks. ThinPro's own Connection Manager needs the same four libraries,
// but its image also carries the whole KDE stack; this one does not.

#include <QApplication>
#include <QDir>
#include <QIcon>

#include "mainwindow.h"
#include "model.h"
#include "registry.h"

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

    // Same two paths the tpreg shell tool uses, overridable the same way so
    // the app can be run against a test registry without touching /etc.
    const QString defaults = envOr("TP_DEFAULTS",
                                   QLatin1String("/etc/tp/registry.defaults"));
    const QString userFile = envOr("TP_REGISTRY",
                                   QLatin1String("/etc/tp/registry.conf"));

    Registry reg(defaults, userFile);
    if (!reg.load()) {
        qWarning("tp-connmgr: %s", qPrintable(reg.lastError()));
        return 1;
    }

    Model model(&reg);
    model.reload();

    const QString user = envOr("USER", QLatin1String("user"));
    const bool kiosk = reg.boolValue(
        QLatin1String("root/users/") + user + QLatin1String("/kioskMode"), false);

    MainWindow w(&reg, &model, kiosk);
    w.show();

    return app.exec();
}
