#ifndef TRADE_HANDLER_H
#define TRADE_HANDLER_H

#include <QEvent>

#include "ThostFtdcTraderApi.h"

#define FRONT_CONNECTED         (QEvent::User + 0)
#define FRONT_DISCONNECTED      (QEvent::User + 1)
#define HEARTBEAT_WARNING       (QEvent::User + 2)
#define RSP_USER_LOGIN          (QEvent::User + 3)
#define RSP_USER_LOGOUT         (QEvent::User + 4)
#define RSP_ERROR               (QEvent::User + 5)

struct RspInfo {
    int errorID;  // CThostFtdcRspInfoField pRspInfo is too big, error ID is enough
    int nRequestID;
    bool bIsLast;

    RspInfo(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
        : errorID(pRspInfo->ErrorID), nRequestID(nRequestID), bIsLast(bIsLast) {}
};

class FrontConnectedEvent : public QEvent {
public:
    FrontConnectedEvent() :
        QEvent(QEvent::Type(FRONT_CONNECTED)) {}
};

class FrontDisconnectedEvent : public QEvent {
protected:
    const int reason;

public:
    FrontDisconnectedEvent(int Reason) :
        QEvent(QEvent::Type(FRONT_DISCONNECTED)),
        reason(Reason) {}

    int getReason() const { return reason; }
};

class HeartBeatWarningEvent : public QEvent {
protected:
    const int nTimeLapse;

public:
    HeartBeatWarningEvent(int nTimeLapse) :
        QEvent(QEvent::Type(HEARTBEAT_WARNING)),
        nTimeLapse(nTimeLapse) {}

    int getnTimeLapse() const { return nTimeLapse; }
};

class UserLoginRspEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcRspUserLoginField RspUserLogin;

    UserLoginRspEvent(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast) :
        QEvent(QEvent::Type(RSP_USER_LOGIN)),
        RspInfo(pRspInfo, nRequestID, bIsLast),
        RspUserLogin(*pRspUserLogin) {}
};

class CTradeHandler : public CThostFtdcTraderSpi {
    QObject * const receiver;

public:
    explicit CTradeHandler(QObject *obj);
    ~CTradeHandler();

    inline void postToReceiver(QEvent *event);

    void OnFrontConnected();

    void OnFrontDisconnected(int nReason);

    void OnHeartBeatWarning(int nTimeLapse);

    void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
};

#endif // TRADE_HANDLER_H
