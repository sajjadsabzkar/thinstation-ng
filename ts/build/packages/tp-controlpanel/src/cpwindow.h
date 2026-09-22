// The control panel shell: a category list beside a grid of panels, with a
// search box that filters across every category at once.
//
// It launches panels as separate processes, exactly as ThinPro does. A panel
// that needs privilege carries X-TP-ExecAsRoot and is started through sudo,
// which is why the shell itself does not have to run as root.

#ifndef TP_CPWINDOW_H
#define TP_CPWINDOW_H

#include <QMainWindow>

#include "panelentry.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QStackedWidget;

class CpWindow : public QMainWindow
{
    Q_OBJECT

public:
    CpWindow(const PanelIndex &index, bool adminMode, QWidget *parent = 0);

private slots:
    void onCategoryChanged(int row);
    void onSearchChanged(const QString &text);
    void onPanelActivated(QListWidgetItem *item);

private:
    void populate();
    void showAll(const QString &filter);
    void launch(const PanelEntry &entry);

    PanelIndex   m_index;
    bool         m_adminMode;
    QLineEdit   *m_search;
    QListWidget *m_categories;
    QListWidget *m_panels;
    QLabel      *m_status;
    QStringList  m_categoryNames;
};

#endif
