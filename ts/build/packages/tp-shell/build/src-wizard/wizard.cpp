#include "wizard.h"
#include "tpstyle.h"
#include "registry.h"

#include <QApplication>
#include <QComboBox>
#include <QFontMetrics>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

// The pages, in order. The cover and the done page are full width; the four
// between them are split.
enum {
    PageCover = 0,
    PageKeyboard,
    PageNetwork,
    PageDateTime,
    PageCheck,
    PageDone,
    PageCount
};

// The four split pages are the ones the step pill counts.
static const int kSplitPages = 4;

static const struct {
    const char *code;
    const char *name;
    const char *keyboard;
} kLanguages[] = {
    { "en_US.UTF-8", "English", "us" },
    { "fr_FR.UTF-8", "Fran\xc3\xa7" "ais", "fr" },
    { "de_DE.UTF-8", "Deutsch", "de" },
    { "es_ES.UTF-8", "Espa\xc3\xb1ol", "es" },
    { "ru_RU.UTF-8", "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9", "ru" },
    { "ja_JP.UTF-8", "\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e", "jp" },
    { "ko_KR.UTF-8", "\xed\x95\x9c\xea\xb5\xad\xec\x96\xb4", "kr" },
    { "zh_CN.UTF-8", "\xe7\xae\x80\xe4\xbd\x93\xe4\xb8\xad\xe6\x96\x87", "cn" },
    { "fa_IR.UTF-8", "\xd9\x81\xd8\xa7\xd8\xb1\xd8\xb3\xdb\x8c", "ir" },
    { "tr_TR.UTF-8", "T\xc3\xbcrk\xc3\xa7" "e", "tr" },
    { "ar_SA.UTF-8", "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9", "ara" },
    { "it_IT.UTF-8", "Italiano", "it" },
    { 0, 0, 0 }
};

// Every split page says the same thing under its headline. That repetition
// is ThinPro's, and it is the point: nothing here is a decision anyone has
// to get right first time.
static const char *kReassurance =
    QT_TRANSLATE_NOOP("Wizard", "You can customize it later, don't worry.");

Wizard::Wizard(Registry *reg, QWidget *parent)
    : QWidget(parent), m_reg(reg), m_language(0),
      m_keyboardFilter(0), m_keyboard(0), m_check(0), m_checkDone(false)
{
    // One large glyph per step. Emoji are in neither Liberation nor DejaVu,
    // so each has plainer stand-ins, and TpStyle::glyph takes the first the
    // fonts can draw.
    static const char *const kbGlyphs[]   = { "\xe2\x8c\xa8", "\xf0\x9f\x96\xae", "A", 0 };
    static const char *const netGlyphs[]  = { "\xf0\x9f\x93\xa1", "\xf0\x9f\x96\xa7",
                                              "\xe2\x87\x85", "\xe2\x86\x94", 0 };
    static const char *const timeGlyphs[] = { "\xf0\x9f\x95\x92", "\xe2\x8f\xb2",
                                              "\xe2\x8c\x9a", "\xe2\x97\xb7", 0 };
    static const char *const hwGlyphs[]   = { "\xe2\x9a\x99", "\xe2\x9c\x93", 0 };

    setWindowTitle(tr("Initial Setup"));

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_pages = new QStackedWidget(this);

    m_pages->addWidget(buildCoverPage());

    QPushButton *back, *next;
    m_pages->addWidget(buildSplitPage(
        tr("Select the standard keyboard format:"), buildKeyboardBody(),
        tr("Keyboard"), tr("Select the default keyboard layout"),
        TpStyle::glyph(font(), kbGlyphs), &back, &next));
    m_backButtons.append(back); m_nextButtons.append(next);

    m_pages->addWidget(buildSplitPage(
        tr("Select a network connection:"), buildNetworkBody(),
        tr("Network"), tr("Select the default network"),
        TpStyle::glyph(font(), netGlyphs), &back, &next));
    m_backButtons.append(back); m_nextButtons.append(next);

    m_pages->addWidget(buildSplitPage(
        tr("Select the appropriate timezone:"), buildDateTimeBody(),
        tr("Date & Time"), tr("Set the system date and time"),
        TpStyle::glyph(font(), timeGlyphs), &back, &next));
    m_backButtons.append(back); m_nextButtons.append(next);

    m_pages->addWidget(buildSplitPage(
        tr("Check this hardware:"), buildCheckBody(),
        tr("Hardware"), tr("See what this client can do"),
        TpStyle::glyph(font(), hwGlyphs), &back, &next));
    m_backButtons.append(back); m_nextButtons.append(next);

    m_pages->addWidget(buildDonePage());

    outer->addWidget(m_pages, 1);

    selectLanguage(0);
    showPage(PageCover);
    resize(1024, 700);
}

// ---------------------------------------------------------------------------
// The cover page
// ---------------------------------------------------------------------------

QWidget *Wizard::buildCoverPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QLatin1String("tpWizardCover"));

    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 40, 0, 40);
    v->setSpacing(0);

    QLabel *brand = new QLabel(tr("ThinPro NG"), page);
    brand->setAlignment(Qt::AlignCenter);
    brand->setStyleSheet(QLatin1String(
        "color: #0096D6; font-size: 56px; font-weight: 300;"));
    v->addWidget(brand);
    v->addSpacing(30);

    // ThinPro puts a photograph of a client on a desk here. We have no such
    // artwork to ship, and a stretched placeholder photograph would look
    // worse than none, so the band is a flat field carrying the strapline.
    QLabel *hero = new QLabel(tr("Thin client"), page);
    hero->setAlignment(Qt::AlignCenter);
    hero->setMinimumHeight(190);
    hero->setStyleSheet(QLatin1String(
        "background: #4C5680; color: #C6CBDF; font-size: 22px;"));
    v->addWidget(hero, 1);
    v->addSpacing(34);

    // Six to a row rather than one long line. ThinPro fits ten across
    // because its screenshot is a 2171px panel; on the 1024x768 this OS is
    // built for, a single row pushes the window wider than the screen and
    // the last few names fall off the edge.
    QGridLayout *langs = new QGridLayout;
    langs->setContentsMargins(40, 0, 40, 0);
    langs->setHorizontalSpacing(24);
    langs->setVerticalSpacing(10);

    int shown = 0;
    for (int i = 0; kLanguages[i].code; ++i) {
        const QString name = QString::fromUtf8(kLanguages[i].name);

        QPushButton *b = new QPushButton(name, page);
        b->setObjectName(QLatin1String("tpLanguageChoice"));

        // Only offer a language this image can actually draw, in the font
        // the button will really use once the stylesheet has had its say.
        // The build carries no CJK font, and a row of empty boxes is a
        // worse answer than a shorter list: nobody can pick a language
        // whose name they cannot read.
        b->ensurePolished();
        if (i != 0 && !TpStyle::drawable(name, b->font())) {
            delete b;
            continue;
        }

        b->setFlat(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("tpIndex", i);
        connect(b, SIGNAL(clicked()), this, SLOT(onLanguageClicked()));
        m_languageButtons.append(b);
        langs->addWidget(b, shown / 6, shown % 6, Qt::AlignCenter);
        ++shown;
    }
    v->addLayout(langs);
    v->addSpacing(30);

    QPushButton *go = new QPushButton(tr("Continue Setup"), page);
    go->setProperty("tpPrimary", true);
    go->setMinimumSize(420, 56);
    connect(go, SIGNAL(clicked()), this, SLOT(onNext()));

    QHBoxLayout *gl = new QHBoxLayout;
    gl->addStretch(1);
    gl->addWidget(go);
    gl->addStretch(1);
    v->addLayout(gl);

    return page;
}

void Wizard::onLanguageClicked()
{
    QObject *s = sender();
    if (s)
        selectLanguage(s->property("tpIndex").toInt());
}

void Wizard::selectLanguage(int index)
{
    m_language = index;
    for (int i = 0; i < m_languageButtons.size(); ++i) {
        QPushButton *b = m_languageButtons.at(i);
        b->setProperty("tpSelected", i == index);
        b->style()->unpolish(b);
        b->style()->polish(b);
    }

    // The keyboard follows the language until someone changes it by hand.
    if (m_keyboard && index >= 0 && kLanguages[index].code) {
        const QString kb = QString::fromLatin1(kLanguages[index].keyboard);
        for (int i = 0; i < m_keyboard->count(); ++i) {
            QListWidgetItem *item = m_keyboard->item(i);
            if (item->data(Qt::UserRole).toString() == kb) {
                m_keyboard->setCurrentItem(item);
                m_keyboard->scrollToItem(item, QAbstractItemView::PositionAtCenter);
                break;
            }
        }
    }
}

void Wizard::onKeyboardFilter(const QString &text)
{
    const QString needle = text.trimmed();
    QListWidgetItem *firstShown = 0;
    for (int i = 0; i < m_keyboard->count(); ++i) {
        QListWidgetItem *item = m_keyboard->item(i);
        const bool match = needle.isEmpty()
            || item->text().contains(needle, Qt::CaseInsensitive)
            || item->data(Qt::UserRole).toString()
                   .startsWith(needle, Qt::CaseInsensitive);
        item->setHidden(!match);
        if (match && !firstShown)
            firstShown = item;
    }
    // Keep a visible row selected, so Next never commits a layout the
    // filter has hidden.
    QListWidgetItem *current = m_keyboard->currentItem();
    if ((!current || current->isHidden()) && firstShown)
        m_keyboard->setCurrentItem(firstShown);
    if (m_keyboard->currentItem())
        m_keyboard->scrollToItem(m_keyboard->currentItem());
}

// ---------------------------------------------------------------------------
// The split pages
// ---------------------------------------------------------------------------

QFrame *Wizard::buildStepBar(int stepIndex, const QString &stepName)
{
    QFrame *bar = new QFrame(this);
    bar->setObjectName(QLatin1String("tpWizardSteps"));

    QHBoxLayout *h = new QHBoxLayout(bar);
    h->setContentsMargins(24, 0, 24, 0);
    h->setSpacing(14);
    h->addStretch(1);

    // Filled dots for the steps already behind, the current step spelled
    // out, hollow dots for what is still to come.
    for (int i = 0; i < stepIndex; ++i) {
        QLabel *dot = new QLabel(QString::fromUtf8("\xe2\x97\x8f"), bar);
        dot->setObjectName(QLatin1String("tpWizardStepDot"));
        h->addWidget(dot);
    }

    QLabel *name = new QLabel(stepName, bar);
    name->setObjectName(QLatin1String("tpWizardStepName"));
    h->addWidget(name);

    for (int i = stepIndex + 1; i < kSplitPages; ++i) {
        QLabel *dot = new QLabel(QString::fromUtf8("\xe2\x97\x8b"), bar);
        dot->setObjectName(QLatin1String("tpWizardStepDot"));
        h->addWidget(dot);
    }

    h->addStretch(1);
    return bar;
}

QWidget *Wizard::buildSplitPage(const QString &question,
                                QWidget *body,
                                const QString &stepName,
                                const QString &headline,
                                const QString &glyph,
                                QPushButton **backOut,
                                QPushButton **nextOut)
{
    QWidget *page = new QWidget(this);
    QHBoxLayout *h = new QHBoxLayout(page);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(0);

    // --- left: white, the question and the controls ----------------------
    QWidget *form = new QWidget(page);
    form->setObjectName(QLatin1String("tpWizardForm"));
    QVBoxLayout *fv = new QVBoxLayout(form);
    fv->setContentsMargins(46, 56, 46, 40);
    fv->setSpacing(0);

    QLabel *q = new QLabel(question, form);
    q->setObjectName(QLatin1String("tpWizardQuestion"));
    q->setWordWrap(true);
    fv->addWidget(q);
    fv->addSpacing(28);

    body->setParent(form);
    fv->addWidget(body, 1);

    QPushButton *back = new QPushButton(tr("Previous"), form);
    QPushButton *next = new QPushButton(tr("Next"), form);
    back->setProperty("tpPrimary", true);
    next->setProperty("tpPrimary", true);
    back->setMinimumSize(150, 48);
    next->setMinimumSize(150, 48);
    connect(back, SIGNAL(clicked()), this, SLOT(onBack()));
    connect(next, SIGNAL(clicked()), this, SLOT(onNext()));

    QHBoxLayout *nav = new QHBoxLayout;
    nav->setSpacing(18);
    nav->addWidget(back);
    nav->addWidget(next);
    nav->addStretch(1);
    fv->addLayout(nav);

    *backOut = back;
    *nextOut = next;

    // --- right: slate, the step pill and the illustration -----------------
    QWidget *aside = new QWidget(page);
    aside->setObjectName(QLatin1String("tpWizardAside"));
    QVBoxLayout *av = new QVBoxLayout(aside);
    av->setContentsMargins(40, 34, 40, 30);
    av->setSpacing(0);

    // stepIndex counts from the first split page, not from the cover.
    const int stepIndex = m_pages->count() - 1;
    av->addWidget(buildStepBar(stepIndex, stepName));
    av->addStretch(1);

    // ThinPro has a drawn illustration per step. A single large glyph says
    // the same thing at a tenth of the weight, and weight is the whole
    // reason this project exists.
    QLabel *art = new QLabel(glyph, aside);
    art->setAlignment(Qt::AlignCenter);
    art->setStyleSheet(QLatin1String("color: #8E97BB; font-size: 130px;"));
    av->addWidget(art);
    av->addStretch(1);

    QLabel *head = new QLabel(headline, aside);
    head->setObjectName(QLatin1String("tpWizardHeadline"));
    head->setWordWrap(true);
    av->addWidget(head);
    av->addSpacing(16);

    QLabel *sub = new QLabel(tr(kReassurance), aside);
    sub->setObjectName(QLatin1String("tpWizardSubtitle"));
    sub->setWordWrap(true);
    av->addWidget(sub);
    av->addSpacing(26);

    QLabel *brand = new QLabel(tr("ThinPro NG"), aside);
    brand->setObjectName(QLatin1String("tpWizardBrand"));
    brand->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    av->addWidget(brand);

    h->addWidget(form, 42);
    h->addWidget(aside, 58);
    return page;
}

QWidget *Wizard::buildKeyboardBody()
{
    QWidget *body = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(10);

    m_keyboardFilter = new QLineEdit(body);
    m_keyboardFilter->setPlaceholderText(tr("Search"));
    m_keyboardFilter->setClearButtonEnabled(true);
    m_keyboardFilter->setMinimumHeight(40);
    v->addWidget(m_keyboardFilter);

    m_keyboard = new QListWidget(body);
    // The form is narrow; long layout names wrap rather than grow a
    // horizontal scroll bar.
    m_keyboard->setWordWrap(true);
    m_keyboard->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v->addWidget(m_keyboard, 1);

    // Every layout xkb knows, by name, the way the Keyboard panel lists
    // them. The language table's own layouts stand in on an image without
    // the xkb rules.
    const QString layouts = HwCheck::runCommand(QLatin1String(
        "sed -n '/^! layout/,/^$/p' /usr/share/X11/xkb/rules/base.lst 2>/dev/null "
        "| awk 'NR>1 && NF { c = $1; $1 = \"\"; sub(/^ +/, \"\"); "
        "printf \"%s\\t%s\\n\", c, $0 }' | sort -f -k2"), 6000);
    const QStringList rows = layouts.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (int i = 0; i < rows.size(); ++i) {
        const QString code = rows.at(i).section(QLatin1Char('\t'), 0, 0).trimmed();
        const QString name = rows.at(i).section(QLatin1Char('\t'), 1).trimmed();
        if (code.isEmpty())
            continue;
        QListWidgetItem *item = new QListWidgetItem(
            name.isEmpty() ? code : name, m_keyboard);
        item->setData(Qt::UserRole, code);
    }
    if (m_keyboard->count() == 0)
        for (int i = 0; kLanguages[i].code; ++i) {
            const QString kb = QString::fromLatin1(kLanguages[i].keyboard);
            if (m_keyboard->findItems(kb, Qt::MatchExactly).isEmpty()) {
                QListWidgetItem *item = new QListWidgetItem(kb, m_keyboard);
                item->setData(Qt::UserRole, kb);
            }
        }

    connect(m_keyboardFilter, SIGNAL(textChanged(QString)),
            this, SLOT(onKeyboardFilter(QString)));
    return body;
}

QWidget *Wizard::buildNetworkBody()
{
    QWidget *body = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(14);

    m_interfaces = new QListWidget(body);
    v->addWidget(m_interfaces, 1);

    m_method = new QComboBox(body);
    m_method->addItem(tr("Automatic (DHCP)"), QLatin1String("dhcp"));
    m_method->addItem(tr("Static address"), QLatin1String("static"));
    m_method->setMinimumHeight(40);
    v->addWidget(m_method);

    m_netStatus = new QLabel(body);
    m_netStatus->setWordWrap(true);
    v->addWidget(m_netStatus);

    onRefreshNetwork();
    return body;
}

QWidget *Wizard::buildDateTimeBody()
{
    QWidget *body = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(18);

    // ThinPro asks for the area and the city separately. The full zone list
    // is nine hundred entries long, and nobody scrolls that.
    m_zoneArea = new QComboBox(body);
    m_zoneCity = new QComboBox(body);
    m_zoneArea->setMinimumHeight(46);
    m_zoneCity->setMinimumHeight(46);

    // zone1970.tab lists the zones people pick from, one per region, with
    // none of the aliases and none of the files beside them. The old
    // `find zoneinfo/posix -type f` found nothing once tzdata made posix a
    // symlink to ".".
    const QString zones = HwCheck::runCommand(QLatin1String(
        "awk '!/^#/ && NF >= 3 {print $3}' /usr/share/zoneinfo/zone1970.tab "
        "2>/dev/null | sort -u"), 8000);
    QStringList all = zones.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    all.prepend(QLatin1String("UTC"));

    QStringList areas;
    for (int i = 0; i < all.size(); ++i) {
        const QString area = all.at(i).section(QLatin1Char('/'), 0, 0);
        if (!area.isEmpty() && !areas.contains(area))
            areas.append(area);
    }
    if (areas.isEmpty())
        areas << QLatin1String("UTC");
    m_zoneArea->addItems(areas);

    connect(m_zoneArea, &QComboBox::currentTextChanged, this,
            [this, all](const QString &area) {
        m_zoneCity->clear();
        for (int i = 0; i < all.size(); ++i)
            if (all.at(i).section(QLatin1Char('/'), 0, 0) == area)
                m_zoneCity->addItem(all.at(i).section(QLatin1Char('/'), 1));
        if (m_zoneCity->count() == 0)
            m_zoneCity->addItem(QString());
    });
    // Start at the zone the client already has, so pressing Next keeps it.
    const QString current = m_reg->value(QLatin1String("root/time/timezone"),
                                         QLatin1String("UTC"));
    const int areaAt = m_zoneArea->findText(current.section(QLatin1Char('/'), 0, 0));
    m_zoneArea->setCurrentIndex(areaAt >= 0 ? areaAt : 0);
    const int cityAt = m_zoneCity->findText(current.section(QLatin1Char('/'), 1));
    if (cityAt >= 0)
        m_zoneCity->setCurrentIndex(cityAt);

    v->addWidget(m_zoneArea);
    v->addWidget(m_zoneCity);
    v->addStretch(1);
    return body;
}

QWidget *Wizard::buildCheckBody()
{
    QWidget *body = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(body);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(12);

    m_checkProgress = new QProgressBar(body);
    m_checkProgress->setRange(0, 1);
    m_checkProgress->setValue(0);
    m_checkProgress->setTextVisible(false);
    v->addWidget(m_checkProgress);

    m_checkList = new QListWidget(body);
    m_checkList->setWordWrap(true);
    m_checkList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    v->addWidget(m_checkList, 1);

    m_checkSummary = new QLabel(body);
    m_checkSummary->setWordWrap(true);
    v->addWidget(m_checkSummary);

    return body;
}

// ---------------------------------------------------------------------------
// The done page
// ---------------------------------------------------------------------------

QWidget *Wizard::buildDonePage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName(QLatin1String("tpWizardCover"));

    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 60, 0, 60);
    v->addStretch(1);

    QLabel *tick = new QLabel(QString::fromUtf8("\xe2\x9c\x93"), page);
    tick->setAlignment(Qt::AlignCenter);
    tick->setStyleSheet(QLatin1String(
        "color: #4E9A3D; font-size: 200px; font-weight: 300;"));
    v->addWidget(tick);

    QLabel *done = new QLabel(tr("All done !"), page);
    done->setObjectName(QLatin1String("tpDoneMessage"));
    v->addWidget(done);
    v->addStretch(1);

    QPushButton *launch = new QPushButton(tr("Launch ThinPro NG"), page);
    launch->setProperty("tpPrimary", true);
    launch->setMinimumSize(380, 52);
    connect(launch, SIGNAL(clicked()), this, SLOT(onNext()));

    QHBoxLayout *gl = new QHBoxLayout;
    gl->addStretch(1);
    gl->addWidget(launch);
    gl->addStretch(1);
    v->addLayout(gl);

    return page;
}

// ---------------------------------------------------------------------------
// Network
// ---------------------------------------------------------------------------

void Wizard::onRefreshNetwork()
{
    if (!m_interfaces)
        return;

    const QString previous = m_interfaces->currentItem()
        ? m_interfaces->currentItem()->data(Qt::UserRole).toString()
        : QString();

    m_interfaces->clear();

    const QString list = HwCheck::runCommand(QLatin1String(
        "ip -o link show 2>/dev/null | awk -F': ' '$2 != \"lo\" { print $2 }'"));
    const QStringList names = list.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    for (int i = 0; i < names.size(); ++i) {
        const QString name = names.at(i).trimmed();
        const QString carrier = HwCheck::runCommand(
            QLatin1String("cat /sys/class/net/") + name
            + QLatin1String("/carrier 2>/dev/null"));
        const QString addr = HwCheck::runCommand(
            QLatin1String("ip -4 -o addr show dev ") + name
            + QLatin1String(" 2>/dev/null | awk '{print $4}' | head -1"));

        QString label = name;
        if (carrier == QLatin1String("1"))
            label += addr.isEmpty() ? tr("  (link up, no address)")
                                    : tr("  (%1)").arg(addr);
        else
            label += tr("  (no link)");

        QListWidgetItem *item = new QListWidgetItem(label, m_interfaces);
        item->setData(Qt::UserRole, name);
        if (name == previous)
            m_interfaces->setCurrentItem(item);
    }

    if (m_interfaces->count() == 0) {
        m_netStatus->setObjectName(QLatin1String("tpStatusFail"));
        m_netStatus->setText(tr("No network interface is present. The client "
                                "will start, but no session can connect."));
    } else {
        if (!m_interfaces->currentItem())
            m_interfaces->setCurrentRow(0);
        m_netStatus->setObjectName(QLatin1String("tpStatusOk"));
        m_netStatus->setText(QString());
    }
    m_netStatus->style()->unpolish(m_netStatus);
    m_netStatus->style()->polish(m_netStatus);
}

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

void Wizard::showPage(int index)
{
    m_pages->setCurrentIndex(index);

    // The split pages carry their own buttons, so the only per-page state
    // to fix up is what the last one's Next button says.
    const int split = index - PageKeyboard;
    if (split >= 0 && split < m_backButtons.size()) {
        // The first split page goes back to the cover, where the language
        // is chosen: that choice is no more final than any other here.
        m_backButtons.at(split)->setEnabled(true);

        if (index == PageNetwork && m_interfaces && m_interfaces->count() == 0)
            m_nextButtons.at(split)->setText(tr("Skip"));
        else if (index == PageCheck)
            m_nextButtons.at(split)->setText(tr("Finish"));
        else
            m_nextButtons.at(split)->setText(tr("Next"));
    }

    if (index == PageCheck && !m_checkDone)
        startCheck();
}

void Wizard::onBack()
{
    const int index = m_pages->currentIndex();
    if (index > 0)
        showPage(index - 1);
}

void Wizard::onNext()
{
    const int index = m_pages->currentIndex();

    if (index == PageCheck) {
        commit();
        showPage(PageDone);
        return;
    }
    if (index == PageDone) {
        close();
        return;
    }
    showPage(index + 1);
}

// ---------------------------------------------------------------------------
// The hardware check
// ---------------------------------------------------------------------------

void Wizard::startCheck()
{
    m_checkDone = true;
    m_checkList->clear();
    m_checkSummary->clear();

    // Finish only greys out for the seconds the probes take; it is never
    // withheld because of what they found.
    m_nextButtons.at(PageCheck - PageKeyboard)->setEnabled(false);

    m_check = new HwCheck(this);
    connect(m_check, SIGNAL(progress(int,int,QString)),
            this, SLOT(onCheckProgress(int,int,QString)));
    connect(m_check, SIGNAL(finished()), this, SLOT(onCheckFinished()));
    m_check->run();
}

void Wizard::onCheckProgress(int done, int total, const QString &label)
{
    m_checkProgress->setRange(0, total);
    m_checkProgress->setValue(done);
    if (!label.isEmpty())
        m_checkSummary->setText(tr("Checking %1...").arg(label));
}

void Wizard::onCheckFinished()
{
    const QVector<CheckResult> results = m_check->results();
    int failures = 0, warnings = 0;

    for (int i = 0; i < results.size(); ++i) {
        const CheckResult &r = results.at(i);

        // ThinPro's own Compatibility Check draws every glyph in blue, pass
        // or fail: the dialog is describing the hardware, not scolding
        // anyone for it. The shape of the glyph carries the verdict.
        QString glyph;
        switch (r.status) {
        case CheckResult::Ok:   glyph = QString::fromUtf8("\xe2\x9c\x93"); break;
        case CheckResult::Warn: glyph = QString::fromUtf8("\xe2\x9a\xa0"); ++warnings; break;
        case CheckResult::Fail: glyph = QString::fromUtf8("\xe2\x9c\x95"); ++failures; break;
        }

        QListWidgetItem *item = new QListWidgetItem(
            QString::fromLatin1("%1  %2 - %3").arg(glyph, r.label, r.detail),
            m_checkList);
        item->setForeground(QColor(0x0F, 0x6F, 0xA8));
    }

    if (failures == 0 && warnings == 0)
        m_checkSummary->setText(tr("Everything checked out."));
    else if (failures == 0)
        m_checkSummary->setText(
            tr("%n item(s) need attention. You can still continue.", "", warnings));
    else
        m_checkSummary->setText(
            tr("%n item(s) failed and %1 need attention. You can still continue; "
               "those features will be unavailable.", "", failures)
            .arg(warnings));

    m_checkProgress->setValue(m_checkProgress->maximum());
    QPushButton *next = m_nextButtons.at(PageCheck - PageKeyboard);
    next->setEnabled(true);
    next->setFocus();
}

// ---------------------------------------------------------------------------
// Saving
// ---------------------------------------------------------------------------

void Wizard::commit()
{
    if (kLanguages[m_language].code)
        m_reg->setValue(QLatin1String("root/locale/language"),
                        QString::fromLatin1(kLanguages[m_language].code));
    m_reg->setValue(QLatin1String("root/i18n/locale"),
                    QString::fromLatin1(kLanguages[m_language].code));
    if (m_keyboard->currentItem())
        m_reg->setValue(QLatin1String("root/keyboard/layout"),
                        m_keyboard->currentItem()->data(Qt::UserRole).toString());

    QString zone = m_zoneArea->currentText();
    if (!m_zoneCity->currentText().isEmpty())
        zone += QLatin1Char('/') + m_zoneCity->currentText();
    m_reg->setValue(QLatin1String("root/time/timezone"), zone);

    if (m_interfaces->currentItem()) {
        const QString iface =
            m_interfaces->currentItem()->data(Qt::UserRole).toString();
        if (!iface.isEmpty())
            m_reg->setValue(QLatin1String("root/network/interface"), iface);
    }
    m_reg->setValue(QLatin1String("root/network/method"),
                    m_method->currentData().toString());

    // Record what the check found, so the System Information panel and any
    // support call can see it later without re-running the probes.
    const QVector<CheckResult> results = m_check ? m_check->results()
                                                 : QVector<CheckResult>();
    for (int i = 0; i < results.size(); ++i) {
        const CheckResult &r = results.at(i);
        const QString node = QLatin1String("root/setup/check/") + r.id;
        m_reg->setValue(node + QLatin1String("/status"),
                        r.status == CheckResult::Ok   ? QLatin1String("ok")
                      : r.status == CheckResult::Warn ? QLatin1String("warn")
                                                      : QLatin1String("fail"));
        m_reg->setValue(node + QLatin1String("/detail"), r.detail);
    }

    m_reg->setValue(QLatin1String("root/setup/completed"), QLatin1String("1"));

    if (!m_reg->save()) {
        // Saying nothing here would mean the wizard runs again on every
        // boot with no explanation.
        m_checkSummary->setObjectName(QLatin1String("tpStatusFail"));
        m_checkSummary->setText(tr("Settings could not be saved: %1")
                                    .arg(m_reg->lastError()));
        return;
    }

    // Apply what was just chosen, rather than waiting for a reboot.
    HwCheck::runCommand(QLatin1String("/etc/tp/panels/keyboard.apply"), 8000);
    HwCheck::runCommand(QLatin1String("/etc/tp/panels/datetime.apply"), 8000);
    HwCheck::runCommand(QLatin1String("/etc/tp/panels/language.apply"), 8000);
}
