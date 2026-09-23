// The connection model, read straight out of the registry.
//
// Nothing here is hard-coded per protocol: a connection type is whatever the
// registry says it is, and the editor form is built from the type's field
// list plus the shared FieldSpec entries. Adding a protocol is a registry
// change, not a code change.

#ifndef TP_MODEL_H
#define TP_MODEL_H

#include <QString>
#include <QStringList>
#include <QVector>

class Registry;

struct FieldSpec {
    QString name;
    QString label;
    QString type;          // text | password | bool | choice
    QString hint;
    QString defaultValue;
    QStringList choices;

    static FieldSpec load(const Registry &reg, const QString &type,
                          const QString &field);
};

struct ConnectionType {
    QString id;            // registry id, also the ThinStation package name
    QString label;         // "FreeRDP 2"
    QString newLabel;      // "New RDP"
    QString package;       // ThinStation package to dispatch to via pkg
    QString icon;
    QString serverRequired;// required | optional | unused
    int     priority;
    QStringList fields;

    // Whether the package behind this type is actually in the image.
    //
    // The registry describes every protocol we know about, but a build can
    // leave one out -- ica and horizon are non-distributable and often are.
    // Offering a type with no /etc/init.d/<package> behind it is worse than
    // hiding it: pkg falls through to no_package, which prints "check your
    // thinstation.conf file" and then sleeps forever, so the client looks
    // hung rather than misconfigured.
    bool available;

    ConnectionType() : priority(50), available(false) {}

    bool isValid() const { return !id.isEmpty(); }
    QString node() const;

    // Looks for the init script pkg would dispatch to.
    static bool packageInstalled(const QString &package);
};

struct Connection {
    QString uuid;
    QString typeId;
    QString label;

    QString node() const;
    bool isValid() const { return !uuid.isEmpty() && !typeId.isEmpty(); }
};

class Model
{
public:
    explicit Model(Registry *reg);

    void reload();

    QVector<ConnectionType> types() const { return m_types; }
    ConnectionType type(const QString &id) const;

    QVector<Connection> connections() const { return m_connections; }
    Connection connection(const QString &uuid) const;

    // Creates a connection with the type's default label and the field
    // defaults from FieldSpec. Returns its uuid.
    QString createConnection(const QString &typeId);
    void removeConnection(const QString &uuid);

    Registry *registry() const { return m_reg; }

    QString fieldValue(const Connection &c, const QString &field) const;
    void setFieldValue(const Connection &c, const QString &field,
                       const QString &value);

private:
    Registry *m_reg;
    QVector<ConnectionType> m_types;
    QVector<Connection> m_connections;
};

#endif
