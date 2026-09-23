// The control panel shell, laid out the way ThinPro lays it out.
//
// ThinPro's control panel is not a list beside a grid. It is three screens
// that replace one another in the same window, and its own stylesheet names
// them: HomePage, ConfigList and ConfigPanel.
//
//   HomePage    a grey header carrying only a search field, then the
//               categories as tall rows: a 28px title with the names of the
//               panels inside it underneath, in small grey type.
//   ConfigList  a header with a back arrow and the category name, then the
//               panels in that category as 50px rows.
//   ConfigPanel the panel itself. Ours runs as a separate process, tp-panel,
//               which draws the same header and footer.
//
// Searching spans every category and replaces the category rows with
// matching panels, as it does there.

#ifndef TP_CPWINDOW_H
#define TP_CPWINDOW_H

#include <QMainWindow>

#include "panelentry.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

class CpWindow : public QMainWindow
{
    Q_OBJECT

public:
    CpWindow(const PanelIndex &index, bool adminMode, QWidget *parent = 0);

private slots:
    void onSearchChanged(const QString &text);
    void onCategoryActivated(QListWidgetItem *item);
    void onPanelActivated(QListWidgetItem *item);
    void onHome();

private:
    QWidget *buildHomePage();
    QWidget *buildListPage();

    void showHome();
    void showCategory(const QString &category);
    void showSearch(const QString &needle);
    void launch(const PanelEntry &entry);

    // A category row: the title plus the panels it holds, which is what
    // makes ThinPro's home page readable without icons.
    void addCategoryRow(const QString &category);
    void addPanelRow(QListWidget *list, const PanelEntry &entry);

    PanelIndex      m_index;
    bool            m_adminMode;

    QStackedWidget *m_pages;
    QLineEdit      *m_search;
    QListWidget    *m_categories;   // home page
    QListWidget    *m_panels;       // category page and search results
    QLabel         *m_listTitle;
    QLabel         *m_homeTitle;
    QPushButton    *m_homeButton;
    QLabel         *m_status;
};

#endif
