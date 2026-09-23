// The control panel shell, laid out the way ThinPro lays it out.
//
// ThinPro's control panel is not a list beside a grid. It is three screens
// that replace one another in the same window, and its own stylesheet names
// them: HomePage, ConfigList and ConfigPanel.
//
//   HomePage    a grey header carrying only a search field, then the
//               categories as tall rows: a 28px title with the names of the
//               panels inside it underneath, in small grey type.
//   ConfigList  a header with a back arrow and the category name as a
//               dropdown, a white sidebar of the panels in that category,
//               and the selected panel's form filling the rest of the
//               window. The selected row is a solid HP blue band.
//   ConfigPanel not a screen of its own: the panel is drawn inside that
//               same window, and one Apply button in the footer commits it.
//
// A panel that is a separate program rather than a form -- the CUPS pages,
// the display tool -- is marked in the sidebar with an external-link arrow
// and shows a single "Launch <name>" button where the form would be.
//
// Searching spans every category and replaces the category rows with
// matching panels, as it does there.

#ifndef TP_CPWINDOW_H
#define TP_CPWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "panelentry.h"

class PanelForm;
class QMenu;
class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class Registry;

class CpWindow : public QMainWindow
{
    Q_OBJECT

public:
    CpWindow(const PanelIndex &index, Registry *reg, bool adminMode,
             QWidget *parent = 0);

    // Open straight at a category, or at one panel by its id or .desktop
    // name. Unknown names leave the window on the home page rather than
    // failing: a wrong argument should not make the control panel refuse to
    // start.
    void openCategory(const QString &category);
    void openPanel(const QString &idOrDesktopName);

protected:
    // The category tiles are plain frames, so their clicks arrive here
    // rather than through a signal. A QPushButton would have been less
    // code, but a button does not take its size from the layout inside it
    // and the tiles came out as one squashed line.
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    void onSearchChanged(const QString &text);
    void onPanelActivated(QListWidgetItem *item);
    void onHome();
    void onCategoryPicked();
    void onApply();
    void onPanelDirty(bool dirty);

private:
    QWidget *buildHomePage();
    QWidget *buildCategoryPage();

    void showHome();
    void showCategory(const QString &category);
    void showSearch(const QString &needle);
    void showPanel(const PanelEntry &entry);
    void launch(const PanelEntry &entry);

    // A panel is embeddable when it is one of our own forms, which the Exec
    // line gives away: 'tp-panel <id>', optionally behind sudo. Anything
    // else is a separate program and gets a Launch button instead.
    static QString embeddedPanelId(const PanelEntry &entry);

    // A category row: the title plus the panels it holds, which is what
    // makes ThinPro's home page readable without icons.
    void addCategoryTile(const QString &category, int row, int column);
    void addPanelRow(QListWidget *list, const PanelEntry &entry);

    PanelIndex      m_index;
    Registry       *m_reg;
    bool            m_adminMode;

    QStackedWidget *m_pages;
    QLineEdit      *m_search;
    QGridLayout    *m_categories;   // home page: two columns of category tiles

    QListWidget    *m_panels;       // category page sidebar, and search results
    QPushButton    *m_categoryBox;  // the '<Category> v' dropdown in the header
    QMenu          *m_categoryMenu;
    QString         m_category;
    QStackedWidget *m_panelStack;   // the embedded forms, one page each
    QPushButton    *m_homeButton;
    QPushButton    *m_apply;
    QLabel         *m_status;

    // Forms are built the first time their panel is opened and kept, so
    // going back and forth in the sidebar does not re-run every
    // choicesCommand and valueCommand.
    QMap<QString, PanelForm *> m_forms;
    PanelForm      *m_current;
};

#endif
