#include "panelform.h"
#include "registry.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

static bool truthy(const QString &v)
{
    const QString s = v.trimmed().toLower();
    return s == QLatin1String("1") || s == QLatin1String("on")
        || s == QLatin1String("true") || s == QLatin1String("yes");
}

PanelForm::PanelForm(Registry *reg, const QString &panelId, QWidget *parent)
    : QDialog(parent), m_reg(reg), m_panelId(panelId), m_buttons(0)
{
    loadSpec();
    if (m_fields.isEmpty()) {
        m_error = tr("No panel named '%1' is defined in the registry.").arg(panelId);
        return;
    }

    setWindowTitle(m_title.isEmpty() ? panelId : m_title);

    QVBoxLayout *outer = new QVBoxLayout(this);

    // Sections become tabs. A panel with no sections is a plain form, which
    // is what most of them are.
    QStringList sections;
    for (int i = 0; i < m_fields.size(); ++i)
        if (!m_fields.at(i).section.isEmpty()
            && !sections.contains(m_fields.at(i).section))
            sections.append(m_fields.at(i).section);

    if (sections.isEmpty()) {
        QFormLayout *form = new QFormLayout;
        form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        for (int i = 0; i < m_fields.size(); ++i) {
            const PanelField &f = m_fields.at(i);
            QWidget *w = buildWidget(f, currentValue(f));
            m_widgets.insert(f.name, w);
            // A checkbox and a button already say what they are; a label in
            // the left column beside them would only repeat the text.
            if (f.type == QLatin1String("bool")
                || f.type == QLatin1String("action"))
                form->addRow(QString(), w);
            else
                form->addRow(f.label + QLatin1Char(':'), w);
        }
        outer->addLayout(form);
    } else {
        QTabWidget *tabs = new QTabWidget(this);
        for (int s = 0; s < sections.size(); ++s) {
            QWidget *page = new QWidget(tabs);
            QFormLayout *form = new QFormLayout(page);
            form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
            for (int i = 0; i < m_fields.size(); ++i) {
                const PanelField &f = m_fields.at(i);
                if (f.section != sections.at(s))
                    continue;
                QWidget *w = buildWidget(f, currentValue(f));
                m_widgets.insert(f.name, w);
                if (f.type == QLatin1String("bool")
                    || f.type == QLatin1String("action"))
                    form->addRow(QString(), w);
                else
                    form->addRow(f.label + QLatin1Char(':'), w);
            }
            tabs->addTab(page, sections.at(s));
        }
        outer->addWidget(tabs);
    }

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok
                                   | QDialogButtonBox::Cancel
                                   | QDialogButtonBox::Apply,
                                     Qt::Horizontal, this);
    outer->addWidget(m_buttons);

    connect(m_buttons, SIGNAL(accepted()), this, SLOT(onOk()));
    connect(m_buttons, SIGNAL(rejected()), this, SLOT(reject()));
    connect(m_buttons->button(QDialogButtonBox::Apply), SIGNAL(clicked()),
            this, SLOT(onApply()));

    resize(440, sizeHint().height());
}

QStringList PanelForm::runCommand(const QString &command, bool *ok)
{
    QProcess p;
    p.start(QLatin1String("/bin/sh"), QStringList()
            << QLatin1String("-c") << command);
    // A hook that hangs must not take the panel down with it.
    if (!p.waitForFinished(10000)) {
        p.kill();
        p.waitForFinished(1000);
        if (ok) *ok = false;
        return QStringList();
    }
    if (ok) *ok = (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);

    const QString out = QString::fromLocal8Bit(p.readAllStandardOutput());
    return out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

void PanelForm::loadSpec()
{
    const QString node = QLatin1String("root/ControlPanel/") + m_panelId;
    m_title = m_reg->value(node + QLatin1String("/title"));

    const QStringList names = m_reg->value(node + QLatin1String("/fields"))
                                  .split(QLatin1Char(' '), Qt::SkipEmptyParts);

    for (int i = 0; i < names.size(); ++i) {
        const QString base = node + QLatin1String("/fields/") + names.at(i) + QLatin1Char('/');

        PanelField f;
        f.name         = names.at(i);
        f.label        = m_reg->value(base + QLatin1String("label"), names.at(i));
        f.type         = m_reg->value(base + QLatin1String("type"),
                                      QLatin1String("text"));
        f.key          = m_reg->value(base + QLatin1String("key"),
                                      node + QLatin1String("/values/") + names.at(i));
        f.hint         = m_reg->value(base + QLatin1String("hint"));
        f.unit         = m_reg->value(base + QLatin1String("unit"));
        f.section      = m_reg->value(base + QLatin1String("section"));
        f.defaultValue = m_reg->value(base + QLatin1String("default"));
        f.valueCommand = m_reg->value(base + QLatin1String("valueCommand"));
        f.command      = m_reg->value(base + QLatin1String("command"));
        f.confirm      = m_reg->value(base + QLatin1String("confirm"));
        f.buttonText   = m_reg->value(base + QLatin1String("buttonText"), f.label);
        f.timeout      = m_reg->value(base + QLatin1String("timeout"),
                                      QLatin1String("300")).toInt();
        f.min          = m_reg->value(base + QLatin1String("min"),
                                      QLatin1String("0")).toInt();
        f.max          = m_reg->value(base + QLatin1String("max"),
                                      QLatin1String("100")).toInt();
        f.step         = m_reg->value(base + QLatin1String("step"),
                                      QLatin1String("1")).toInt();

        const QString choices = m_reg->value(base + QLatin1String("choices"));
        if (!choices.isEmpty())
            f.choices = choices.split(QLatin1Char('|'), Qt::SkipEmptyParts);

        const QString cmd = m_reg->value(base + QLatin1String("choicesCommand"));
        if (!cmd.isEmpty()) {
            // Dynamic choices are what make a display or keyboard panel
            // possible without compiling xrandr and xkb knowledge into us.
            const QStringList dynamic = runCommand(cmd);
            if (!dynamic.isEmpty())
                f.choices = dynamic;
        }

        m_fields.append(f);
    }
}

QString PanelForm::currentValue(const PanelField &f) const
{
    if (f.type == QLatin1String("action"))
        return QString();

    if (f.type == QLatin1String("info") && !f.valueCommand.isEmpty())
        return runCommand(f.valueCommand).join(QLatin1String(" "));

    QString v = m_reg->value(f.key);
    if (v.isEmpty())
        v = f.defaultValue;
    return v;
}

QWidget *PanelForm::buildWidget(const PanelField &f, const QString &value)
{
    if (f.type == QLatin1String("bool")) {
        QCheckBox *cb = new QCheckBox(f.label, this);
        cb->setChecked(truthy(value));
        if (!f.hint.isEmpty())
            cb->setToolTip(f.hint);
        return cb;
    }

    if (f.type == QLatin1String("choice")) {
        QComboBox *cb = new QComboBox(this);
        cb->addItems(f.choices);
        const int idx = f.choices.indexOf(value);
        if (idx >= 0)
            cb->setCurrentIndex(idx);
        else if (!value.isEmpty())
            cb->setEditText(value);     // keep a value the list does not know
        if (!f.hint.isEmpty())
            cb->setToolTip(f.hint);
        return cb;
    }

    if (f.type == QLatin1String("int")) {
        QSpinBox *sb = new QSpinBox(this);
        sb->setRange(f.min, f.max);
        sb->setSingleStep(f.step);
        sb->setValue(value.toInt());
        if (!f.unit.isEmpty())
            sb->setSuffix(QLatin1Char(' ') + f.unit);
        return sb;
    }

    if (f.type == QLatin1String("slider")) {
        // A slider plus a live readout, in one widget so the form layout
        // keeps them on the same row. readWidget finds the slider again
        // with findChild, so nothing has to be stashed away here.
        QWidget *box = new QWidget(this);
        QHBoxLayout *row = new QHBoxLayout(box);
        row->setContentsMargins(0, 0, 0, 0);

        QSlider *sl = new QSlider(Qt::Horizontal, box);
        sl->setRange(f.min, f.max);
        sl->setSingleStep(f.step);
        sl->setPageStep(f.step * 5);
        sl->setValue(value.toInt());

        QLabel *lbl = new QLabel(QString::number(sl->value()) + f.unit, box);
        lbl->setMinimumWidth(44);
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        row->addWidget(sl, 1);
        row->addWidget(lbl);
        connect(sl, SIGNAL(valueChanged(int)), lbl, SLOT(setNum(int)));
        return box;
    }

    if (f.type == QLatin1String("action")) {
        QPushButton *b = new QPushButton(f.buttonText.isEmpty() ? f.label
                                                                : f.buttonText, this);
        // The slot finds the field again by name, so one slot serves every
        // button on the panel.
        b->setProperty("tpField", f.name);
        if (!f.hint.isEmpty())
            b->setToolTip(f.hint);
        connect(b, SIGNAL(clicked()), this, SLOT(onAction()));
        return b;
    }

    if (f.type == QLatin1String("info")) {
        QLabel *l = new QLabel(value, this);
        l->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return l;
    }

    QLineEdit *le = new QLineEdit(value, this);
    if (f.type == QLatin1String("password"))
        le->setEchoMode(QLineEdit::Password);
    if (!f.hint.isEmpty())
        le->setPlaceholderText(f.hint);
    return le;
}

QString PanelForm::readWidget(const PanelField &f, QWidget *w) const
{
    if (f.type == QLatin1String("bool")) {
        QCheckBox *cb = qobject_cast<QCheckBox *>(w);
        return (cb && cb->isChecked()) ? QLatin1String("1") : QLatin1String("0");
    }
    if (f.type == QLatin1String("choice")) {
        QComboBox *cb = qobject_cast<QComboBox *>(w);
        return cb ? cb->currentText() : QString();
    }
    if (f.type == QLatin1String("int")) {
        QSpinBox *sb = qobject_cast<QSpinBox *>(w);
        return sb ? QString::number(sb->value()) : QString();
    }
    if (f.type == QLatin1String("slider")) {
        QSlider *sl = w->findChild<QSlider *>();
        return sl ? QString::number(sl->value()) : QString();
    }
    if (f.type == QLatin1String("info") || f.type == QLatin1String("action"))
        return QString();           // read-only, never written back

    QLineEdit *le = qobject_cast<QLineEdit *>(w);
    return le ? le->text() : QString();
}

bool PanelForm::runApplyHook(QString *output)
{
    const QString hook = QLatin1String("/etc/tp/panels/") + m_panelId
                       + QLatin1String(".apply");

    QProcess p;
    p.start(hook, QStringList());
    if (!p.waitForStarted(3000))
        return true;                // no hook is not an error

    if (!p.waitForFinished(20000)) {
        p.kill();
        p.waitForFinished(1000);
        if (output) *output = tr("%1 did not finish.").arg(hook);
        return false;
    }

    if (output)
        *output = QString::fromLocal8Bit(p.readAllStandardError()).trimmed();

    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

bool PanelForm::commit()
{
    for (int i = 0; i < m_fields.size(); ++i) {
        const PanelField &f = m_fields.at(i);
        if (f.type == QLatin1String("info") || f.type == QLatin1String("action"))
            continue;
        QWidget *w = m_widgets.value(f.name);
        if (!w)
            continue;
        m_reg->setValue(f.key, readWidget(f, w));
    }

    if (!m_reg->save()) {
        QMessageBox::critical(this, windowTitle(),
                              tr("Could not save settings: %1")
                                  .arg(m_reg->lastError()));
        return false;
    }

    QString hookError;
    if (!runApplyHook(&hookError)) {
        QMessageBox::warning(this, windowTitle(),
                             hookError.isEmpty()
                                 ? tr("The settings were saved but could not be applied.")
                                 : tr("The settings were saved but could not be applied:\n%1")
                                       .arg(hookError));
        // Saved is saved; report it and keep the dialog usable.
    }
    return true;
}

void PanelForm::onAction()
{
    const QString name = sender() ? sender()->property("tpField").toString()
                                  : QString();
    for (int i = 0; i < m_fields.size(); ++i)
        if (m_fields.at(i).name == name) {
            runAction(m_fields.at(i));
            return;
        }
}

void PanelForm::runAction(const PanelField &f)
{
    if (f.command.isEmpty())
        return;

    // Factory reset and friends are one click away from destroying the
    // client's configuration, so a panel can demand a yes first.
    if (!f.confirm.isEmpty()
        && QMessageBox::question(this, windowTitle(), f.confirm,
                                 QMessageBox::Yes | QMessageBox::No,
                                 QMessageBox::No) != QMessageBox::Yes)
        return;

    QPushButton *button = qobject_cast<QPushButton *>(m_widgets.value(f.name));
    if (button)
        button->setEnabled(false);
    setCursor(Qt::WaitCursor);

    QProcess p;
    p.setProcessChannelMode(QProcess::MergedChannels);
    p.start(QLatin1String("/bin/sh"),
            QStringList() << QLatin1String("-c") << f.command);

    bool started = p.waitForStarted(5000);
    bool finished = false;
    if (started)
        finished = p.waitForFinished(f.timeout > 0 ? f.timeout * 1000 : -1);

    if (started && !finished) {
        p.kill();
        p.waitForFinished(1000);
    }

    unsetCursor();
    if (button)
        button->setEnabled(true);

    const QString output =
        QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();

    if (!started) {
        QMessageBox::critical(this, f.label, tr("Could not run the command."));
        return;
    }
    if (!finished) {
        QMessageBox::warning(this, f.label,
                             tr("The command did not finish within %1 seconds "
                                "and was stopped.").arg(f.timeout));
        return;
    }

    const bool ok = (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);

    // Show the output whatever the exit code: for something like the task
    // manager the output *is* the point, and for a failure it is the only
    // clue the user gets.
    QMessageBox box(ok ? QMessageBox::Information : QMessageBox::Warning,
                    f.label,
                    ok ? tr("Done.") : tr("Finished with errors."),
                    QMessageBox::Ok, this);
    if (!output.isEmpty()) {
        if (output.length() < 400 && !output.contains(QLatin1Char('\n')))
            box.setInformativeText(output);
        else
            box.setDetailedText(output);
    }
    box.exec();
}

void PanelForm::onApply()
{
    commit();
}

void PanelForm::onOk()
{
    if (commit())
        accept();
}
