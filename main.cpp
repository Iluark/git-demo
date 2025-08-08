#include <QApplication>
#include "ProcessModelWithQt3D.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ProcessModelWithQt3D w;
    w.resize(1000, 700);
    w.show();

    return app.exec();
}