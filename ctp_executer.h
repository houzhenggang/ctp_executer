#ifndef CTP_EXECUTER_H
#define CTP_EXECUTER_H

#include <QAtomicInt>
#include <QObject>
#include <QMap>

class CThostFtdcTraderApi;
class CTradeHandler;

class CtpExecuter : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.ctp.ctp_executer")
public:
    explicit CtpExecuter(QObject *parent = 0);
    ~CtpExecuter();

protected:
    QAtomicInt nRequestID;
    CThostFtdcTraderApi *pUserApi;
    CTradeHandler *pHandler;

    int FrontID;
    int SessionID;

    QMap<QString, int> target_pos_map;
    QMap<QString, int> real_pos_map;

    void customEvent(QEvent *event);

    void login();

signals:

public slots:
     // int getTimeDiff(int id);  //shijiancha id = 0, 1, 2, 3
    void setPosition(const QString& instrument, int position);
    int getPosition(const QString& instrument) const;
    void quit() const;
};

#endif // CTP_EXECUTER_H
