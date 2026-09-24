#include "tpstyle.h"

#include <QGlyphRun>
#include <QTextLayout>

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

bool TpStyle::drawable(const QString &text, const QFont &font)
{
    if (text.trimmed().isEmpty())
        return true;

    QTextLayout layout(text, font);
    layout.beginLayout();
    layout.createLine();
    layout.endLayout();

    const QList<QGlyphRun> runs = layout.glyphRuns();
    if (runs.isEmpty())
        return false;
    for (int r = 0; r < runs.size(); ++r) {
        const QVector<quint32> glyphs = runs.at(r).glyphIndexes();
        for (int g = 0; g < glyphs.size(); ++g)
            if (glyphs.at(g) == 0)
                return false;
    }
    return true;
}

QString TpStyle::glyph(const QFont &font, const char *const *candidates)
{
    for (int i = 0; candidates && candidates[i]; ++i) {
        const QString candidate = QString::fromUtf8(candidates[i]);
        if (!candidate.isEmpty() && drawable(candidate, font))
            return candidate;
    }
    return QString();
}
