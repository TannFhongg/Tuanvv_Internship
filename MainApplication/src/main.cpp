#include "applicationcontroller.h"
#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    ApplicationController controller;
    MainWindow window(&controller);
    window.show();
    return QApplication::exec();
}
