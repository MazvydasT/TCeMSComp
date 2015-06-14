#include "rootwindow.h"
#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setOrganizationName("JLR");
    a.setApplicationName("TCeMSComp");
    //a.setStyle("fusion");
    RootWindow w;
    w.show();

    return a.exec();
}
