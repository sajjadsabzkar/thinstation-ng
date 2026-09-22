// One control panel entry, read from a freedesktop .desktop file.
//
// This mirrors HP ThinPro exactly. There, /etc/hptc-control-panel/applications
// holds one .desktop per panel; the shell keeps the ones whose Categories
// contain "config-panel" and groups them by X-HPTC-Category. Which panels a
// non-admin sees is decided by a second directory, users/<name>/applications,
// holding the visible subset.
//
// We keep the same layout under /etc/tp/control-panel and the same two keys,
// renamed to X-TP-Category and X-TP-ExecAsRoot.

#ifndef TP_PANELENTRY_H
#define TP_PANELENTRY_H

#include <QString>
#include <QStringList>
#include <QVector>

struct PanelEntry {
    QString id;            // basename without .desktop
    QString name;          // Name=
    QString comment;       // Comment=
    QString icon;          // Icon=
    QString exec;          // Exec=
    QString category;      // X-TP-Category=
    QStringList keywords;  // Keywords=
    QString execAsRoot;    // X-TP-ExecAsRoot=, empty when the panel is unprivileged
    bool    noDisplay;     // NoDisplay=true hides an entry without deleting it

    bool isValid() const { return !id.isEmpty() && !name.isEmpty() && !exec.isEmpty(); }

    // Case-insensitive match against name, comment and keywords, for the
    // search box. ThinPro's panels carry translated Keywords= for the same
    // purpose.
    bool matches(const QString &needle) const;
};

class PanelIndex
{
public:
    PanelIndex();

    // Scans dir for *.desktop, keeping only entries whose Categories list
    // contains "config-panel". When visibleDir is non-empty, only entries
    // that also exist there are kept -- this is the user-mode subset.
    void scan(const QString &dir, const QString &visibleDir = QString());

    QVector<PanelEntry> entries() const { return m_entries; }
    QStringList categories() const;
    QVector<PanelEntry> inCategory(const QString &category) const;

private:
    static bool parse(const QString &path, PanelEntry *out);

    QVector<PanelEntry> m_entries;
};

#endif
