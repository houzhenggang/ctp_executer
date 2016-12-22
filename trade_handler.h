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
#define RSP_QRY_INSTRUMENT_CR   (QEvent::User + 9)  // commistion_rate
#define RSP_QRY_INSTRUMENT      (QEvent::User + 10)
#define RSP_DEPTH_MARKET_DATA   (QEvent::User + 11)
#define RSP_ORDER_INSERT        (QEvent::User + 12)
#define RSP_ORDER_ACTION        (QEvent::User + 13)
#define ERR_RTN_ORDER_INSERT    (QEvent::User + 14)
#define ERR_RTN_ORDER_ACTION    (QEvent::User + 15)
#define RTN_ORDER               (QEvent::User + 16)
#define RTN_TRADE               (QEvent::User + 17)
#define RSP_QRY_ORDER           (QEvent::User + 18)
#define RSP_QRY_TRADE           (QEvent::User + 19)
#define RSP_QRY_POSITION        (QEvent::User + 20)
#define RSP_QRY_POSITION_DETAIL (QEvent::User + 21)

struct RspInfo {
    const int errorID;
    const int nRequestID;

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
    explicit FrontDisconnectedEvent(int Reason) :
        QEvent(QEvent::Type(FRONT_DISCONNECTED)),
        reason(Reason) {}

    int getReason() const { return reason; }
};

class HeartBeatWarningEvent : public QEvent {
protected:
    const int nTimeLapse;

public:
    explicit HeartBeatWarningEvent(int nTimeLapse) :
        QEvent(QEvent::Type(HEARTBEAT_WARNING)),
        nTimeLapse(nTimeLapse) {}

    int getLapseTime() const { return nTimeLapse; }
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

class RspQryInstrumentCommissionRateEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcInstrumentCommissionRateField> instrumentCommissionRateList;

    RspQryInstrumentCommissionRateEvent(QList<CThostFtdcInstrumentCommissionRateField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_QRY_INSTRUMENT_CR)),
        RspInfo(err, id),
        instrumentCommissionRateList(list) {}
};

class RspQryInstrumentEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcInstrumentField> instrumentList;

    RspQryInstrumentEvent(QList<CThostFtdcInstrumentField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_QRY_INSTRUMENT)),
        RspInfo(err, id),
        instrumentList(list) {}
};

class DepthMarketDataEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcDepthMarketDataField depthMarketDataField;

    DepthMarketDataEvent(CThostFtdcDepthMarketDataField *pDepthDataField, int err, int id) :
        QEvent(QEvent::Type(RSP_DEPTH_MARKET_DATA)),
        RspInfo(err, id),
        depthMarketDataField(*pDepthDataField) {}
};

class RspOrderInsertEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcInputOrderField inputOrderField;

    RspOrderInsertEvent(CThostFtdcInputOrderField *pInputOrder, int err, int id) :
        QEvent(QEvent::Type(RSP_ORDER_INSERT)),
        RspInfo(err, id),
        inputOrderField(*pInputOrder) {}
};

class RspOrderActionEvent : public QEvent, public RspInfo {
public:
    const CThostFtdcInputOrderActionField inputOrderActionField;

    RspOrderActionEvent(CThostFtdcInputOrderActionField *pInputOrderAction, int err, int id) :
        QEvent(QEvent::Type(RSP_ORDER_ACTION)),
        RspInfo(err, id),
        inputOrderActionField(*pInputOrderAction) {}
};

class ErrRtnOrderInsertEvent: public QEvent, public RspInfo {
public:
    const CThostFtdcInputOrderField inputOrderField;

    ErrRtnOrderInsertEvent(CThostFtdcInputOrderField *pInputOrder, int err, int id) :
        QEvent(QEvent::Type(ERR_RTN_ORDER_INSERT)),
        RspInfo(err, id),
        inputOrderField(*pInputOrder) {}
};

class ErrRtnOrderActionEvent: public QEvent, public RspInfo {
public:
    const CThostFtdcOrderActionField inputOrderField;

    ErrRtnOrderActionEvent(CThostFtdcOrderActionField *pOrderAction, int err, int id) :
        QEvent(QEvent::Type(ERR_RTN_ORDER_ACTION)),
        RspInfo(err, id),
        inputOrderField(*pOrderAction) {}
};

class RtnOrderEvent : public QEvent {
public:
    const CThostFtdcOrderField orderField;

    explicit RtnOrderEvent(CThostFtdcOrderField *pOrderField) :
        QEvent(QEvent::Type(RTN_ORDER)),
        orderField(*pOrderField) {}
};

class RtnTradeEvent : public QEvent {
public:
    const CThostFtdcTradeField tradeField;

    explicit RtnTradeEvent(CThostFtdcTradeField *pTradeField) :
        QEvent(QEvent::Type(RTN_TRADE)),
        tradeField(*pTradeField) {}
};

class QryOrderEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcOrderField> orderList;

    QryOrderEvent(QList<CThostFtdcOrderField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_QRY_ORDER)),
        RspInfo(err, id),
        orderList(list) {}
};

class QryTradeEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcTradeField> tradeList;

    QryTradeEvent(QList<CThostFtdcTradeField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_QRY_TRADE)),
        RspInfo(err, id),
        tradeList(list) {}
};

class PositionEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcInvestorPositionField> positionList;

    PositionEvent(QList<CThostFtdcInvestorPositionField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_QRY_POSITION)),
        RspInfo(err, id),
        positionList(list) {}
};

class PositionDetailEvent : public QEvent, public RspInfo {
public:
    const QList<CThostFtdcInvestorPositionDetailField> positionDetailList;

    PositionDetailEvent(QList<CThostFtdcInvestorPositionDetailField> list, int err, int id) :
        QEvent(QEvent::Type(RSP_QRY_POSITION_DETAIL)),
        RspInfo(err, id),
        positionDetailList(list) {}
};

class CTradeHandler : public CThostFtdcTraderSpi {
    QObject * const receiver;

    int lastRequestID;
    QList<CThostFtdcSettlementInfoField> settlementInfoList;
    QList<CThostFtdcInstrumentCommissionRateField> instrumentCommissionRateList;
    QList<CThostFtdcInstrumentField> instrumentList;
    QList<CThostFtdcOrderField> orderList;
    QList<CThostFtdcTradeField> tradeList;
    QList<CThostFtdcInvestorPositionField> positionList;
    QList<CThostFtdcInvestorPositionDetailField> positionDetailList;

public:
    explicit CTradeHandler(QObject *obj);
    ~CTradeHandler();

    void postToReceiver(QEvent *event);

    template<class EVT, class F>
    void handleSingleRsp(F *pField, CThostFtdcRspInfoField *pRspInfo, const int nRequestID = -1);

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
    void OnRspQryInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField *pInstrumentCommissionRate, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryInstrument(CThostFtdcInstrumentField *pInstrument, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo);
    void OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo);
    void OnRtnOrder(CThostFtdcOrderField *pOrder);
    void OnRtnTrade(CThostFtdcTradeField *pTrade);

    void OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);

    void OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
    void OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast);
};

#endif // TRADE_HANDLER_H
