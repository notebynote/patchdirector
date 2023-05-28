#ifndef EDITORDIALOG_H
#define EDITORDIALOG_H

#include <QDialog>

namespace Ui {
class EditorDialog;
}

class EditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit EditorDialog(QWidget *parent = 0);
    ~EditorDialog();

    QString text() const;
    void setText(const QString& text);

    bool wordWrap() const;
    void setWordWrap(bool wrap);

    bool readOnly() const;
    void setReadOnly(bool ro);

private:
    Ui::EditorDialog *ui;
};

#endif // EDITORDIALOG_H
