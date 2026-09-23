// The first-boot setup wizard, modelled on ThinPro's Initial Setup Wizard.
//
// Three pages: language, network, and a hardware check. The check reports
// what works and what does not, and the Finish button is enabled either way
// -- a client that fails the sound probe is still a usable terminal, and a
// wizard that refuses to let go of a machine is worse than one that warns.
//
// It runs once. Finishing writes root/setup/completed, which is what
// tp-session looks at before deciding to start it.

#ifndef TP_WIZARD_H
#define TP_WIZARD_H

#include <QWidget>

#include "hwcheck.h"

class Registry;
class QComboBox;
class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QStackedWidget;

class Wizard : public QWidget
{
    Q_OBJECT

public:
    explicit Wizard(Registry *reg, QWidget *parent = 0);

private slots:
    void onBack();
    void onNext();
    void onCheckProgress(int done, int total, const QString &label);
    void onCheckFinished();
    void onRefreshNetwork();

private:
    QWidget *buildLanguagePage();
    QWidget *buildNetworkPage();
    QWidget *buildCheckPage();

    void showPage(int index);
    void startCheck();
    void commit();

    Registry       *m_reg;
    QStackedWidget *m_pages;
    QLabel         *m_title;
    QLabel         *m_step;
    QPushButton    *m_back;
    QPushButton    *m_next;

    QComboBox      *m_language;
    QComboBox      *m_keyboard;
    QComboBox      *m_timezone;

    QComboBox      *m_interface;
    QComboBox      *m_method;
    QLabel         *m_netStatus;

    QListWidget    *m_checkList;
    QProgressBar   *m_checkProgress;
    QLabel         *m_checkSummary;
    HwCheck        *m_check;
    bool            m_checkDone;
};

#endif
