// The hardware check, ThinPro's Compatibility Check in miniature.
//
// Each probe is a shell command plus a rule for reading its output, so the
// set can grow without touching C++ -- the same approach the settings panels
// take. Nothing here is allowed to block the wizard: a failed probe is
// reported and the user can still continue, because a thin client with no
// sound card is still a perfectly good RDP terminal.

#ifndef TP_HWCHECK_H
#define TP_HWCHECK_H

#include <QObject>
#include <QString>
#include <QVector>

struct CheckResult {
    enum Status { Ok, Warn, Fail };

    QString id;
    QString label;
    QString detail;
    Status  status;

    CheckResult() : status(Ok) {}
};

class HwCheck : public QObject
{
    Q_OBJECT

public:
    explicit HwCheck(QObject *parent = 0);

    // Runs every probe in turn, emitting progress as it goes. Each probe is
    // given a short timeout of its own: a wedged lspci must not hang the
    // first boot of the machine.
    void run();

    QVector<CheckResult> results() const { return m_results; }
    static QString runCommand(const QString &command, int timeoutMs = 5000);

signals:
    void progress(int done, int total, const QString &label);
    void finished();

private:
    void add(const QString &id, const QString &label,
             const QString &detail, CheckResult::Status status);

    void checkCpu();
    void checkMemory();
    void checkGraphics();
    void checkNetwork();
    void checkSound();
    void checkStorage();
    void checkUsb();

    QVector<CheckResult> m_results;
};

#endif
