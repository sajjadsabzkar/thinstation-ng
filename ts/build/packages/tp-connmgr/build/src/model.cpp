#include "model.h"
#include "registry.h"

#include <QFileInfo>

#include <QtAlgorithms>

QString ConnectionType::node() const
{
    return QLatin1String("root/ConnectionType/") + id;
}

QString Connection::node() const
{
    return QLatin1String("root/ConnectionType/") + typeId
         + QLatin1String("/connections/") + uuid;
}

FieldSpec FieldSpec::load(const Registry &reg, const QString &typeId,
                          const QString &field)
{
    FieldSpec s;
    s.name = field;

    // A type may override any part of the shared spec; the shared entry is
    // the fallback. This keeps the common case (address, username, ...)
    // defined exactly once.
    const QString shared = QLatin1String("root/FieldSpec/") + field + QLatin1Char('/');
    const QString own = QLatin1String("root/ConnectionType/") + typeId
                      + QLatin1String("/fieldSpec/") + field + QLatin1Char('/');

    s.label        = reg.value(own + QLatin1String("label"),
                     reg.value(shared + QLatin1String("label"), field));
    s.type         = reg.value(own + QLatin1String("type"),
                     reg.value(shared + QLatin1String("type"),
                               QLatin1String("text")));
    s.hint         = reg.value(own + QLatin1String("hint"),
                     reg.value(shared + QLatin1String("hint")));
    s.defaultValue = reg.value(own + QLatin1String("default"),
                     reg.value(shared + QLatin1String("default")));

    const QString choices = reg.value(own + QLatin1String("choices"),
                            reg.value(shared + QLatin1String("choices")));
    if (!choices.isEmpty())
        s.choices = choices.split(QLatin1Char('|'), Qt::SkipEmptyParts);

    return s;
}

static bool typeLessThan(const ConnectionType &a, const ConnectionType &b)
{
    if (a.priority != b.priority)
        return a.priority < b.priority;
    return a.label.localeAwareCompare(b.label) < 0;
}

static bool connectionLessThan(const Connection &a, const Connection &b)
{
    const int byLabel = a.label.localeAwareCompare(b.label);
    if (byLabel != 0)
        return byLabel < 0;
    return a.uuid < b.uuid;
}

// pkg dispatches everything it does not handle itself to
// /etc/init.d/<package>, so the presence of that script is exactly the
// question "can this connection actually start".
bool ConnectionType::packageInstalled(const QString &package)
{
    if (package.isEmpty())
        return false;

    const QByteArray root = qgetenv("TP_INITDIR");
    const QString dir = root.isEmpty() ? QLatin1String("/etc/init.d")
                                       : QString::fromLocal8Bit(root);
    return QFileInfo(dir + QLatin1Char('/') + package).exists();
}

Model::Model(Registry *reg) : m_reg(reg)
{
}

void Model::reload()
{
    m_types.clear();
    m_connections.clear();

    const QStringList ids = m_reg->children(QLatin1String("root/ConnectionType"));
    for (int i = 0; i < ids.size(); ++i) {
        const QString id = ids.at(i);
        const QString core = QLatin1String("root/ConnectionType/") + id
                           + QLatin1String("/coreSettings/");

        // A node with no label is not a usable type (ThinPro has such
        // registry-only overlays too, e.g. ffxen). Skip it rather than
        // showing a blank row.
        const QString label = m_reg->value(core + QLatin1String("label"));
        if (label.isEmpty())
            continue;

        ConnectionType t;
        t.id             = id;
        t.label          = label;
        t.newLabel       = m_reg->value(core + QLatin1String("newLabel"),
                                        QLatin1String("New Connection"));
        t.package        = m_reg->value(core + QLatin1String("package"), id);
        t.icon           = m_reg->value(core + QLatin1String("icon"), id);
        t.serverRequired = m_reg->value(core + QLatin1String("serverRequired"),
                                        QLatin1String("optional"));
        t.priority       = m_reg->value(core + QLatin1String("priority"),
                                        QLatin1String("50")).toInt();
        t.fields         = m_reg->value(core + QLatin1String("fields"))
                               .split(QLatin1Char(' '), Qt::SkipEmptyParts);
        t.available      = ConnectionType::packageInstalled(t.package);
        m_types.append(t);

        const QStringList uuids = m_reg->children(
            QLatin1String("root/ConnectionType/") + id + QLatin1String("/connections"));
        for (int j = 0; j < uuids.size(); ++j) {
            Connection c;
            c.uuid   = uuids.at(j);
            c.typeId = id;
            c.label  = m_reg->value(c.node() + QLatin1String("/label"), c.uuid);
            m_connections.append(c);
        }
    }

    std::sort(m_types.begin(), m_types.end(), typeLessThan);
    std::sort(m_connections.begin(), m_connections.end(), connectionLessThan);
}

ConnectionType Model::type(const QString &id) const
{
    for (int i = 0; i < m_types.size(); ++i)
        if (m_types.at(i).id == id)
            return m_types.at(i);
    return ConnectionType();
}

Connection Model::connection(const QString &uuid) const
{
    for (int i = 0; i < m_connections.size(); ++i)
        if (m_connections.at(i).uuid == uuid)
            return m_connections.at(i);
    return Connection();
}

QString Model::createConnection(const QString &typeId)
{
    const ConnectionType t = type(typeId);
    if (!t.isValid())
        return QString();

    Connection c;
    c.uuid   = Registry::newUuid();
    c.typeId = typeId;
    c.label  = t.newLabel;

    m_reg->setValue(c.node() + QLatin1String("/label"), c.label);
    for (int i = 0; i < t.fields.size(); ++i) {
        const QString f = t.fields.at(i);
        if (f == QLatin1String("label"))
            continue;
        const FieldSpec spec = FieldSpec::load(*m_reg, typeId, f);
        if (!spec.defaultValue.isEmpty())
            m_reg->setValue(c.node() + QLatin1Char('/') + f, spec.defaultValue);
    }
    return c.uuid;
}

void Model::removeConnection(const QString &uuid)
{
    const Connection c = connection(uuid);
    if (!c.isValid())
        return;
    m_reg->removeTree(c.node() + QLatin1Char('/'));
}

QString Model::fieldValue(const Connection &c, const QString &field) const
{
    return m_reg->value(c.node() + QLatin1Char('/') + field);
}

void Model::setFieldValue(const Connection &c, const QString &field,
                          const QString &value)
{
    m_reg->setValue(c.node() + QLatin1Char('/') + field, value);
}
