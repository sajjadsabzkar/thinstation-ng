#include "hwcheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QTemporaryFile>

QString HwCheck::runCommand(const QString &command, int timeoutMs)
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

HwCheck::HwCheck(QObject *parent) : QObject(parent)
{
}

void HwCheck::add(const QString &id, const QString &label,
                  const QString &detail, CheckResult::Status status)
{
    CheckResult r;
    r.id = id;
    r.label = label;
    r.detail = detail;
    r.status = status;
    m_results.append(r);
}

void HwCheck::run()
{
    m_results.clear();

    struct Probe {
        void (HwCheck::*fn)();
        const char *label;
    };
    static const Probe probes[] = {
        { &HwCheck::checkCpu,      QT_TR_NOOP("Processor") },
        { &HwCheck::checkMemory,   QT_TR_NOOP("Memory") },
        { &HwCheck::checkGraphics, QT_TR_NOOP("Graphics") },
        { &HwCheck::checkNetwork,  QT_TR_NOOP("Network") },
        { &HwCheck::checkSound,    QT_TR_NOOP("Sound") },
        { &HwCheck::checkStorage,  QT_TR_NOOP("Storage") },
        { &HwCheck::checkUsb,      QT_TR_NOOP("USB") },
    };
    const int total = int(sizeof(probes) / sizeof(probes[0]));

    for (int i = 0; i < total; ++i) {
        emit progress(i, total, tr(probes[i].label));
        QCoreApplication::processEvents();
        (this->*(probes[i].fn))();
    }

    emit progress(total, total, QString());
    emit finished();
}

void HwCheck::checkCpu()
{
    const QString model = runCommand(QLatin1String(
        "sed -n 's/^model name[ \\t]*: //p' /proc/cpuinfo | head -1"));
    const QString cores = runCommand(QLatin1String("nproc 2>/dev/null"));
    const QString flags = runCommand(QLatin1String(
        "sed -n 's/^flags[ \\t]*: //p' /proc/cpuinfo | head -1"));

    QString detail = model.isEmpty() ? tr("unknown") : model;
    if (!cores.isEmpty())
        detail += tr(", %1 core(s)", "", cores.toInt());

    // The image is built for x86-64-baseline on purpose, so anything 64-bit
    // will run it. Say so rather than leaving the user guessing.
    if (!flags.contains(QLatin1String("lm"))) {
        add(QLatin1String("cpu"), tr("Processor"),
            detail + tr(" - not 64-bit, this image cannot run here"),
            CheckResult::Fail);
        return;
    }

    add(QLatin1String("cpu"), tr("Processor"), detail, CheckResult::Ok);
}

void HwCheck::checkMemory()
{
    const QString total = runCommand(QLatin1String(
        "awk '/^MemTotal:/ { print int($2 / 1024) }' /proc/meminfo"));
    const int mb = total.toInt();

    if (mb <= 0) {
        add(QLatin1String("memory"), tr("Memory"),
            tr("could not be read"), CheckResult::Warn);
        return;
    }

    const QString detail = tr("%1 MB").arg(mb);

    // 1 GB is what this image is designed for; below 512 MB the squashfs
    // page cache alone will thrash.
    if (mb < 512)
        add(QLatin1String("memory"), tr("Memory"),
            detail + tr(" - below the 512 MB minimum"), CheckResult::Fail);
    else if (mb < 900)
        add(QLatin1String("memory"), tr("Memory"),
            detail + tr(" - tight, a browser session may struggle"),
            CheckResult::Warn);
    else
        add(QLatin1String("memory"), tr("Memory"), detail, CheckResult::Ok);
}

void HwCheck::checkGraphics()
{
    const QString card = runCommand(QLatin1String(
        "lspci 2>/dev/null | sed -n 's/.*VGA compatible controller: //p' | head -1"));
    const QString mode = runCommand(QLatin1String(
        "xrandr --query 2>/dev/null | sed -n 's/.*current \\([0-9]* x [0-9]*\\).*/\\1/p' | head -1"));
    const QString driver = runCommand(QLatin1String(
        "sed -n 's/.*Loading .*modules\\/drivers\\/\\([a-z]*\\)_drv.so.*/\\1/p' "
        "/var/log/Xorg.0.log 2>/dev/null | tail -1"));

    QString detail;
    if (!card.isEmpty())
        detail = card;
    if (!mode.isEmpty())
        detail += detail.isEmpty() ? mode : tr(" at %1").arg(mode);
    if (detail.isEmpty())
        detail = tr("unknown");

    // vesa and fbdev mean no acceleration: everything still works, but a
    // full-screen session will be slow, and that is worth saying up front.
    if (driver == QLatin1String("vesa") || driver == QLatin1String("fbdev"))
        add(QLatin1String("graphics"), tr("Graphics"),
            detail + tr(" - unaccelerated %1 driver").arg(driver),
            CheckResult::Warn);
    else if (mode.isEmpty())
        add(QLatin1String("graphics"), tr("Graphics"),
            detail + tr(" - no mode detected"), CheckResult::Warn);
    else
        add(QLatin1String("graphics"), tr("Graphics"), detail, CheckResult::Ok);
}

void HwCheck::checkNetwork()
{
    const QString up = runCommand(QLatin1String(
        "ip -o link show up 2>/dev/null | awk -F': ' '$2 != \"lo\" { print $2 }'"));
    const QString addr = runCommand(QLatin1String(
        "ip -4 -o addr show scope global 2>/dev/null | awk '{ print $2\" \"$4 }'"));

    if (up.isEmpty()) {
        add(QLatin1String("network"), tr("Network"),
            tr("no interface is up"), CheckResult::Fail);
        return;
    }
    if (addr.isEmpty()) {
        add(QLatin1String("network"), tr("Network"),
            tr("%1 has a link but no address")
                .arg(up.split(QLatin1Char('\n')).first()),
            CheckResult::Warn);
        return;
    }
    add(QLatin1String("network"), tr("Network"),
        addr.split(QLatin1Char('\n')).first(), CheckResult::Ok);
}

void HwCheck::checkSound()
{
    const QString cards = runCommand(QLatin1String(
        "aplay -l 2>/dev/null | sed -n 's/^card [0-9]*: [^[]*\\[\\([^]]*\\)\\].*/\\1/p' | head -1"));

    if (cards.isEmpty())
        add(QLatin1String("sound"), tr("Sound"),
            tr("no playback device - remote audio will not work"),
            CheckResult::Warn);
    else
        add(QLatin1String("sound"), tr("Sound"), cards, CheckResult::Ok);
}

void HwCheck::checkStorage()
{
    // What matters is not disk size but whether settings will survive: the
    // registry has to be writable or nothing the user changes is kept.
    const QByteArray env = qgetenv("TP_REGISTRY");
    const QString path = env.isEmpty()
        ? QLatin1String("/var/lib/tp/registry.conf")
        : QString::fromLocal8Bit(env);
    const QString dir = QFileInfo(path).absolutePath();

    QTemporaryFile probe(dir + QLatin1String("/tp-check-XXXXXX"));
    if (probe.open()) {
        probe.write("ok");
        probe.close();
        add(QLatin1String("storage"), tr("Settings storage"),
            tr("writable at %1").arg(dir), CheckResult::Ok);
    } else {
        add(QLatin1String("storage"), tr("Settings storage"),
            tr("%1 is not writable - changes will not be kept").arg(dir),
            CheckResult::Fail);
    }
}

void HwCheck::checkUsb()
{
    const QString count = runCommand(QLatin1String(
        "ls /sys/bus/usb/devices 2>/dev/null | grep -c '^[0-9]*-[0-9]'"));
    const int n = count.toInt();

    if (n <= 0)
        add(QLatin1String("usb"), tr("USB"),
            tr("no devices detected"), CheckResult::Warn);
    else
        add(QLatin1String("usb"), tr("USB"),
            tr("%n device(s) attached", "", n), CheckResult::Ok);
}
