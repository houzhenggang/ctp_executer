#include <QCoreApplication>

#include "trade_handler.h"

CTradeHandler::CTradeHandler(QObject *obj) :
    receiver(obj)
{
    lastRequestID = -1;
}

CTradeHandler::~CTradeHandler()
{
    //
}

inline void CTradeHandler::postToReceiver(QEvent *event)
{
    QCoreApplication::postEvent(receiver, event);
}

template<class EVT, class F>
void CTradeHandler::handleSingleRsp(F *pField, CThostFtdcRspInfoField *pRspInfo, const int nRequestID)
{
    int err = -1;
    if (pRspInfo != NULL) {
        err = pRspInfo->ErrorID;
    }

    if (pField != NULL) {
        postToReceiver(new EVT(pField, err, nRequestID));
    }
}

template<class EVT, class F>
void CTradeHandler::handleMultiRsp(QList<F> *pTList, F *pField, CThostFtdcRspInfoField *pRspInfo, const int nRequestID, const bool bIsLast)
{
    if (nRequestID != lastRequestID) {  // 新Rsp
        lastRequestID = nRequestID;
        if (!pTList->isEmpty()) {       // 上一个requestID的消息没有接收全
            postToReceiver(new EVT(*pTList, 0, nRequestID));
            pTList->clear();
        }
        if (pRspInfo != NULL) {
            if (pRspInfo->ErrorID != 0) {
                postToReceiver(new EVT(*pTList, pRspInfo->ErrorID, nRequestID));
                return;
            }
        }
    }

    if (pField != NULL) {
        pTList->append(*pField);
    }
    if (bIsLast) {
        postToReceiver(new EVT(*pTList, 0, nRequestID));
        pTList->clear();
    }
}

void CTradeHandler::OnFrontConnected()
{
    postToReceiver(new FrontConnectedEvent());
}

void CTradeHandler::OnFrontDisconnected(int nReason)
{
    postToReceiver(new FrontDisconnectedEvent(nReason));
}

void CTradeHandler::OnHeartBeatWarning(int nTimeLapse)
{
    postToReceiver(new HeartBeatWarningEvent(nTimeLapse));
}

void CTradeHandler::OnRspUserLogin(CThostFtdcRspUserLoginField *pRspUserLogin, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<UserLoginRspEvent>(pRspUserLogin, pRspInfo, nRequestID);
}

void CTradeHandler::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    handleMultiRsp<SettlementInfoEvent>(&settlementInfoList, pSettlementInfo, pRspInfo, nRequestID, bIsLast);
}

void CTradeHandler::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<SettlementInfoConfirmEvent>(pSettlementInfoConfirm, pRspInfo, nRequestID);
}

void CTradeHandler::OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<SettlementInfoConfirmEvent>(pSettlementInfoConfirm, pRspInfo, nRequestID);
}

void CTradeHandler::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<TradingAccountEvent>(pTradingAccount, pRspInfo, nRequestID);
}

void CTradeHandler::OnRspQryDepthMarketData(CThostFtdcDepthMarketDataField *pDepthMarketData, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<DepthMarketDataEvent>(pDepthMarketData, pRspInfo, nRequestID);
}

void CTradeHandler::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<RspOrderInsertEvent>(pInputOrder, pRspInfo, nRequestID);
}

void CTradeHandler::OnRspOrderAction(CThostFtdcInputOrderActionField *pInputOrderAction, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    Q_UNUSED(bIsLast);
    handleSingleRsp<RspOrderActionEvent>(pInputOrderAction, pRspInfo, nRequestID);
}

void CTradeHandler::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    handleSingleRsp<ErrRtnOrderInsertEvent>(pInputOrder, pRspInfo);
}

void CTradeHandler::OnErrRtnOrderAction(CThostFtdcOrderActionField *pOrderAction, CThostFtdcRspInfoField *pRspInfo)
{
    handleSingleRsp<ErrRtnOrderActionEvent>(pOrderAction, pRspInfo);
}

void CTradeHandler::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    postToReceiver(new RtnOrderEvent(pOrder));
}

void CTradeHandler::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
    postToReceiver(new RtnTradeEvent(pTrade));
}

void CTradeHandler::OnRspQryOrder(CThostFtdcOrderField *pOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    handleMultiRsp<QryOrderEvent>(&orderList, pOrder, pRspInfo, nRequestID, bIsLast);
}

void CTradeHandler::OnRspQryTrade(CThostFtdcTradeField *pTrade, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    handleMultiRsp<QryTradeEvent>(&tradeList, pTrade, pRspInfo, nRequestID, bIsLast);
}

void CTradeHandler::OnRspQryInvestorPosition(CThostFtdcInvestorPositionField *pInvestorPosition, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    handleMultiRsp<PositionEvent>(&positionList, pInvestorPosition, pRspInfo, nRequestID, bIsLast);
}

void CTradeHandler::OnRspQryInvestorPositionDetail(CThostFtdcInvestorPositionDetailField *pInvestorPositionDetail, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    handleMultiRsp<PositionDetailEvent>(&positionDetailList, pInvestorPositionDetail, pRspInfo, nRequestID, bIsLast);
}
