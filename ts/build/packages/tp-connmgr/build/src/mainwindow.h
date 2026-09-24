// The Connection Manager window.
//
// Two presentations, the way ThinPro's hptc-kiosk has MainWindow and
// KioskWindow:
//
//   normal  an ordinary window; connections can be added, edited and deleted
//           when the user is allowed to.
//   kiosk   frameless and full screen, no window controls, no editing. The
//           user picks a connection and nothing else. Selected by
//           root/product/config = zero.

#ifndef TP_MAINWINDOW_H
#define TP_MAINWINDOW_H

#include <QWidget>

#include "model.h"

class QListWidget;
class QListWidgetItem;
class QPushButton;
class QLabel;
class Registry;

class MainWindow : public QWidget
{
    Q_OBJECT

public:
    MainWindow(Registry *reg, Model *model, bool kioskMode,
               QWidget *parent = 0);

private slots:
    void refresh();
    void onConnect();
    void onAdd();
    void onEdit();
    void onDelete();
    void onSelectionChanged();
    void onItemActivated(QListWidgetItem *item);

private:
    QString selectedUuid() const;
    void    saveOrWarn();

    Registry *m_reg;
    Model    *m_model;
    bool      m_kiosk;
    bool      m_mayEdit;

    QListWidget *m_list;
    QLabel      *m_status;
    QPushButton *m_connectBtn;
    QPushButton *m_addBtn;
    QPushButton *m_editBtn;
    QPushButton *m_deleteBtn;
    QPushButton *m_quitBtn;
};

#endif
