// A settings panel, built from the registry instead of from C++.
//
// ThinPro ships one shared-object plugin per panel under
// /usr/lib/manticore/config-panels, and each one is a GUI over a handful of
// Manticore keys. Thirty-six binaries is a lot of weight for a client with
// 1 GB of RAM, so here a panel is data: the registry describes its fields,
// this renders them, and a shell hook applies them to the running system.
//
//   root/ControlPanel/<panel>/title            window title
//   root/ControlPanel/<panel>/fields           space-separated field names
//   root/ControlPanel/<panel>/fields/<f>/label     shown beside the widget
//   root/ControlPanel/<panel>/fields/<f>/type      text|password|bool|choice|int|slider|info|action
//   root/ControlPanel/<panel>/fields/<f>/key       registry key holding the value
//   root/ControlPanel/<panel>/fields/<f>/choices   a|b|c
//   root/ControlPanel/<panel>/fields/<f>/choicesCommand  stdout lines become choices
//   root/ControlPanel/<panel>/fields/<f>/valueCommand    stdout fills an info field
//   root/ControlPanel/<panel>/fields/<f>/min|max|step|unit|hint|default
//   root/ControlPanel/<panel>/fields/<f>/section   optional tab name
//
// An 'action' field is a button rather than a setting. Several of ThinPro's
// panels are not forms at all -- Factory Reset, ThinState, Snapshots and the
// Task Manager exist to run one thing and show what it said -- so they need a
// field that does something instead of storing something:
//
//   root/ControlPanel/<panel>/fields/<f>/command   shell run when pressed
//   root/ControlPanel/<panel>/fields/<f>/confirm   asked first; empty = no ask
//   root/ControlPanel/<panel>/fields/<f>/buttonText  defaults to the label
//   root/ControlPanel/<panel>/fields/<f>/timeout   seconds, default 300
//   root/ControlPanel/<panel>/fields/<f>/prompt    asked for before running;
//                                                 the answer goes to stdin
//   root/ControlPanel/<panel>/fields/<f>/promptSecret  1 = mask the answer
//   root/ControlPanel/<panel>/fields/<f>/restartOnSuccess  1 = restart the
//                  program afterwards, for actions (administrator mode) that
//                  change what it shows
//
// The command's stdin is always closed, with or without a prompt: a script
// that reads stdin gets EOF instead of hanging the panel until the timeout.
//
// Actions are never written back to the registry and never reach the apply
// hook: the command is the whole of their effect.
//
// Saving writes the user layer and then runs /etc/tp/panels/<panel>.apply,
// which is where the translation to amixer, setxkbmap, xrandr and friends
// lives.

#ifndef TP_PANELFORM_H
#define TP_PANELFORM_H

#include <QDialog>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

class Registry;
class QDialogButtonBox;
class QWidget;

struct PanelField {
    QString name;
    QString label;
    QString type;
    QString key;
    QString hint;
    QString unit;
    QString section;
    QString defaultValue;
    QString valueCommand;
    QString command;
    QString confirm;
    QString buttonText;
    QString prompt;
    bool promptSecret;
    bool restartOnSuccess;
    QStringList choices;
    int min;
    int max;
    int step;
    int timeout;

    PanelField() : promptSecret(false), restartOnSuccess(false),
                   min(0), max(100), step(1), timeout(300) {}
};

class PanelForm : public QDialog
{
    Q_OBJECT

public:
    // Embedded mode drops the OK/Cancel/Apply row and leaves the widget
    // otherwise identical. ThinPro's control panel is one window that hosts
    // each panel inside itself with a single Apply in the footer, so the
    // same form has to work both ways: inside that window, and on its own
    // when tp-panel is run for one setting from the kiosk sidebar.
    PanelForm(Registry *reg, const QString &panelId, QWidget *parent = 0,
              bool embedded = false);

    bool isValid() const { return !m_fields.isEmpty(); }
    QString errorString() const { return m_error; }

    // For the host window's footer button.
    bool isDirty() const { return m_dirty; }
    bool applyChanges() { return commit(); }

signals:
    void dirtyChanged(bool dirty);

private slots:
    void onOk();
    void onApply();
    void onAction();
    void markDirty();

private:
    void        loadSpec();
    void        watchForChanges(QWidget *w);
    QWidget    *buildWidget(const PanelField &f, const QString &value);
    QString     readWidget(const PanelField &f, QWidget *w) const;
    QString     currentValue(const PanelField &f) const;
    bool        commit();
    // Runs /etc/tp/panels/<panel>.apply and reports a non-zero exit.
    bool        runApplyHook(QString *output);

    void        runAction(const PanelField &f);

    static QStringList runCommand(const QString &command, bool *ok = 0);

    Registry   *m_reg;
    QString     m_panelId;
    QString     m_title;
    QString     m_error;
    QVector<PanelField>      m_fields;
    QMap<QString, QWidget *> m_widgets;
    QDialogButtonBox        *m_buttons;
    bool                     m_embedded;
    bool                     m_dirty;
};

#endif
