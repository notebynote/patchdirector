#include "logdialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QPushButton>
#include <QFileDialog>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>

void LogDialog::create(QWidget *parent)
{
    if (logDialog) {
        logDialog->show();
        logDialog->activateWindow();
        logDialog->raise();
        return;
    }
    logDialog = new LogDialog(parent);
    logDialog->show();
    qInstallMessageHandler(msgHandler);
}

LogDialog::LogDialog(QWidget *parent) : QDialog(parent)
{
    QVBoxLayout *layout = new QVBoxLayout;
    setLayout(layout);

    browser = new QTextBrowser(this);
    layout->addWidget(browser);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    layout->addLayout(buttonLayout);

    buttonLayout->addStretch(10);

    clearButton = new QPushButton(this);
    clearButton->setText("Clear");
    buttonLayout->addWidget(clearButton);
    connect(clearButton, SIGNAL (clicked()), browser, SLOT (clear()));

    saveButton = new QPushButton(this);
    saveButton->setText("Save");
    buttonLayout->addWidget(saveButton);
    connect(saveButton, SIGNAL (clicked()), this, SLOT (save()));

    resize(600, 400);

    setWindowTitle("Patchdirector Log");
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
}

LogDialog::~LogDialog()
{
    logDialog = Q_NULLPTR;
    qInstallMessageHandler(0);
}

void LogDialog::save()
{
    QString saveFileName = QFileDialog::getSaveFileName(
        this, "Save Log", QDir::home().absoluteFilePath("log.txt"),
        "Text Files (*.txt);;All Files (*.*)"
    );
    if(saveFileName.isEmpty()) return;
    QFile file(saveFileName);
    if(!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "Error", "The log could not be saved!");
        return;
    }
    QTextStream stream(&file);
    stream << browser->toPlainText();
    file.close();
}

void LogDialog::msgHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg)
{
    if (!logDialog) return;
    QString s = msg;
    switch (type) {
        case QtWarningMsg:  s.prepend("Warning: ");  break;
        case QtCriticalMsg: s.prepend("Critical: "); break;
        case QtFatalMsg:    s.prepend("Fatal: ");    break;
    }
    logDialog->browser->append(s);
}

LogDialog* LogDialog::logDialog = Q_NULLPTR;
