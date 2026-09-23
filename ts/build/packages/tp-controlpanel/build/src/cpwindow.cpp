#include "cpwindow.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QStatusBar>
#include <QVBoxLayout>

static const int PageHome = 0;
static const int PageList = 1;

CpWindow::CpWindow(const PanelIndex &index, bool adminMode, QWidget *parent)
    : QMainWindow(parent), m_index(index), m_adminMode(adminMode)
{
    setWindowTitle(adminMode ? tr("Control Panel - Administrator")
                             : tr("Control Panel"));

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildHomePage());
    m_pages->addWidget(buildListPage());
    setCentralWidget(m_pages);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status);

    const int n = m_index.entries().size();
    m_status->setText(adminMode
        ? tr("%n setting(s), administrator mode", "", n)
        : tr("%n setting(s)", "", n));

    showHome();
    resize(760, 560);
}

QWidget *CpWindow::buildHomePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ThinPro's HomePageHeader: a grey bar carrying the search field and
    // nothing else.
    QFrame *header = new QFrame(page);
    header->setObjectName(QLatin1String("tpHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(24, 0, 24, 0);

    m_homeTitle = new QLabel(tr("Settings"), header);
    m_homeTitle->setObjectName(QLatin1String("tpTitle"));

    m_search = new QLineEdit(header);
    m_search->setObjectName(QLatin1String("tpSearch"));
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_search->setMaximumWidth(280);

    hl->addWidget(m_homeTitle);
    hl->addStretch(1);
    hl->addWidget(m_search);
    v->addWidget(header);

    m_categories = new QListWidget(page);
    m_categories->setObjectName(QLatin1String("tpCategories"));
    m_categories->setSelectionMode(QAbstractItemView::SingleSelection);
    m_categories->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v->addWidget(m_categories, 1);

    connect(m_search, SIGNAL(textChanged(QString)),
            this, SLOT(onSearchChanged(QString)));
    connect(m_categories, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onCategoryActivated(QListWidgetItem*)));
    connect(m_categories, SIGNAL(itemActivated(QListWidgetItem*)),
            this, SLOT(onCategoryActivated(QListWidgetItem*)));

    return page;
}

QWidget *CpWindow::buildListPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // ConfigListHeader: back arrow, then the category name.
    QFrame *header = new QFrame(page);
    header->setObjectName(QLatin1String("tpHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(10, 0, 24, 0);
    hl->setSpacing(14);

    m_homeButton = new QPushButton(QString::fromUtf8("\xe2\x80\xb9"), header);
    m_homeButton->setObjectName(QLatin1String("homeButton"));
    m_homeButton->setToolTip(tr("Back to all settings"));

    m_listTitle = new QLabel(header);
    m_listTitle->setObjectName(QLatin1String("tpTitle"));

    hl->addWidget(m_homeButton);
    hl->addWidget(m_listTitle);
    hl->addStretch(1);
    v->addWidget(header);

    m_panels = new QListWidget(page);
    m_panels->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v->addWidget(m_panels, 1);

    connect(m_homeButton, SIGNAL(clicked()), this, SLOT(onHome()));
    connect(m_panels, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onPanelActivated(QListWidgetItem*)));
    connect(m_panels, SIGNAL(itemActivated(QListWidgetItem*)),
            this, SLOT(onPanelActivated(QListWidgetItem*)));

    return page;
}

void CpWindow::addCategoryRow(const QString &category)
{
    const QVector<PanelEntry> list = m_index.inCategory(category);
    if (list.isEmpty())
        return;

    QStringList names;
    for (int i = 0; i < list.size(); ++i)
        names << list.at(i).name;

    QWidget *row = new QWidget(m_categories);
    QVBoxLayout *v = new QVBoxLayout(row);
    v->setContentsMargins(24, 10, 24, 10);
    v->setSpacing(2);

    QLabel *title = new QLabel(category, row);
    title->setObjectName(QLatin1String("tpCategoryTitle"));

    QLabel *items = new QLabel(names.join(QLatin1String(" \xc2\xb7 ")), row);
    items->setObjectName(QLatin1String("tpCategoryItems"));
    items->setWordWrap(true);

    v->addWidget(title);
    v->addWidget(items);

    QListWidgetItem *item = new QListWidgetItem(m_categories);
    item->setData(Qt::UserRole, category);
    item->setSizeHint(row->sizeHint());
    m_categories->setItemWidget(item, row);
}

void CpWindow::addPanelRow(QListWidget *list, const PanelEntry &entry)
{
    QListWidgetItem *item = new QListWidgetItem(entry.name, list);
    item->setData(Qt::UserRole, entry.id);
    if (!entry.comment.isEmpty())
        item->setToolTip(entry.comment);
}

void CpWindow::showHome()
{
    m_categories->clear();
    const QStringList cats = m_index.categories();
    for (int i = 0; i < cats.size(); ++i)
        addCategoryRow(cats.at(i));

    m_pages->setCurrentIndex(PageHome);
}

void CpWindow::showCategory(const QString &category)
{
    m_panels->clear();
    m_listTitle->setText(category);

    const QVector<PanelEntry> list = m_index.inCategory(category);
    for (int i = 0; i < list.size(); ++i)
        addPanelRow(m_panels, list.at(i));

    m_pages->setCurrentIndex(PageList);
}

void CpWindow::showSearch(const QString &needle)
{
    m_panels->clear();
    m_listTitle->setText(tr("Results for \"%1\"").arg(needle));

    const QVector<PanelEntry> all = m_index.entries();
    int matches = 0;
    for (int i = 0; i < all.size(); ++i) {
        if (!all.at(i).matches(needle))
            continue;
        addPanelRow(m_panels, all.at(i));
        ++matches;
    }

    if (matches == 0) {
        QListWidgetItem *none = new QListWidgetItem(tr("No matches"), m_panels);
        none->setFlags(Qt::NoItemFlags);
    }

    m_pages->setCurrentIndex(PageList);
}

void CpWindow::onSearchChanged(const QString &text)
{
    if (text.trimmed().isEmpty())
        showHome();
    else
        showSearch(text.trimmed());
}

void CpWindow::onHome()
{
    // Leaving the results page should clear the search that produced it,
    // otherwise Back lands on a home page the search box contradicts.
    if (!m_search->text().isEmpty()) {
        const bool blocked = m_search->blockSignals(true);
        m_search->clear();
        m_search->blockSignals(blocked);
    }
    showHome();
}

void CpWindow::onCategoryActivated(QListWidgetItem *item)
{
    if (item)
        showCategory(item->data(Qt::UserRole).toString());
}

void CpWindow::onPanelActivated(QListWidgetItem *item)
{
    if (!item || !(item->flags() & Qt::ItemIsEnabled))
        return;

    const QString id = item->data(Qt::UserRole).toString();
    const QVector<PanelEntry> all = m_index.entries();
    for (int i = 0; i < all.size(); ++i)
        if (all.at(i).id == id) {
            launch(all.at(i));
            return;
        }
}

void CpWindow::launch(const PanelEntry &entry)
{
    // Exec fields carry freedesktop field codes (%f, %U, ...) that mean
    // nothing here; drop them rather than passing them on as arguments.
    QString command = entry.exec;
    command.remove(QRegularExpression(QLatin1String("%[a-zA-Z]")));

    QStringList args = command.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (args.isEmpty())
        return;

    const QString program = args.takeFirst();

    if (!QProcess::startDetached(program, args)) {
        QMessageBox::warning(this, tr("Control Panel"),
                             tr("Could not start %1.").arg(entry.name));
        return;
    }
    m_status->setText(tr("Opened %1").arg(entry.name));
}
