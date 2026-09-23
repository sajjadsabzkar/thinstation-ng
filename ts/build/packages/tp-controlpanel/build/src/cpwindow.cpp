#include "cpwindow.h"
#include "tpstyle.h"

#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
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

    // ThinPro's HomePageHeader: a grey strip carrying the search field at
    // the right and nothing else. There is no window title inside the
    // window; the title bar already says Control Panel.
    QFrame *header = new QFrame(page);
    header->setObjectName(QLatin1String("tpHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(24, 0, 24, 0);

    m_homeTitle = new QLabel(QString(), header);
    m_homeTitle->setObjectName(QLatin1String("tpTitle"));

    m_search = new QLineEdit(header);
    m_search->setObjectName(QLatin1String("tpSearch"));
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(420);

    hl->addWidget(m_homeTitle);
    hl->addStretch(1);
    hl->addWidget(m_search);
    v->addWidget(header);

    // The categories sit two to a row on white, each one an icon, a large
    // title and the panels it holds listed underneath. Not a list: ThinPro
    // lays these out as a grid, and with seven categories a single column
    // would need scrolling on an 800px screen.
    QWidget *body = new QWidget(page);
    body->setObjectName(QLatin1String("tpHomeBody"));
    m_categories = new QGridLayout(body);
    m_categories->setContentsMargins(60, 40, 60, 40);
    m_categories->setHorizontalSpacing(60);
    m_categories->setVerticalSpacing(40);

    QScrollArea *scroll = new QScrollArea(page);
    scroll->setWidget(body);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v->addWidget(scroll, 1);

    connect(m_search, SIGNAL(textChanged(QString)),
            this, SLOT(onSearchChanged(QString)));

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

// One glyph per category, standing in for the icon ThinPro draws there. The
// target image carries no icon theme, which is why the control panel already
// falls back to drawn tiles elsewhere.
//
// Which glyphs exist depends on the fonts the build selected, so each
// category names several, best first, and TpStyle::glyph picks the first one
// the font can really draw. The first attempt at this drew five empty boxes.

static QString categoryGlyph(const QFont &font, const QString &category)
{
    static const char *system[]  = { "\xe2\x8f\xb1", "\xe2\x9a\x99", "\xe2\x97\x8f", "S", 0 };
    static const char *security[]= { "\xf0\x9f\x94\x91", "\xe2\x9a\xbf", "\xe2\x9a\xb7", "S", 0 };
    static const char *manage[]  = { "\xf0\x9f\x94\x92", "\xe2\x9a\x91", "\xe2\x96\xa3", "M", 0 };
    static const char *input[]   = { "\xe2\x8c\xa8", "\xe2\x8c\xa7", "I", 0 };
    static const char *hardware[]= { "\xf0\x9f\x94\x8a", "\xe2\x99\xac", "\xe2\x96\xa0", "H", 0 };
    static const char *look[]    = { "\xf0\x9f\x96\x8c", "\xe2\x97\xa7", "\xe2\x97\x91", "A", 0 };
    static const char *other[]   = { "\xf0\x9f\x94\xa7", "\xe2\x9a\x99", "\xe2\x80\xa6", "+", 0 };

    if (category == QLatin1String("System"))        return TpStyle::glyph(font, system);
    if (category == QLatin1String("Security"))      return TpStyle::glyph(font, security);
    if (category == QLatin1String("Manageability")) return TpStyle::glyph(font, manage);
    if (category == QLatin1String("Input Devices")) return TpStyle::glyph(font, input);
    if (category == QLatin1String("Hardware"))      return TpStyle::glyph(font, hardware);
    if (category == QLatin1String("Appearance"))    return TpStyle::glyph(font, look);
    return TpStyle::glyph(font, other);
}

void CpWindow::addCategoryTile(const QString &category, int row, int column)
{
    const QVector<PanelEntry> list = m_index.inCategory(category);
    if (list.isEmpty())
        return;

    QStringList names;
    for (int i = 0; i < list.size(); ++i)
        names << list.at(i).name;

    // A frame, not a button: QPushButton sizes itself from its text, so a
    // layout put inside one is squeezed into a single line. The frame takes
    // its height from the labels, and eventFilter turns a click on it into
    // the same thing a button press would have been.
    QFrame *tile = new QFrame;
    tile->setCursor(Qt::PointingHandCursor);
    tile->setAttribute(Qt::WA_Hover, true);
    tile->setProperty("tpCategory", category);
    tile->setStyleSheet(QLatin1String(
        "QFrame { background: transparent; border: none; }"
        "QFrame:hover { background: #F4F4F4; }"));
    tile->installEventFilter(this);

    QVBoxLayout *v = new QVBoxLayout(tile);
    v->setContentsMargins(10, 10, 10, 10);
    v->setSpacing(10);

    QHBoxLayout *head = new QHBoxLayout;
    head->setSpacing(12);

    QLabel *icon = new QLabel(categoryGlyph(font(), category), tile);
    icon->setObjectName(QLatin1String("tpCategoryIcon"));
    icon->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    QLabel *title = new QLabel(category, tile);
    title->setObjectName(QLatin1String("tpCategoryTitle"));
    title->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    head->addWidget(icon);
    head->addWidget(title);
    head->addStretch(1);
    v->addLayout(head);

    // Comma separated, the way ThinPro writes them, wrapping to as many
    // lines as it takes.
    QLabel *items = new QLabel(names.join(QLatin1String(", ")), tile);
    items->setObjectName(QLatin1String("tpCategoryItems"));
    items->setWordWrap(true);
    items->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    items->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    v->addWidget(items, 1);

    m_categories->addWidget(tile, row, column, Qt::AlignTop);
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
    // Rebuilt from scratch: a panel can appear or disappear while the
    // window is open, and the grid is cheap to make.
    while (QLayoutItem *old = m_categories->takeAt(0)) {
        if (old->widget())
            old->widget()->deleteLater();
        delete old;
    }

    const QStringList cats = m_index.categories();
    for (int i = 0; i < cats.size(); ++i)
        addCategoryTile(cats.at(i), i / 2, i % 2);

    m_categories->setColumnStretch(0, 1);
    m_categories->setColumnStretch(1, 1);
    m_categories->setRowStretch(m_categories->rowCount(), 1);

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

bool CpWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonRelease) {
        const QString category = watched->property("tpCategory").toString();
        if (!category.isEmpty()) {
            showCategory(category);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
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
