#include "mainwindow.h"
#include "licensedialog.h"
#include <QApplication>
#include <QTranslator>
#include <QSettings>
#include <QProxyStyle>

class AppProxyStyle : public QProxyStyle
{
  public:
    int styleHint(StyleHint hint, const QStyleOption *option = 0,
                  const QWidget *widget = 0, QStyleHintReturn *returnData = 0) const
    {
        if (hint == QStyle::SH_ToolTip_WakeUpDelay)
            return 500;
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    QCoreApplication::setOrganizationName("Patchdirector");
#endif
    //QCoreApplication::setOrganizationDomain("patchdirector.com");
    //QCoreApplication::setApplicationName("Patchdirector");

    app.setStyle(new AppProxyStyle);

#ifdef Q_OS_WIN
    app.setStyleSheet("* { font-size: 8pt; }");
#endif

    MainWindow::loadTranslation();

#ifdef Q_OS_OSX
    QSettings settings;
    if (!settings.value("EULA").toBool()) {
        LicenseDialog dlg(0, true);
        if (dlg.exec() != QDialog::Accepted) return 0;
        settings.setValue("EULA", true);
    }
#endif

    MainWindow::setMainWnd(new MainWindow());

    return app.exec();
}
