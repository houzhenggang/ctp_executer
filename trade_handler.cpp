#include <QCoreApplication>

#include "trade_handler.h"
#include "ctp_executer.h"

CTradeHandler::CTradeHandler(QObject *obj) :
    receiver(obj)
{
}

CTradeHandler::~CTradeHandler()
{
    //
}

inline void CTradeHandler::postToReceiver(QEvent *event)
{
    QCoreApplication::postEvent(receiver, event);
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
    postToReceiver(new UserLoginRspEvent(pRspUserLogin, pRspInfo, nRequestID));
}

void CTradeHandler::OnRspQrySettlementInfo(CThostFtdcSettlementInfoField *pSettlementInfo, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    printf("OnRspQrySettlementInfo\n");
    if (pSettlementInfo == NULL) {
        printf("pSettlementInfo = NULL\n");
    }
    if (pRspInfo == NULL) {
        printf("pRspInfo = NULL\n");
    }
    if (pSettlementInfo != NULL && pRspInfo != NULL) {
        postToReceiver(new SettlementInfoEvent(pSettlementInfo, pRspInfo, nRequestID));
    }
    if (pRspInfo != NULL)
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
        pRspInfo->ErrorMsg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);

    if (pSettlementInfo != NULL) {
        printf("TradingDay: %s, SettlementID = %d, SequenceNo = %d\n", pSettlementInfo->TradingDay, pSettlementInfo->SettlementID, pSettlementInfo->SequenceNo);
        printf("%s\n", pSettlementInfo->Content);
    }
}

void CTradeHandler::OnRspSettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    printf("OnRspSettlementInfoConfirm\n");
    if (pSettlementInfoConfirm == NULL) {
        printf("pSettlementInfoConfirm = NULL\n");
    }
    if (pRspInfo == NULL) {
        printf("pRspInfo = NULL\n");
    }
    if (pSettlementInfoConfirm != NULL && pRspInfo != NULL)
    postToReceiver(new SettlementInfoConfirmEvent(pSettlementInfoConfirm, pRspInfo, nRequestID));
    if (pRspInfo != NULL)
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
        pRspInfo->ErrorMsg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);

    if (pSettlementInfoConfirm != NULL)
        printf("%s\t%s\n", pSettlementInfoConfirm->ConfirmDate, pSettlementInfoConfirm->ConfirmTime);
}

void CTradeHandler::OnRspQrySettlementInfoConfirm(CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    printf("OnRspQrySettlementInfoConfirm\n");

    if (pSettlementInfoConfirm == NULL) {
        printf("pSettlementInfoConfirm = NULL\n");
    }
    if (pRspInfo == NULL) {
        printf("pRspInfo = NULL\n");
    }
    if (pSettlementInfoConfirm != NULL && pRspInfo != NULL)
    postToReceiver(new SettlementInfoConfirmEvent(pSettlementInfoConfirm, pRspInfo, nRequestID));
    if (pRspInfo != NULL)
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
        pRspInfo->ErrorMsg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);

    if (pSettlementInfoConfirm != NULL)
        printf("%s\t%s\n", pSettlementInfoConfirm->ConfirmDate, pSettlementInfoConfirm->ConfirmTime);
}

void CTradeHandler::OnRspQryTradingAccount(CThostFtdcTradingAccountField *pTradingAccount, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    printf("OnRspQryTradingAccount\n");
    if (pRspInfo != NULL)
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
        pRspInfo->ErrorMsg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);

    if(pTradingAccount != NULL) {
        printf("%llf\n", pTradingAccount->Available);
    }
}

void CTradeHandler::OnRspOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    printf("OnRspOrderInsert\n");
    if (pRspInfo != NULL)
    printf("ErrorCode=[%d], ErrorMsg=[%s]\n", pRspInfo->ErrorID,
        pRspInfo->ErrorMsg);
    printf("RequestID=[%d], Chain=[%d]\n", nRequestID, bIsLast);
}

void CTradeHandler::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
    printf("OnRtnOrder\n");
    if (pOrder != NULL)
        printf("StatusMsg: %s\n", pOrder->StatusMsg);
}

void CTradeHandler::OnErrRtnOrderInsert(CThostFtdcInputOrderField *pInputOrder, CThostFtdcRspInfoField *pRspInfo)
{
    printf("OnErrRtnOrderInsert: %s\n", pRspInfo->ErrorMsg);
}
