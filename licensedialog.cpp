#include "licensedialog.h"
#include "ui_licensedialog.h"

#include <QPushButton>

LicenseDialog::LicenseDialog(QWidget *parent, bool accept) :
    QDialog(parent),
    ui(new Ui::LicenseDialog)
{
    ui->setupUi(this);
    ui->textBrowser->setSource(QUrl(tr("qrc:/res/EULA.html")));
    if (accept) {
        ui->buttonBox->clear();
        QPushButton* btn = ui->buttonBox->addButton(QDialogButtonBox::Ok);
        btn->setText(tr("Accept"));
        btn = ui->buttonBox->addButton(QDialogButtonBox::Cancel);
        btn->setDefault(true);
    }
}

LicenseDialog::~LicenseDialog()
{
    delete ui;
}
