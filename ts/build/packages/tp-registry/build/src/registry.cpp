#include "registry.h"

#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTextStream>
#include <QUuid>

// Written into the user layer to suppress a key that exists in the defaults.
// Without it, deleting such a key would silently resurrect the default on the
// next read. Must stay byte-identical to the tpreg shell tool.
static const QString kTombstone = QString(QChar(0x01)) + QLatin1String("deleted");

Registry::Registry(const QString &defaultsPath, const QString &userPath)
    : m_defaultsPath(defaultsPath), m_userPath(userPath)
{
}

bool Registry::parseFile(const QString &path, QMap<QString, QString> *out)
{
    QFile f(path);
    if (!f.exists())
        return true;                    // absent is fine, just no entries
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in(&f);
    in.setCodec("UTF-8");
    while (!in.atEnd()) {
        const QString line = in.readLine();
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#')))
            continue;
        // Split on the FIRST '=' only, so values may contain '=' freely
        // (URLs with query strings, RDP options, ...).
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        out->insert(line.left(eq), line.mid(eq + 1));
    }
    return true;
}

bool Registry::load()
{
    m_defaults.clear();
    m_user.clear();
    if (!parseFile(m_defaultsPath, &m_defaults)) {
        m_error = QObject::tr("Cannot read %1").arg(m_defaultsPath);
        return false;
    }
    if (!parseFile(m_userPath, &m_user)) {
        m_error = QObject::tr("Cannot read %1").arg(m_userPath);
        return false;
    }
    return true;
}

bool Registry::save()
{
    QFileInfo fi(m_userPath);
    QDir().mkpath(fi.absolutePath());

    // QSaveFile writes to a temporary and renames on commit, so an
    // interrupted write cannot truncate an existing registry.
    QSaveFile f(m_userPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_error = QObject::tr("Cannot write %1: %2").arg(m_userPath, f.errorString());
        return false;
    }

    QTextStream out(&f);
    out.setCodec("UTF-8");
    out << "# Written by tp-connmgr. Overrides /etc/tp/registry.defaults.\n";
    for (QMap<QString, QString>::const_iterator it = m_user.constBegin();
         it != m_user.constEnd(); ++it) {
        out << it.key() << '=' << it.value() << '\n';
    }
    out.flush();

    if (!f.commit()) {
        m_error = QObject::tr("Cannot write %1: %2").arg(m_userPath, f.errorString());
        return false;
    }
    QFile::setPermissions(m_userPath,
                          QFile::ReadOwner | QFile::WriteOwner);
    return true;
}

QString Registry::value(const QString &key, const QString &fallback) const
{
    QMap<QString, QString>::const_iterator it = m_user.constFind(key);
    if (it != m_user.constEnd())
        return it.value() == kTombstone ? fallback : it.value();
    it = m_defaults.constFind(key);
    if (it != m_defaults.constEnd())
        return it.value();
    return fallback;
}

bool Registry::boolValue(const QString &key, bool fallback) const
{
    const QString v = value(key).trimmed().toLower();
    if (v.isEmpty())
        return fallback;
    return v == QLatin1String("1") || v == QLatin1String("on")
        || v == QLatin1String("true") || v == QLatin1String("yes");
}

bool Registry::contains(const QString &key) const
{
    QMap<QString, QString>::const_iterator it = m_user.constFind(key);
    if (it != m_user.constEnd())
        return it.value() != kTombstone;
    return m_defaults.contains(key);
}

void Registry::setValue(const QString &key, const QString &value)
{
    // Newlines would corrupt the line-based file format.
    QString clean = value;
    clean.replace(QLatin1Char('\n'), QLatin1Char(' '));
    clean.replace(QLatin1Char('\r'), QLatin1Char(' '));
    m_user.insert(key, clean);
}

void Registry::remove(const QString &key)
{
    m_user.remove(key);
    if (m_defaults.contains(key))
        m_user.insert(key, kTombstone);
}

void Registry::removeTree(const QString &prefix)
{
    const QStringList doomed = keys(prefix);
    for (int i = 0; i < doomed.size(); ++i)
        remove(doomed.at(i));
}

QStringList Registry::keys(const QString &prefix) const
{
    QStringList result;
    QMap<QString, QString> merged = m_defaults;
    for (QMap<QString, QString>::const_iterator it = m_user.constBegin();
         it != m_user.constEnd(); ++it) {
        merged.insert(it.key(), it.value());
    }
    for (QMap<QString, QString>::const_iterator it = merged.constBegin();
         it != merged.constEnd(); ++it) {
        if (it.value() == kTombstone)
            continue;
        if (prefix.isEmpty() || it.key().startsWith(prefix))
            result.append(it.key());
    }
    return result;
}

QStringList Registry::children(const QString &prefix) const
{
    QString base = prefix;
    while (base.endsWith(QLatin1Char('/')))
        base.chop(1);
    base += QLatin1Char('/');

    QStringList result;
    const QStringList all = keys(base);
    for (int i = 0; i < all.size(); ++i) {
        const QString rest = all.at(i).mid(base.size());
        const int slash = rest.indexOf(QLatin1Char('/'));
        const QString child = slash < 0 ? rest : rest.left(slash);
        if (!child.isEmpty() && !result.contains(child))
            result.append(child);
    }
    result.sort();
    return result;
}

QString Registry::newUuid()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
