#include "cpwindow.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QRegularExpression>
#include <QSplitter>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

// Panel ids double as icon names, so a theme icon is tried first and a
// generic one is used when the theme has nothing. The image ships a small
// icon set rather than a full theme, so most lookups land on the fallback.
static QIcon panelIcon(const PanelEntry &e)
{
    QIcon icon = QIcon::fromTheme(e.icon);
    if (icon.isNull())
        icon = QIcon::fromTheme(QLatin1String("preferences-system"));
    return icon;
}

CpWindow::CpWindow(const PanelIndex &index, bool adminMode, QWidget *parent)
    : QMainWindow(parent), m_index(index), m_adminMode(adminMode)
{
    setWindowTitle(adminMode ? tr("Control Panel - Administrator")
                             : tr("Control Panel"));

    QWidget *central = new QWidget(this);
    QVBoxLayout *outer = new QVBoxLayout(central);

    m_search = new QLineEdit(central);
    m_search->setPlaceholderText(tr("Search settings"));
    m_search->setClearButtonEnabled(true);
    outer->addWidget(m_search);

    QSplitter *split = new QSplitter(Qt::Horizontal, central);

    m_categories = new QListWidget(split);
    m_categories->setMaximumWidth(190);
    m_categories->setAlternatingRowColors(false);

    m_panels = new QListWidget(split);
    m_panels->setViewMode(QListView::IconMode);
    m_panels->setIconSize(QSize(48, 48));
    m_panels->setGridSize(QSize(140, 96));
    m_panels->setResizeMode(QListView::Adjust);
    m_panels->setMovement(QListView::Static);
    m_panels->setWordWrap(true);
    m_panels->setSpacing(6);

    split->addWidget(m_categories);
    split->addWidget(m_panels);
    split->setStretchFactor(1, 1);
    outer->addWidget(split, 1);

    setCentralWidget(central);

    m_status = new QLabel(this);
    statusBar()->addWidget(m_status);

    connect(m_categories, SIGNAL(currentRowChanged(int)),
            this, SLOT(onCategoryChanged(int)));
    connect(m_search, SIGNAL(textChanged(QString)),
            this, SLOT(onSearchChanged(QString)));
    connect(m_panels, SIGNAL(itemActivated(QListWidgetItem*)),
            this, SLOT(onPanelActivated(QListWidgetItem*)));
    connect(m_panels, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(onPanelActivated(QListWidgetItem*)));

    populate();
    resize(720, 460);
}

void CpWindow::populate()
{
    m_categoryNames = m_index.categories();

    m_categories->clear();
    m_categories->addItem(tr("All Settings"));
    for (int i = 0; i < m_categoryNames.size(); ++i)
        m_categories->addItem(m_categoryNames.at(i));

    m_categories->setCurrentRow(0);

    const int n = m_index.entries().size();
    m_status->setText(m_adminMode
        ? tr("%n panel(s), administrator mode", "", n)
        : tr("%n panel(s)", "", n));
}

void CpWindow::showAll(const QString &filter)
{
    m_panels->clear();
    const QVector<PanelEntry> all = m_index.entries();
    for (int i = 0; i < all.size(); ++i) {
        if (!all.at(i).matches(filter))
            continue;
        QListWidgetItem *item = new QListWidgetItem(panelIcon(all.at(i)),
                                                    all.at(i).name, m_panels);
        item->setToolTip(all.at(i).comment);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setData(Qt::UserRole, all.at(i).id);
    }
}

void CpWindow::onCategoryChanged(int row)
{
    if (!m_search->text().isEmpty())
        return;                     // a search spans every category

    if (row <= 0) {
        showAll(QString());
        return;
    }

    m_panels->clear();
    const QVector<PanelEntry> list = m_index.inCategory(m_categoryNames.at(row - 1));
    for (int i = 0; i < list.size(); ++i) {
        QListWidgetItem *item = new QListWidgetItem(panelIcon(list.at(i)),
                                                    list.at(i).name, m_panels);
        item->setToolTip(list.at(i).comment);
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setData(Qt::UserRole, list.at(i).id);
    }
}

void CpWindow::onSearchChanged(const QString &text)
{
    if (text.isEmpty()) {
        onCategoryChanged(m_categories->currentRow());
        return;
    }
    m_categories->setCurrentRow(0);
    showAll(text);
}

void CpWindow::onPanelActivated(QListWidgetItem *item)
{
    if (!item)
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
    m_status->setText(tr("Started %1").arg(entry.name));
}
