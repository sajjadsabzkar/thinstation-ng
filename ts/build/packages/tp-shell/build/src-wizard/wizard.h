// The first-boot setup wizard, drawn the way ThinPro's Initial Setup Wizard
// is drawn.
//
// The shape comes from screenshots of the real product, not from its code:
//
//   * a full-width white cover page first, with the product name, a hero
//     band, the languages as a row of flat blue links, and one wide button
//   * then a run of split pages: white on the left carrying the question in
//     blue and the controls under it, slate blue on the right carrying a
//     step pill, an illustration, a headline and a reassuring subtitle
//   * a full-width white page at the end: a green tick, "All done !", and a
//     button that launches the session
//
// The subtitle on every split page is the same sentence -- "You can
// customize it later, don't worry." -- and it is doing real work: it is why
// someone setting up twenty clients does not stop to think about each one.
//
// The Finish button is enabled whatever the hardware check says. A client
// that fails the sound probe is still a usable terminal, and a wizard that
// refuses to let go of a machine is worse than one that warns.
//
// It runs once. Finishing writes root/setup/completed, which is what
// tp-session looks at before deciding to start it.

#ifndef TP_WIZARD_H
#define TP_WIZARD_H

#include <QWidget>

#include "hwcheck.h"

class Registry;
class QComboBox;
class QFrame;
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
    void onLanguageClicked();

private:
    // The full-width pages.
    QWidget *buildCoverPage();
    QWidget *buildDonePage();

    // A split page: white form on the left, slate aside on the right. The
    // caller hands in the question and the body; everything else about the
    // two halves is the same on every page, which is why it lives here.
    QWidget *buildSplitPage(const QString &question,
                            QWidget *body,
                            const QString &stepName,
                            const QString &headline,
                            const QString &glyph,
                            QPushButton **backOut,
                            QPushButton **nextOut);

    QWidget *buildKeyboardBody();
    QWidget *buildNetworkBody();
    QWidget *buildDateTimeBody();
    QWidget *buildCheckBody();

    QFrame  *buildStepBar(int stepIndex, const QString &stepName);

    void showPage(int index);
    void startCheck();
    void commit();
    void selectLanguage(int index);

    Registry       *m_reg;
    QStackedWidget *m_pages;

    // One pair of navigation buttons per split page: they live inside the
    // page's own white half, so there is no single shared footer.
    QVector<QPushButton *> m_backButtons;
    QVector<QPushButton *> m_nextButtons;

    QVector<QPushButton *> m_languageButtons;
    int             m_language;

    QComboBox      *m_keyboard;
    QComboBox      *m_zoneArea;
    QComboBox      *m_zoneCity;

    QListWidget    *m_interfaces;
    QComboBox      *m_method;
    QLabel         *m_netStatus;

    QListWidget    *m_checkList;
    QProgressBar   *m_checkProgress;
    QLabel         *m_checkSummary;
    HwCheck        *m_check;
    bool            m_checkDone;
};

#endif
