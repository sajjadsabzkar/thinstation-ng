#include "editdialog.h"
#include "registry.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

EditDialog::EditDialog(Model *model, const Connection &conn, QWidget *parent)
    : QDialog(parent), m_model(model), m_conn(conn)
{
    m_type = model->type(conn.typeId);

    setWindowTitle(tr("%1 - %2").arg(m_type.label, conn.label));
    setModal(true);

    QVBoxLayout *outer = new QVBoxLayout(this);
    QFormLayout *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    for (int i = 0; i < m_type.fields.size(); ++i) {
        const QString field = m_type.fields.at(i);
        const FieldSpec spec = FieldSpec::load(*m_model->registry(), m_type.id, field);
        m_specs.insert(field, spec);

        QString value = m_model->fieldValue(m_conn, field);
        if (value.isEmpty() && !spec.defaultValue.isEmpty())
            value = spec.defaultValue;

        QWidget *w = buildField(spec, value);
        m_widgets.insert(field, w);

        if (spec.type == QLatin1String("bool"))
            form->addRow(QString(), w);   // the checkbox carries its own text
        else
            form->addRow(spec.label + QLatin1Char(':'), w);
    }

    outer->addLayout(form);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
                                     Qt::Horizontal, this);
    outer->addWidget(m_buttons);
    connect(m_buttons, SIGNAL(accepted()), this, SLOT(onAccept()));
    connect(m_buttons, SIGNAL(rejected()), this, SLOT(reject()));

    resize(420, sizeHint().height());
}

QWidget *EditDialog::buildField(const FieldSpec &spec, const QString &value)
{
    if (spec.type == QLatin1String("bool")) {
        QCheckBox *cb = new QCheckBox(spec.label, this);
        const QString v = value.trimmed().toLower();
        cb->setChecked(v == QLatin1String("1") || v == QLatin1String("on")
                    || v == QLatin1String("true") || v == QLatin1String("yes"));
        return cb;
    }

    if (spec.type == QLatin1String("choice")) {
        QComboBox *cb = new QComboBox(this);
        cb->addItems(spec.choices);
        const int idx = spec.choices.indexOf(value);
        cb->setCurrentIndex(idx < 0 ? 0 : idx);
        return cb;
    }

    QLineEdit *le = new QLineEdit(value, this);
    if (spec.type == QLatin1String("password"))
        le->setEchoMode(QLineEdit::Password);
    if (!spec.hint.isEmpty())
        le->setPlaceholderText(spec.hint);
    return le;
}

QString EditDialog::readField(const FieldSpec &spec, QWidget *w) const
{
    if (spec.type == QLatin1String("bool")) {
        QCheckBox *cb = qobject_cast<QCheckBox *>(w);
        return cb && cb->isChecked() ? QLatin1String("1") : QLatin1String("0");
    }
    if (spec.type == QLatin1String("choice")) {
        QComboBox *cb = qobject_cast<QComboBox *>(w);
        return cb ? cb->currentText() : QString();
    }
    QLineEdit *le = qobject_cast<QLineEdit *>(w);
    return le ? le->text() : QString();
}

void EditDialog::onAccept()
{
    // A connection with no name is unusable in the list, and a type that
    // demands a server is unusable without one. Catch both before writing.
    const QWidget *labelWidget = m_widgets.value(QLatin1String("label"));
    if (labelWidget) {
        const QLineEdit *le = qobject_cast<const QLineEdit *>(labelWidget);
        if (le && le->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Connection Manager"),
                                 tr("Please give this connection a name."));
            return;
        }
    }

    if (m_type.serverRequired == QLatin1String("required")) {
        const QWidget *addr = m_widgets.value(QLatin1String("address"));
        const QLineEdit *le = qobject_cast<const QLineEdit *>(addr);
        if (le && le->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, tr("Connection Manager"),
                                 tr("%1 connections need a server address.")
                                     .arg(m_type.label));
            return;
        }
    }

    for (QMap<QString, QWidget *>::const_iterator it = m_widgets.constBegin();
         it != m_widgets.constEnd(); ++it) {
        const FieldSpec spec = m_specs.value(it.key());
        m_model->setFieldValue(m_conn, it.key(), readField(spec, it.value()));
    }

    accept();
}
