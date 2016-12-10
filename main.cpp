#include <QCoreApplication>

#include "ctp_executer.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    CtpExecuter ctp_executer;
    return a.exec();
}
