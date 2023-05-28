#ifndef LOGDIALOG_H
#define LOGDIALOG_H

#include <QDialog>

class QTextBrowser;
class QPushButton;

class LogDialog : public QDialog
{
    Q_OBJECT
public:
    static void create(QWidget *parent = 0);

    explicit LogDialog(QWidget *parent = 0);
    ~LogDialog();

protected slots:
    void save();

protected:
    static void msgHandler(QtMsgType, const QMessageLogContext& context, const QString& msg);

    static LogDialog* logDialog;
    QTextBrowser *browser;
    QPushButton *clearButton;
    QPushButton *saveButton;
};

#endif // LOGDIALOG_H
