#include "tpstyle.h"

#include <QFontMetrics>

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

QString TpStyle::glyph(const QFont &font, const char *const *candidates)
{
    const QFontMetrics metrics(font);
    for (int i = 0; candidates && candidates[i]; ++i) {
        const QString candidate = QString::fromUtf8(candidates[i]);
        if (candidate.isEmpty())
            continue;

        // Anything above the BMP arrives as a surrogate pair, and inFont()
        // only takes a QChar, so ask by code point instead.
        const uint ucs4 = (candidate.at(0).isHighSurrogate() && candidate.size() > 1)
            ? QChar::surrogateToUcs4(candidate.at(0), candidate.at(1))
            : candidate.at(0).unicode();

        if (metrics.inFontUcs4(ucs4))
            return candidate;
    }
    return QString();
}
