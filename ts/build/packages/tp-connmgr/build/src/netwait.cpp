#include "netwait.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QLabel>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

static QString firstLine(const QString &command, int timeoutMs = 3000)
{
    QProcess p;
    p.start(QLatin1String("/bin/sh"),
            QStringList() << QLatin1String("-c") << command);
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(500);
        return QString();
    }
    return QString::fromLocal8Bit(p.readAllStandardOutput()).trimmed();
}

bool NetWait::networkReady(QString *detail)
{
    // Carrier first: it is a single file read per interface and it is what
    // distinguishes "no cable" from "no lease", which is the distinction
    // worth showing the user.
    bool anyCarrier = false;
    const QStringList ifaces = QDir(QLatin1String("/sys/class/net"))
        .entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (int i = 0; i < ifaces.size(); ++i) {
        if (ifaces.at(i) == QLatin1String("lo"))
            continue;
        QFile f(QLatin1String("/sys/class/net/") + ifaces.at(i)
                + QLatin1String("/carrier"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;                       // down interfaces refuse the read
        if (f.readAll().trimmed() == "1") {
            anyCarrier = true;
            break;
        }
    }

    const QString addr = firstLine(QLatin1String(
        "ip -4 -o addr show scope global 2>/dev/null | awk '{print $2\" \"$4}' | head -1"));

    if (detail) {
        if (!addr.isEmpty())
            *detail = addr;
        else if (anyCarrier)
            *detail = tr("Link is up, waiting for an address");
        else
            *detail = tr("No link detected");
    }

    return !addr.isEmpty();
}

NetWait::NetWait(QWidget *parent)
    : QDialog(parent), m_elapsed(0), m_timeout(0)
{
    setWindowTitle(tr("Connection Manager"));
    setModal(true);

    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(28, 24, 28, 20);
    v->setSpacing(14);

    m_message = new QLabel(tr("Waiting for networking..."), this);
    QFont f = m_message->font();
    f.setPointSize(f.pointSize() + 2);
    m_message->setFont(f);
    v->addWidget(m_message);

    m_bar = new QProgressBar(this);
    m_bar->setRange(0, 0);               // indeterminate: no honest estimate
    m_bar->setTextVisible(false);
    v->addWidget(m_bar);

    m_detail = new QLabel(this);
    m_detail->setWordWrap(true);
    v->addWidget(m_detail);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    v->addWidget(buttons);

    m_timer = new QTimer(this);
    connect(m_timer, SIGNAL(timeout()), this, SLOT(poll()));
    m_timer->start(1000);

    setMinimumWidth(380);
}

void NetWait::poll()
{
    ++m_elapsed;

    QString detail;
    if (networkReady(&detail)) {
        m_timer->stop();
        accept();
        return;
    }

    m_detail->setText(detail);

    if (m_timeout > 0 && m_elapsed >= m_timeout) {
        m_timer->stop();
        reject();
    }
}

bool NetWait::waitFor(QWidget *parent, int timeoutSeconds)
{
    if (networkReady())
        return true;

    NetWait dialog(parent);
    dialog.m_timeout = timeoutSeconds;

    QString detail;
    networkReady(&detail);
    dialog.m_detail->setText(detail);

    return dialog.exec() == QDialog::Accepted;
}
