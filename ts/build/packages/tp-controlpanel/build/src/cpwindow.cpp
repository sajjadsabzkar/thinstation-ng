#include "cpwindow.h"
#include "panelform.h"
#include "tpstyle.h"

#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
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

CpWindow::CpWindow(const PanelIndex &index, Registry *reg, bool adminMode,
                   QWidget *parent)
    : QMainWindow(parent), m_index(index), m_reg(reg), m_adminMode(adminMode),
      m_current(0)
{
    setWindowTitle(adminMode ? tr("Control Panel - Administrator")
                             : tr("Control Panel"));

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildHomePage());
    m_pages->addWidget(buildCategoryPage());
    setCentralWidget(m_pages);

    // No status bar: ThinPro has none, and the window title already says
    // whether this is administrator mode. m_status stays as a place to put
    // a message without giving it a permanent strip of the window.
    m_status = new QLabel(this);
    m_status->hide();

    showHome();
    resize(980, 680);
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

    m_search = new QLineEdit(header);
    m_search->setObjectName(QLatin1String("tpSearch"));
    m_search->setPlaceholderText(tr("Search"));
    m_search->setClearButtonEnabled(true);
    m_search->setMinimumWidth(420);

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

QWidget *CpWindow::buildCategoryPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    // --- header: back arrow, then the category as a dropdown -------------
    QFrame *header = new QFrame(page);
    header->setObjectName(QLatin1String("tpHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(14, 8, 24, 8);
    hl->setSpacing(16);

    m_homeButton = new QPushButton(QString::fromUtf8("\xe2\x80\xb9"), header);
    m_homeButton->setObjectName(QLatin1String("tpBackButton"));
    m_homeButton->setToolTip(tr("Back to all settings"));

    // ThinPro puts the category name here as a dropdown, so an admin can
    // move between categories without going back to the home page first.
    //
    // A button with a menu rather than a QComboBox: the caret has to sit
    // right after the text, and a borderless combo draws its arrow at the
    // far edge of whatever width it is given. Styling that arrow with the
    // usual CSS triangle does not work in Qt either -- it comes out as a
    // small filled box.
    m_categoryBox = new QPushButton(header);
    m_categoryBox->setObjectName(QLatin1String("tpCategoryPicker"));
    m_categoryBox->setFlat(true);
    m_categoryBox->setCursor(Qt::PointingHandCursor);
    m_categoryMenu = new QMenu(m_categoryBox);
    m_categoryBox->setMenu(m_categoryMenu);

    hl->addWidget(m_homeButton);
    hl->addWidget(m_categoryBox);
    hl->addStretch(1);
    v->addWidget(header);

    // --- body: the panels on the left, the selected one on the right -----
    QWidget *body = new QWidget(page);
    QHBoxLayout *bl = new QHBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    bl->setSpacing(0);

    m_panels = new QListWidget(body);
    m_panels->setObjectName(QLatin1String("tpPanelList"));
    m_panels->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_panels->setFixedWidth(290);

    m_panelStack = new QStackedWidget(body);
    m_panelStack->setObjectName(QLatin1String("tpPanelStack"));

    bl->addWidget(m_panels);
    bl->addWidget(m_panelStack, 1);
    v->addWidget(body, 1);

    // --- footer: one Apply for whatever is showing -----------------------
    QFrame *footer = new QFrame(page);
    footer->setObjectName(QLatin1String("tpFooter"));
    QHBoxLayout *fl = new QHBoxLayout(footer);
    fl->setContentsMargins(24, 8, 24, 8);

    m_apply = new QPushButton(tr("Apply"), footer);
    m_apply->setProperty("tpPrimary", true);
    m_apply->setMinimumSize(130, 40);
    m_apply->setEnabled(false);

    fl->addStretch(1);
    fl->addWidget(m_apply);
    v->addWidget(footer);

    connect(m_homeButton, SIGNAL(clicked()), this, SLOT(onHome()));
    connect(m_apply, SIGNAL(clicked()), this, SLOT(onApply()));

    connect(m_panels, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)),
            this, SLOT(onPanelActivated(QListWidgetItem*)));

    return page;
}

QString CpWindow::embeddedPanelId(const PanelEntry &entry)
{
    QStringList args = entry.exec.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (!args.isEmpty() && args.first() == QLatin1String("sudo"))
        args.removeFirst();

    // Match the program by its basename, so both 'tp-panel' and an absolute
    // path to it count.
    if (args.size() < 2)
        return QString();
    const QString program = args.first().section(QLatin1Char('/'), -1);
    if (program != QLatin1String("tp-panel"))
        return QString();

    return args.at(1);
}

void CpWindow::showPanel(const PanelEntry &entry)
{
    const QString panelId = embeddedPanelId(entry);

    if (panelId.isEmpty()) {
        // A separate program. ThinPro shows a single button where the form
        // would be rather than pretending it can draw something it cannot.
        QWidget *page = new QWidget(m_panelStack);
        QVBoxLayout *v = new QVBoxLayout(page);
        v->addStretch(1);

        QPushButton *launchButton =
            new QPushButton(tr("Launch %1").arg(entry.name), page);
        launchButton->setMinimumSize(280, 52);
        const PanelEntry copy = entry;
        connect(launchButton, &QPushButton::clicked,
                this, [this, copy]() { launch(copy); });

        QHBoxLayout *h = new QHBoxLayout;
        h->addStretch(1);
        h->addWidget(launchButton);
        h->addStretch(1);
        v->addLayout(h);
        v->addStretch(1);

        m_panelStack->addWidget(page);
        m_panelStack->setCurrentWidget(page);
        m_current = 0;
        m_apply->setEnabled(false);
        return;
    }

    PanelForm *form = m_forms.value(panelId);
    if (!form) {
        form = new PanelForm(m_reg, panelId, m_panelStack, true);
        if (!form->isValid()) {
            // A .desktop naming a panel the registry does not define. Say
            // which, rather than showing an empty pane.
            delete form;
            QLabel *problem = new QLabel(
                tr("%1 is listed but not defined in the registry.")
                    .arg(entry.name), m_panelStack);
            problem->setAlignment(Qt::AlignCenter);
            problem->setWordWrap(true);
            m_panelStack->addWidget(problem);
            m_panelStack->setCurrentWidget(problem);
            m_current = 0;
            m_apply->setEnabled(false);
            return;
        }
        connect(form, SIGNAL(dirtyChanged(bool)), this, SLOT(onPanelDirty(bool)));
        m_forms.insert(panelId, form);
        m_panelStack->addWidget(form);
    }

    m_panelStack->setCurrentWidget(form);
    m_current = form;
    m_apply->setEnabled(form->isDirty());
}

void CpWindow::openCategory(const QString &category)
{
    const QStringList known = m_index.categories();
    for (int i = 0; i < known.size(); ++i)
        if (known.at(i).compare(category, Qt::CaseInsensitive) == 0) {
            showCategory(known.at(i));
            return;
        }
}

void CpWindow::openPanel(const QString &idOrDesktopName)
{
    QString id = idOrDesktopName;
    if (id.endsWith(QLatin1String(".desktop")))
        id.chop(8);

    const QVector<PanelEntry> all = m_index.entries();
    for (int i = 0; i < all.size(); ++i) {
        const PanelEntry &e = all.at(i);
        if (e.id != id && embeddedPanelId(e) != id)
            continue;

        showCategory(e.category);

        // Select its row, so the sidebar agrees with the pane.
        for (int r = 0; r < m_panels->count(); ++r)
            if (m_panels->item(r)->data(Qt::UserRole).toString() == e.id) {
                m_panels->setCurrentRow(r);
                break;
            }
        return;
    }
}

void CpWindow::onPanelDirty(bool dirty)
{
    if (sender() == m_current)
        m_apply->setEnabled(dirty);
}

void CpWindow::onApply()
{
    if (!m_current)
        return;
    if (m_current->applyChanges())
        m_status->setText(tr("Applied"));
}

void CpWindow::onCategoryPicked()
{
    QAction *a = qobject_cast<QAction *>(sender());
    if (!a)
        return;
    const QString category = a->data().toString();
    if (!category.isEmpty() && category != m_category)
        showCategory(category);
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

// A glyph per panel, keyed off the Icon= name in its .desktop file. Same
// approach as the category tiles: several candidates, first one the font can
// draw wins, so no row ever shows an empty box.
static QString panelGlyph(const QFont &font, const QString &icon)
{
    static const struct { const char *name; const char *candidates[4]; } map[] = {
        { "sound",       { "\xf0\x9f\x94\x8a", "\xe2\x99\xac", "\xe2\x96\xb6", 0 } },
        { "usb",         { "\xe2\x9a\xb2", "\xe2\x87\xa5", "U", 0 } },
        { "display",     { "\xf0\x9f\x96\xb5", "\xe2\x96\xa3", "D", 0 } },
        { "keyboard",    { "\xe2\x8c\xa8", "\xe2\x8c\xa7", "K", 0 } },
        { "mouse",       { "\xf0\x9f\x96\xb1", "\xe2\x97\x8b", "M", 0 } },
        { "network",     { "\xe2\x87\xb5", "\xe2\x87\x84", "N", 0 } },
        { "power",       { "\xe2\x8f\xbb", "\xe2\x9a\xa1", "P", 0 } },
        { "restart",     { "\xe2\x86\xbb", "\xe2\x9f\xb3", "R", 0 } },
        { "snapshot",    { "\xe2\x97\x89", "\xe2\x97\x8b", "S", 0 } },
        { "reset",       { "\xe2\x9a\x99", "\xe2\x9c\xb1", "F", 0 } },
        { "task",        { "\xf0\x9f\x93\x8a", "\xe2\x96\xa4", "T", 0 } },
        { "terminal",    { "\xe2\x96\xae", "\xe2\x96\xa0", ">", 0 } },
        { "security",    { "\xf0\x9f\x94\x91", "\xe2\x9a\xbf", "S", 0 } },
        { "certificate", { "\xf0\x9f\x93\x9c", "\xe2\x9c\x93", "C", 0 } },
        { "firewall",    { "\xf0\x9f\x9b\xa1", "\xe2\x96\xa5", "F", 0 } },
        { "admin",       { "\xf0\x9f\x94\x93", "\xe2\x9a\xbf", "A", 0 } },
        { "ssh",         { "\xf0\x9f\x94\x92", "\xe2\x9a\x91", "S", 0 } },
        { "vnc",         { "\xf0\x9f\x91\x81", "\xe2\x97\x8e", "V", 0 } },
        { "snmp",        { "\xf0\x9f\x93\xa1", "\xe2\x87\x84", "S", 0 } },
        { "thinstate",   { "\xf0\x9f\x92\xbe", "\xe2\x96\xa4", "T", 0 } },
        { "background",  { "\xf0\x9f\x96\x8c", "\xe2\x97\xa7", "B", 0 } },
        { "language",    { "\xf0\x9f\x8c\x90", "\xe2\x97\x8f", "L", 0 } },
        { "shortcuts",   { "\xe2\x8c\x98", "\xe2\x8c\xa8", "K", 0 } },
        { "touch",       { "\xe2\x98\x9e", "\xe2\x97\x8f", "T", 0 } },
        { "registry",    { "\xf0\x9f\x94\xa7", "\xe2\x9a\x99", "R", 0 } },
        { "compat",      { "\xe2\x9c\x93", "\xe2\x97\x8b", "C", 0 } },
        { "printer",     { "\xf0\x9f\x96\xa8", "\xe2\x96\xa4", "P", 0 } },
        { "bluetooth",   { "\xe2\x9a\xb7", "\xe2\x97\x88", "B", 0 } },
        { "serial",      { "\xe2\x8e\x87", "\xe2\x96\xa6", "S", 0 } },
        { "wireless",    { "\xf0\x9f\x93\xb6", "\xe2\x88\xbf", "W", 0 } },
        { 0, { 0, 0, 0, 0 } }
    };

    for (int i = 0; map[i].name; ++i)
        if (icon == QLatin1String(map[i].name))
            return TpStyle::glyph(font, map[i].candidates);

    static const char *fallback[] = { "\xe2\x97\x8f", "\xe2\x80\xa2", 0 };
    return TpStyle::glyph(font, fallback);
}

void CpWindow::addPanelRow(QListWidget *list, const PanelEntry &entry)
{
    const QString glyph = panelGlyph(font(), entry.icon);
    QString label = glyph.isEmpty() ? entry.name
                                    : glyph + QLatin1String("   ") + entry.name;

    // ThinPro marks the rows that open a separate program with an
    // external-link arrow, so nobody is surprised when a new window appears
    // instead of the pane filling in.
    if (embeddedPanelId(entry).isEmpty())
        label += QLatin1String("   ") + QString::fromUtf8("\xe2\x86\x97");

    QListWidgetItem *item = new QListWidgetItem(label, list);
    item->setData(Qt::UserRole, entry.id);
    if (!entry.comment.isEmpty())
        item->setToolTip(entry.comment);

    // An even height for every row: without this the rows take their height
    // from whichever glyph the font happened to have, and the blue selection
    // band ends up a different size on each one.
    item->setSizeHint(QSize(0, 56));
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
    m_category = category;
    m_categoryBox->setText(category + QLatin1String("  ")
                           + QString::fromUtf8("\xe2\x8c\x84"));

    m_categoryMenu->clear();
    const QStringList all = m_index.categories();
    for (int i = 0; i < all.size(); ++i) {
        QAction *a = m_categoryMenu->addAction(all.at(i));
        a->setData(all.at(i));
        connect(a, SIGNAL(triggered()), this, SLOT(onCategoryPicked()));
    }

    m_panels->clear();
    const QVector<PanelEntry> list = m_index.inCategory(category);
    for (int i = 0; i < list.size(); ++i)
        addPanelRow(m_panels, list.at(i));

    m_pages->setCurrentIndex(PageList);

    // Open the first panel, so the pane beside the list is never blank.
    if (m_panels->count() > 0)
        m_panels->setCurrentRow(0);
}

void CpWindow::showSearch(const QString &needle)
{
    m_category.clear();
    m_categoryBox->setText(tr("Results for \"%1\"").arg(needle));
    m_categoryMenu->clear();

    m_panels->clear();

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
    if (matches > 0)
        m_panels->setCurrentRow(0);
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
    m_current = 0;
    m_apply->setEnabled(false);
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
            showPanel(all.at(i));
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
