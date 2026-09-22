#include "mainwindow.h"
#include "editdialog.h"
#include "registry.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QVBoxLayout>

MainWindow::MainWindow(Registry *reg, Model *model, bool kioskMode,
                       QWidget *parent)
    : QWidget(parent), m_reg(reg), m_model(model), m_kiosk(kioskMode)
{
    // In kiosk mode the connection list is all the user gets, so editing is
    // off regardless of the user-rights key.
    m_mayEdit = !m_kiosk
             && m_reg->boolValue(QLatin1String("root/users/user/switchAdmin"), true);

    setWindowTitle(tr("Connection Manager"));

    QVBoxLayout *outer = new QVBoxLayout(this);

    QLabel *heading = new QLabel(tr("Choose a connection"), this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(headingFont.pointSize() + 3);
    heading->setFont(headingFont);
    outer->addWidget(heading);

    m_list = new QListWidget(this);
    m_list->setAlternatingRowColors(true);
    m_list->setIconSize(QSize(32, 32));
    outer->addWidget(m_list, 1);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    outer->addWidget(m_status);

    QHBoxLayout *buttons = new QHBoxLayout;
    m_connectBtn = new QPushButton(tr("Connect"), this);
    m_connectBtn->setDefault(true);
    buttons->addWidget(m_connectBtn);

    m_addBtn    = new QPushButton(tr("Add"), this);
    m_editBtn   = new QPushButton(tr("Edit"), this);
    m_deleteBtn = new QPushButton(tr("Delete"), this);
    buttons->addWidget(m_addBtn);
    buttons->addWidget(m_editBtn);
    buttons->addWidget(m_deleteBtn);

    buttons->addStretch(1);

    m_quitBtn = new QPushButton(tr("Close"), this);
    buttons->addWidget(m_quitBtn);

    outer->addLayout(buttons);

    m_addBtn->setVisible(m_mayEdit);
    m_editBtn->setVisible(m_mayEdit);
    m_deleteBtn->setVisible(m_mayEdit);
    // Nothing should be able to close the chooser in kiosk mode.
    m_quitBtn->setVisible(!m_kiosk);

    connect(m_connectBtn, SIGNAL(clicked()), this, SLOT(onConnect()));
    connect(m_addBtn,     SIGNAL(clicked()), this, SLOT(onAdd()));
    connect(m_editBtn,    SIGNAL(clicked()), this, SLOT(onEdit()));
    connect(m_deleteBtn,  SIGNAL(clicked()), this, SLOT(onDelete()));
    connect(m_quitBtn,    SIGNAL(clicked()), qApp, SLOT(quit()));
    connect(m_list, SIGNAL(itemSelectionChanged()),
            this, SLOT(onSelectionChanged()));
    connect(m_list, SIGNAL(itemActivated(QListWidgetItem *)),
            this, SLOT(onItemActivated(QListWidgetItem *)));

    if (m_kiosk) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint
                     | Qt::WindowStaysOnTopHint);
        showFullScreen();
    } else {
        resize(560, 420);
    }

    refresh();
}

void MainWindow::refresh()
{
    const QString keep = selectedUuid();

    m_model->reload();
    m_list->clear();

    const QVector<Connection> conns = m_model->connections();
    for (int i = 0; i < conns.size(); ++i) {
        const Connection &c = conns.at(i);
        const ConnectionType t = m_model->type(c.typeId);

        QListWidgetItem *item = new QListWidgetItem(m_list);
        item->setText(QString::fromLatin1("%1\n%2").arg(c.label, t.label));
        item->setData(Qt::UserRole, c.uuid);
        if (!t.icon.isEmpty())
            item->setIcon(QIcon::fromTheme(t.icon));
        if (c.uuid == keep)
            m_list->setCurrentItem(item);
    }

    if (conns.isEmpty()) {
        m_status->setText(m_mayEdit
            ? tr("No connections yet. Use Add to create one.")
            : tr("No connections have been configured."));
    } else {
        m_status->clear();
        if (!m_list->currentItem())
            m_list->setCurrentRow(0);
    }

    onSelectionChanged();
}

QString MainWindow::selectedUuid() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void MainWindow::onSelectionChanged()
{
    const bool have = !selectedUuid().isEmpty();
    m_connectBtn->setEnabled(have);
    m_editBtn->setEnabled(have && m_mayEdit);
    m_deleteBtn->setEnabled(have && m_mayEdit);
}

void MainWindow::onItemActivated(QListWidgetItem *)
{
    onConnect();
}

void MainWindow::saveOrWarn()
{
    if (!m_reg->save()) {
        QMessageBox::warning(this, tr("Connection Manager"),
            tr("The connection could not be saved.\n\n%1").arg(m_reg->lastError()));
    }
}

void MainWindow::onConnect()
{
    const QString uuid = selectedUuid();
    if (uuid.isEmpty())
        return;

    // tp-launch resolves the connection out of the registry and hands it to
    // ThinStation's pkg dispatcher. Detached, so the chooser stays alive and
    // the session survives if the chooser is later closed.
    if (!QProcess::startDetached(QLatin1String("tp-launch"), QStringList() << uuid)) {
        QMessageBox::critical(this, tr("Connection Manager"),
            tr("Could not start the connection.\n\n"
               "tp-launch was not found on PATH."));
        return;
    }

    m_status->setText(tr("Starting %1 ...").arg(m_model->connection(uuid).label));
}

void MainWindow::onAdd()
{
    const QVector<ConnectionType> types = m_model->types();
    if (types.isEmpty()) {
        QMessageBox::warning(this, tr("Connection Manager"),
            tr("No connection types are available in this image."));
        return;
    }

    QStringList labels;
    for (int i = 0; i < types.size(); ++i)
        labels << types.at(i).label;

    bool ok = false;
    const QString chosen = QInputDialog::getItem(this, tr("New Connection"),
        tr("Connection type:"), labels, 0, false, &ok);
    if (!ok)
        return;

    const int idx = labels.indexOf(chosen);
    if (idx < 0)
        return;

    const QString uuid = m_model->createConnection(types.at(idx).id);
    if (uuid.isEmpty())
        return;

    m_model->reload();
    EditDialog dlg(m_model, m_model->connection(uuid), this);
    if (dlg.exec() == QDialog::Accepted) {
        saveOrWarn();
    } else {
        // Nothing was committed to disk yet; drop the half-made connection
        // so a cancelled Add leaves no trace.
        m_model->removeConnection(uuid);
    }
    refresh();
}

void MainWindow::onEdit()
{
    const QString uuid = selectedUuid();
    if (uuid.isEmpty())
        return;

    EditDialog dlg(m_model, m_model->connection(uuid), this);
    if (dlg.exec() == QDialog::Accepted) {
        saveOrWarn();
        refresh();
    }
}

void MainWindow::onDelete()
{
    const QString uuid = selectedUuid();
    if (uuid.isEmpty())
        return;

    const Connection c = m_model->connection(uuid);
    const int answer = QMessageBox::question(this, tr("Deleting connection"),
        tr("Delete the connection \"%1\"?").arg(c.label),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    m_model->removeConnection(uuid);
    saveOrWarn();
    refresh();
}
