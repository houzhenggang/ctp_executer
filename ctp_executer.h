#ifndef CTP_EXECUTER_H
#define CTP_EXECUTER_H

#include <QObject>
#include <QAtomicInt>
#include <QByteArray>
#include <QMap>
#include <QMutex>
#include <QDateTime>
#include <QPair>

class CThostFtdcTraderApi;
class CTradeHandler;
class Order;

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
    QMap<QString, int> yd_pos_map;
    QMap<QString, int> td_pos_map;
    QDateTime pos_update_time;
    QMultiMap<QString, Order> order_map;
    QDateTime order_expire_time;

    QMap<QString, QDateTime> instrument_expire_time_map;
    QMap<QString, QPair<double, double>> instrument_upper_lower_map;

    void customEvent(QEvent *event) override;

    template<typename Fn, typename T>
    int callTraderApi(Fn pTraderApi, T * ptr);

private slots:
    int login();
    int qrySettlementInfo();
    int confirmSettlementInfo();
    int qrySettlementInfoConfirm();
    int qryTradingAccount();
    int qryInstrumentCommissionRate(const QString &instrument = QString());
    int qryInstrument(const QString &instrument = QString(), const QString &exchangeID = QString());
    int qryDepthMarketData(const QString &instrument = QString());
    int insertLimitOrder(const QString &instrument, bool open, int volume, double price);
    int cancelOrder(char* orderRef, int frontID, int sessionID, const QString &instrument);
    int qryMaxOrderVolume(const QString &instrument, bool buy, char offsetFlag);
    int qryOrder(const QString &instrument = QString());
    int qryTrade(const QString &instrument = QString());
    int qryPosition(const QString &instrument = QString());
    int qryPositionDetail(const QString &instrument = QString());

    QDateTime getExpireTime() const;
    void operate(const QString &instrument, int new_position);

signals:
    void heartBeatWarning(int nTimeLapse);
    void dealMade(const QString& instrument, int volume);
public slots:
    QString getTradingDay() const;
    void setPosition(const QString& instrument, int new_position);
    int getPosition(const QString& instrument) const;
    int getPendingOrderPosition(const QString &instrument) const;
    void quit();
};

#endif // CTP_EXECUTER_H
