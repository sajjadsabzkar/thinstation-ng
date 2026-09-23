#include "wizard.h"
#include "registry.h"

#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

static const int PageLanguage = 0;
static const int PageNetwork  = 1;
static const int PageCheck    = 2;
static const int PageCount    = 3;

// The languages the image can actually present. ThinPro carries a much
// longer list; ours is bounded by the locale package in the build, so
// offering more would be a promise the image cannot keep.
struct Language {
    const char *code;
    const char *name;
    const char *keyboard;
};
static const Language kLanguages[] = {
    { "en_US.UTF-8", "English (United States)", "us" },
    { "en_GB.UTF-8", "English (United Kingdom)", "gb" },
    { "de_DE.UTF-8", "Deutsch",                 "de" },
    { "fr_FR.UTF-8", "Fran\xc3\xa7" "ais",      "fr" },
    { "es_ES.UTF-8", "Espa\xc3\xb1ol",          "es" },
    { "it_IT.UTF-8", "Italiano",                "it" },
    { "pt_BR.UTF-8", "Portugu\xc3\xaas (Brasil)", "br" },
    { "ru_RU.UTF-8", "\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9", "ru" },
    { "fa_IR.UTF-8", "\xd9\x81\xd8\xa7\xd8\xb1\xd8\xb3\xdb\x8c", "ir" },
    { "tr_TR.UTF-8", "T\xc3\xbcrk\xc3\xa7" "e", "tr" },
    { "ar_SA.UTF-8", "\xd8\xa7\xd9\x84\xd8\xb9\xd8\xb1\xd8\xa8\xd9\x8a\xd8\xa9", "ara" },
    { "zh_CN.UTF-8", "\xe4\xb8\xad\xe6\x96\x87 (\xe7\xae\x80\xe4\xbd\x93)", "cn" },
    { 0, 0, 0 }
};

Wizard::Wizard(Registry *reg, QWidget *parent)
    : QWidget(parent), m_reg(reg), m_check(0), m_checkDone(false)
{
    setWindowTitle(tr("Initial Setup"));

    QVBoxLayout *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // --- header ---------------------------------------------------------
    QFrame *header = new QFrame(this);
    header->setObjectName(QLatin1String("tpHeader"));
    QHBoxLayout *hl = new QHBoxLayout(header);
    hl->setContentsMargins(20, 0, 20, 0);

    m_title = new QLabel(header);
    m_title->setObjectName(QLatin1String("tpTitle"));
    m_step = new QLabel(header);

    hl->addWidget(m_title);
    hl->addStretch(1);
    hl->addWidget(m_step);
    outer->addWidget(header);

    // --- pages ----------------------------------------------------------
    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildLanguagePage());
    m_pages->addWidget(buildNetworkPage());
    m_pages->addWidget(buildCheckPage());
    outer->addWidget(m_pages, 1);

    // --- footer ---------------------------------------------------------
    QFrame *footer = new QFrame(this);
    footer->setObjectName(QLatin1String("tpFooter"));
    QHBoxLayout *fl = new QHBoxLayout(footer);
    fl->setContentsMargins(20, 6, 20, 6);

    m_back = new QPushButton(tr("Back"), footer);
    m_next = new QPushButton(tr("Next"), footer);
    m_next->setDefault(true);

    fl->addStretch(1);
    fl->addWidget(m_back);
    fl->addWidget(m_next);
    outer->addWidget(footer);

    connect(m_back, SIGNAL(clicked()), this, SLOT(onBack()));
    connect(m_next, SIGNAL(clicked()), this, SLOT(onNext()));

    showPage(PageLanguage);
    resize(720, 480);
}

QWidget *Wizard::buildLanguagePage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(40, 30, 40, 30);

    QLabel *intro = new QLabel(
        tr("Choose the language and keyboard layout for this client."), page);
    intro->setWordWrap(true);
    v->addWidget(intro);
    v->addSpacing(16);

    QFormLayout *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(14);

    m_language = new QComboBox(page);
    for (int i = 0; kLanguages[i].code; ++i)
        m_language->addItem(QString::fromUtf8(kLanguages[i].name),
                            QString::fromLatin1(kLanguages[i].code));

    m_keyboard = new QComboBox(page);
    m_keyboard->setEditable(true);
    for (int i = 0; kLanguages[i].code; ++i) {
        const QString kb = QString::fromLatin1(kLanguages[i].keyboard);
        if (m_keyboard->findText(kb) < 0)
            m_keyboard->addItem(kb);
    }

    m_timezone = new QComboBox(page);
    m_timezone->setEditable(true);
    const QString zones = HwCheck::runCommand(QLatin1String(
        "find /usr/share/zoneinfo/posix -type f 2>/dev/null "
        "| sed 's|/usr/share/zoneinfo/posix/||' | sort"), 8000);
    if (!zones.isEmpty())
        m_timezone->addItems(zones.split(QLatin1Char('\n'), Qt::SkipEmptyParts));
    else
        m_timezone->addItem(QLatin1String("UTC"));

    // The keyboard follows the language unless the user says otherwise.
    connect(m_language, SIGNAL(currentIndexChanged(int)),
            this, SLOT(onRefreshNetwork()));   // reused: just refreshes state

    form->addRow(tr("Language:"), m_language);
    form->addRow(tr("Keyboard:"), m_keyboard);
    form->addRow(tr("Time zone:"), m_timezone);
    v->addLayout(form);
    v->addStretch(1);

    return page;
}

QWidget *Wizard::buildNetworkPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(40, 30, 40, 30);

    QLabel *intro = new QLabel(
        tr("Choose which network interface this client should use."), page);
    intro->setWordWrap(true);
    v->addWidget(intro);
    v->addSpacing(16);

    QFormLayout *form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    form->setHorizontalSpacing(20);
    form->setVerticalSpacing(14);

    m_interface = new QComboBox(page);
    m_method = new QComboBox(page);
    m_method->addItem(tr("Automatic (DHCP)"), QLatin1String("dhcp"));
    m_method->addItem(tr("Static address"), QLatin1String("static"));

    form->addRow(tr("Interface:"), m_interface);
    form->addRow(tr("Addressing:"), m_method);
    v->addLayout(form);

    v->addSpacing(12);
    m_netStatus = new QLabel(page);
    m_netStatus->setWordWrap(true);
    v->addWidget(m_netStatus);

    QPushButton *refresh = new QPushButton(tr("Refresh"), page);
    connect(refresh, SIGNAL(clicked()), this, SLOT(onRefreshNetwork()));
    QHBoxLayout *rl = new QHBoxLayout;
    rl->addWidget(refresh);
    rl->addStretch(1);
    v->addLayout(rl);

    v->addStretch(1);
    onRefreshNetwork();
    return page;
}

QWidget *Wizard::buildCheckPage()
{
    QWidget *page = new QWidget(this);
    QVBoxLayout *v = new QVBoxLayout(page);
    v->setContentsMargins(40, 30, 40, 30);

    QLabel *intro = new QLabel(
        tr("This is what the client found. Anything marked as a problem "
           "will simply be unavailable -- you can still continue."), page);
    intro->setWordWrap(true);
    v->addWidget(intro);
    v->addSpacing(12);

    m_checkProgress = new QProgressBar(page);
    m_checkProgress->setRange(0, 1);
    m_checkProgress->setValue(0);
    m_checkProgress->setTextVisible(false);
    v->addWidget(m_checkProgress);

    m_checkList = new QListWidget(page);
    v->addWidget(m_checkList, 1);

    m_checkSummary = new QLabel(page);
    m_checkSummary->setWordWrap(true);
    v->addWidget(m_checkSummary);

    return page;
}

void Wizard::onRefreshNetwork()
{
    if (!m_interface)
        return;

    const QString current = m_interface->currentText();
    m_interface->clear();

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

        m_interface->addItem(label, name);
    }

    if (m_interface->count() == 0) {
        m_interface->addItem(tr("No interface found"), QString());
        m_netStatus->setObjectName(QLatin1String("tpStatusFail"));
        m_netStatus->setText(tr("No network interface is present. The client "
                                "will start, but no session can connect."));
    } else {
        const int idx = m_interface->findText(current);
        if (idx >= 0)
            m_interface->setCurrentIndex(idx);
        m_netStatus->setObjectName(QLatin1String("tpStatusOk"));
        m_netStatus->setText(QString());
    }
    m_netStatus->style()->unpolish(m_netStatus);
    m_netStatus->style()->polish(m_netStatus);
}

void Wizard::showPage(int index)
{
    m_pages->setCurrentIndex(index);

    static const char *titles[] = {
        QT_TR_NOOP("Language"),
        QT_TR_NOOP("Network"),
        QT_TR_NOOP("Hardware check"),
    };
    m_title->setText(tr(titles[index]));
    m_step->setText(tr("Step %1 of %2").arg(index + 1).arg(PageCount));

    m_back->setEnabled(index > 0);
    m_next->setText(index == PageCount - 1 ? tr("Finish") : tr("Next"));

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
    if (index < PageCount - 1) {
        showPage(index + 1);
        return;
    }
    commit();
    close();
}

void Wizard::startCheck()
{
    m_checkDone = true;
    m_checkList->clear();
    m_checkSummary->clear();

    // The check must not strand the user on a page with no way forward, so
    // Finish only greys out for the seconds the probes take.
    m_next->setEnabled(false);

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

        QString mark;
        QColor colour;
        switch (r.status) {
        case CheckResult::Ok:
            mark = tr("OK");     colour = QColor(0x2E, 0x7D, 0x32); break;
        case CheckResult::Warn:
            mark = tr("Check");  colour = QColor(0xB2, 0x65, 0x00); ++warnings; break;
        case CheckResult::Fail:
            mark = tr("Failed"); colour = QColor(0xB0, 0x00, 0x20); ++failures; break;
        }

        QListWidgetItem *item = new QListWidgetItem(
            tr("%1    %2 - %3").arg(mark, -7).arg(r.label, r.detail),
            m_checkList);
        item->setForeground(colour);
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
    m_next->setEnabled(true);
    m_next->setFocus();
}

void Wizard::commit()
{
    const QString locale = m_language->currentData().toString();
    m_reg->setValue(QLatin1String("root/locale/language"), locale);
    m_reg->setValue(QLatin1String("root/keyboard/layout"),
                    m_keyboard->currentText());
    m_reg->setValue(QLatin1String("root/time/timezone"),
                    m_timezone->currentText());

    const QString iface = m_interface->currentData().toString();
    if (!iface.isEmpty())
        m_reg->setValue(QLatin1String("root/network/interface"), iface);
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
}
