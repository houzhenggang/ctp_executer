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
#define RSP_SETTLEMENT_INFO     (QEvent::User + 6)
#define RSP_SETTLEMENT_CONFIRM  (QEvent::User + 7)
#define RSP_TRADING_ACCOUNT     (QEvent::User + 8)
#define RSP_DEPTH_MARKET_DATA   (QEvent::User + 9)
#define RTN_ORDER               (QEvent::User + 10)
#define QRY_ORDER               (QEvent::User + 11)
#define RSP_POSITION            (QEvent::User + 12)
#define RSP_POSITION_DETAIL     (QEvent::User + 13)

struct RspInfo {
    int errorID;
    int nRequestID;

    RspInfo(int err, int id)
        : errorID(err), nRequestID(id) {}
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
    const CThostFtdcRspUserLoginField rspUserLogin;

    UserLoginRspEvent(CThostFtdcRspUserLoginField *pRspUserLogin, int err, int id) :
        QEvent(QEvent::Type(RSP_USER_LOGIN)),
        RspInfo(err, id),
        rspUserLogin(*pRspUserLogin) {}
};

class SettlementInfoEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcSettlementInfoField> settlementInfoList;

    SettlementInfoEvent(QList<CThostFtdcSettlementInfoField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_SETTLEMENT_INFO)),
        RspInfo(err, id),
        settlementInfoList(list) {}
};

class SettlementInfoConfirmEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcSettlementInfoConfirmField settlementInfoConfirm;

    SettlementInfoConfirmEvent(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, int err, int id) :
        QEvent(QEvent::Type(RSP_SETTLEMENT_CONFIRM)),
        RspInfo(err, id),
        settlementInfoConfirm(*pSettlementInfoConfirm) {}
};

class TradingAccountEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcTradingAccountField tradingAccount;

    TradingAccountEvent(CThostFtdcTradingAccountField *pTradingAccount, int err, int id) :
        QEvent(QEvent::Type(RSP_TRADING_ACCOUNT)),
        RspInfo(err, id),
        tradingAccount(*pTradingAccount) {}
};

class DepthMarketDataEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcDepthMarketDataField depthMarketDataField;

    DepthMarketDataEvent(CThostFtdcDepthMarketDataField *pDepthDataField, int err, int id) :
        QEvent(QEvent::Type(RSP_DEPTH_MARKET_DATA)),
        RspInfo(err, id),
        depthMarketDataField(*pDepthDataField) {}
};

class RtnOrderEvent : public QEvent {
public:
    const CThostFtdcOrderField orderField;

    explicit RtnOrderEvent(CThostFtdcOrderField *pOrderField) :
        QEvent(QEvent::Type(RTN_ORDER)),
        orderField(*pOrderField) {}
};

class QryOrderEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcOrderField> orderList;

    QryOrderEvent(QList<CThostFtdcOrderField> list, int err, int id) :
        QEvent(QEvent::Type(QRY_ORDER)),
        RspInfo(err, id),
        orderList(list) {}
};

class PositionEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcInvestorPositionField> positionList;

    PositionEvent(QList<CThostFtdcInvestorPositionField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_POSITION)),
        RspInfo(err, id),
        positionList(list) {}
};

class PositionDetailEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcInvestorPositionDetailField> positionDetailList;

    PositionDetailEvent(QList<CThostFtdcInvestorPositionDetailField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_POSITION_DETAIL)),
        RspInfo(err, id),
        positionDetailList(list) {}
};

class CTradeHandler : public CThostFtdcTraderSpi {
    QObject * const receiver;

    int lastRequestID;
    QList<CThostFtdcSettlementInfoField> settlementInfoList;
    QList<CThostFtdcOrderField> orderList;
    QList<CThostFtdcInvestorPositionField> positionList;
    QList<CThostFtdcInvestorPositionDetailField> positionDetailList;

public:
    explicit CTradeHandler(QObject *obj);
    ~CTradeHandler();

    void postToReceiver(QEvent *event);

    template<class EVT, class F>
    void handleSingleRsp(F *pField, CThostFtdcRspInfoField *pRspInfo, const int nRequestID);

    template<class EVT, class F>
    void handleMultiRsp(QList<F> *pTList, F *pField, CThostFtdcRspInfoField *pRspInfo, const int nRequestID, const bool bIsLast);

    void OnFrontConnected();

    void OnFrontDisconnected(int nReason);

    void OnHeartBeatWarning(int nTimeLapse);

    void OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRtnOrder(CThostFtdcOrderField *pOrder);
    void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);

    void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
};

#endif // TRADE_HANDLER_H
