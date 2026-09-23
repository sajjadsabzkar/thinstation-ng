#include "tpstyle.h"

#include <QApplication>
#include <QByteArray>
#include <QFile>
#include <QFont>
#include <QStyleFactory>

namespace TpStyle {

void apply(QApplication *app)
{
    if (!app)
        return;

    // Fusion draws the same on any machine. Without it the sheet lands on
    // whatever style the image happens to carry, and the metrics shift.
    QStyle *fusion = QStyleFactory::create(QLatin1String("Fusion"));
    if (fusion)
        app->setStyle(fusion);

    QFont f = app->font();
    f.setPointSize(10);
    app->setFont(f);

    const QByteArray env = qgetenv("TP_STYLE");
    const QString path = env.isEmpty()
        ? QLatin1String("/etc/tp/style/thinpro.qss")
        : QString::fromLocal8Bit(env);

    QFile sheet(path);
    if (sheet.open(QIODevice::ReadOnly | QIODevice::Text))
        app->setStyleSheet(QString::fromUtf8(sheet.readAll()));
}

}
