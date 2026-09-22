// Two-layer key/value registry, wire-compatible with the tpreg shell tool.
//
// Layout mirrors HP ThinPro's Manticore registry so admin knowledge carries
// over, but there is no daemon: defaults are read from a read-only file in
// the image and overrides are written to a user file.
//
//   root/ConnectionType/<type>/coreSettings/<key>
//   root/ConnectionType/<type>/connections/<uuid>/<key>
//   root/FieldSpec/<field>/<key>
//   root/ConnectionManager/defaultConnection
//   root/users/<user>/<key>

#ifndef TP_REGISTRY_H
#define TP_REGISTRY_H

#include <QMap>
#include <QString>
#include <QStringList>

class Registry
{
public:
    Registry(const QString &defaultsPath, const QString &userPath);

    bool load();
    // Writes only the user layer. Returns false and leaves the old file in
    // place if the write fails, so a full disk cannot lose the registry.
    bool save();

    QString value(const QString &key, const QString &fallback = QString()) const;
    bool    boolValue(const QString &key, bool fallback = false) const;
    bool    contains(const QString &key) const;

    void setValue(const QString &key, const QString &value);
    void remove(const QString &key);
    // Removes every key under a prefix, e.g. a whole connection node.
    void removeTree(const QString &prefix);

    QStringList keys(const QString &prefix = QString()) const;
    // Immediate child node names, e.g. children("root/ConnectionType")
    // -> freerdp, firefox, horizon, ica
    QStringList children(const QString &prefix) const;

    QString userPath() const { return m_userPath; }
    QString lastError() const { return m_error; }

    static QString newUuid();

private:
    static bool parseFile(const QString &path, QMap<QString, QString> *out);

    QString m_defaultsPath;
    QString m_userPath;
    QMap<QString, QString> m_defaults;
    QMap<QString, QString> m_user;
    QString m_error;
};

#endif
