#ifndef CTP_EXECUTER_H
#define CTP_EXECUTER_H

#include <QObject>
#include <QAtomicInt>
#include <QByteArray>
#include <QMap>
#include <QMutex>
#include <QDateTime>

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
    QMutex traderApiMutex;
    CThostFtdcTraderApi *pUserApi;
    CTradeHandler *pHandler;

    QByteArray brokerID;
    QByteArray userID;
    QByteArray password;
    char* c_brokerID;
    char* c_userID;
    char* c_password;

    int FrontID;
    int SessionID;

    QMap<QString, int> target_pos_map;
    QMap<QString, int> real_pos_map;
    QDateTime pos_update_time;

    void customEvent(QEvent *event) override;

    template<typename Fn>
    void callTraderApi(Fn &traderApi, void * ptr);

    int login();
    int qrySettlementInfo();
    int confirmSettlementInfo();
    int qrySettlementInfoConfirm();
    int qryTradingAccount();
    int qryDepthMarketData(const QString &instrument);
    int insertLimitOrder(const QString &instrument, bool open, int volume, double price);
    int cancelOrder(char* orderRef, int frontID, int sessionID, const QString &instrument);
    int qryPosition(const QString &instrument = "");
    int qryPositionDetail(const QString &instrument = "");

signals:

public slots:
     // int getTimeDiff(int id);  //shijiancha id = 0, 1, 2, 3
    void setPosition(const QString& instrument, int position);
    int getPosition(const QString& instrument) const;
    void quit();
};

#endif // CTP_EXECUTER_H
