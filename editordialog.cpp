#include "editordialog.h"
#include "ui_editordialog.h"

EditorDialog::EditorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditorDialog)
{
    ui->setupUi(this);
}

EditorDialog::~EditorDialog()
{
    delete ui;
}

QString EditorDialog::text() const
{
    return ui->edit->toPlainText();
}

void EditorDialog::setText(const QString &text)
{
    ui->edit->setPlainText(text);
}

bool EditorDialog::wordWrap() const
{
    return ui->edit->wordWrapMode() != QTextOption::NoWrap;
}

void EditorDialog::setWordWrap(bool wrap)
{
    ui->edit->setWordWrapMode(wrap ? QTextOption::WordWrap : QTextOption::NoWrap);
}

bool EditorDialog::readOnly() const
{
    return ui->edit->isReadOnly();
}

void EditorDialog::setReadOnly(bool ro)
{
    ui->edit->setReadOnly(ro);
    if (ro) {
        ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok);
    } else {
        ui->buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    }
}
