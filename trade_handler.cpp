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
    postToReceiver(new UserLoginRspEvent(pRspUserLogin, pRspInfo, nRequestID, bIsLast));
}
