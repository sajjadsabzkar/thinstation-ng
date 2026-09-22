#include "panelentry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

bool PanelEntry::matches(const QString &needle) const
{
    if (needle.isEmpty())
        return true;
    if (name.contains(needle, Qt::CaseInsensitive))
        return true;
    if (comment.contains(needle, Qt::CaseInsensitive))
        return true;
    for (int i = 0; i < keywords.size(); ++i)
        if (keywords.at(i).contains(needle, Qt::CaseInsensitive))
            return true;
    return false;
}

PanelIndex::PanelIndex()
{
}

// A .desktop file is an ini file, but QSettings mangles keys with brackets
// (Name[de]) and would silently drop our X- keys on some builds, so parse it
// by hand. We only care about the [Desktop Entry] group and we ignore the
// localised Name[xx] variants: the image ships one language.
bool PanelIndex::parse(const QString &path, PanelEntry *out)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);
    bool inEntry = false;
    QString categories;

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;

        if (line.startsWith(QLatin1Char('['))) {
            inEntry = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        if (!inEntry)
            continue;

        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        // Skip localised variants such as Name[de].
        if (key.contains(QLatin1Char('[')))
            continue;

        if (key == QLatin1String("Name"))              out->name = value;
        else if (key == QLatin1String("Comment"))      out->comment = value;
        else if (key == QLatin1String("Icon"))         out->icon = value;
        else if (key == QLatin1String("Exec"))         out->exec = value;
        else if (key == QLatin1String("Categories"))   categories = value;
        else if (key == QLatin1String("X-TP-Category")) out->category = value;
        else if (key == QLatin1String("X-TP-ExecAsRoot")) out->execAsRoot = value;
        else if (key == QLatin1String("NoDisplay"))
            out->noDisplay = (value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0);
        else if (key == QLatin1String("Keywords"))
            out->keywords = value.split(QLatin1Char(';'), Qt::SkipEmptyParts);
    }

    if (!categories.split(QLatin1Char(';'), Qt::SkipEmptyParts)
             .contains(QLatin1String("config-panel")))
        return false;

    out->id = QFileInfo(path).completeBaseName();
    if (out->category.isEmpty())
        out->category = QLatin1String("Other");

    return out->isValid() && !out->noDisplay;
}

void PanelIndex::scan(const QString &dir, const QString &visibleDir)
{
    m_entries.clear();

    QStringList visible;
    if (!visibleDir.isEmpty())
        visible = QDir(visibleDir).entryList(QStringList() << QLatin1String("*.desktop"),
                                             QDir::Files);

    const QStringList files = QDir(dir).entryList(QStringList() << QLatin1String("*.desktop"),
                                                  QDir::Files, QDir::Name);
    for (int i = 0; i < files.size(); ++i) {
        if (!visible.isEmpty() && !visible.contains(files.at(i)))
            continue;

        PanelEntry e;
        e.noDisplay = false;
        if (parse(dir + QLatin1Char('/') + files.at(i), &e))
            m_entries.append(e);
    }
}

// Categories come back in the order ThinPro shows them, with anything we did
// not anticipate appended alphabetically rather than dropped.
QStringList PanelIndex::categories() const
{
    static const char *order[] = {
        "Appearance", "Hardware", "Input Devices", "Network",
        "Security", "System", "Manageability", "Advanced", 0
    };

    QStringList present;
    for (int i = 0; i < m_entries.size(); ++i)
        if (!present.contains(m_entries.at(i).category))
            present.append(m_entries.at(i).category);

    QStringList sorted;
    for (int i = 0; order[i]; ++i) {
        const QString c = QLatin1String(order[i]);
        if (present.removeAll(c) > 0)
            sorted.append(c);
    }
    present.sort();
    sorted += present;
    return sorted;
}

QVector<PanelEntry> PanelIndex::inCategory(const QString &category) const
{
    QVector<PanelEntry> out;
    for (int i = 0; i < m_entries.size(); ++i)
        if (m_entries.at(i).category == category)
            out.append(m_entries.at(i));
    return out;
}
