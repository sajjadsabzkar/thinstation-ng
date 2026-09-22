// Connection editor. The form is generated from the type's field list, so
// this file knows nothing about RDP, Citrix, Horizon or the browser.

#ifndef TP_EDITDIALOG_H
#define TP_EDITDIALOG_H

#include <QDialog>
#include <QMap>

#include "model.h"

class QWidget;
class QDialogButtonBox;

class EditDialog : public QDialog
{
    Q_OBJECT

public:
    EditDialog(Model *model, const Connection &conn, QWidget *parent = 0);

private slots:
    void onAccept();

private:
    QWidget *buildField(const FieldSpec &spec, const QString &value);
    QString  readField(const FieldSpec &spec, QWidget *w) const;

    Model *m_model;
    Connection m_conn;
    ConnectionType m_type;
    QMap<QString, QWidget *> m_widgets;
    QMap<QString, FieldSpec> m_specs;
    QDialogButtonBox *m_buttons;
};

#endif
