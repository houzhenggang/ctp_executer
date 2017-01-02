#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <QSettings>
#include <QtConcurrentRun>

#include "ctp_executer.h"
#include "ctp_executer_adaptor.h"
#include "trade_handler.h"
#include "order.h"
#include "expires.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

/*!
 * \brief _sleep
 * 暂停当前线程. 代码参考QTestLib模块, 不要在主线程中调用.
 *
 * \param ms 暂停时长(<932毫秒)
 */
static inline void _sleep(int ms)
{
#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = { 0, (ms % 1024) * 1024 * 1024 };
    nanosleep(&ts, NULL);
#endif
}

#define DATE_TIME (QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))

CtpExecuter::CtpExecuter(QObject *parent) :
    QObject(parent)
{
    nRequestID = 0;
    FrontID = 0;
    SessionID = 0;

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "ctp", "ctp_executer");
    QByteArray flowPath = settings.value("FlowPath").toByteArray();

    settings.beginGroup("AccountInfo");
    brokerID = settings.value("BrokerID").toByteArray();
    userID = settings.value("UserID").toByteArray();
    password = settings.value("Password").toByteArray();
    settings.endGroup();

    // Pre-convert QString to char*
    c_brokerID = brokerID.data();
    c_userID = userID.data();
    c_password = password.data();

    pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi(flowPath.data());
    pHandler = new CTradeHandler(this);
    pUserApi->RegisterSpi(pHandler);

    pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);
    pUserApi->SubscribePublicTopic(THOST_TERT_QUICK);

    settings.beginGroup("FrontSites");
    QStringList keys = settings.childKeys();
    const QString protocol = "tcp://";
    foreach (const QString &str, keys) {
        QString address = settings.value(str).toString();
        pUserApi->RegisterFront((protocol + address).toLatin1().data());
    }
    settings.endGroup();

    new Ctp_executerAdaptor(this);
    QDBusConnection dbus = QDBusConnection::sessionBus();
    dbus.registerObject("/ctp_executer", this);
    dbus.registerService("org.ctp.ctp_executer");

    pUserApi->Init();
}

CtpExecuter::~CtpExecuter()
{
    pUserApi->Release();
    delete pHandler;
}

void CtpExecuter::customEvent(QEvent *event)
{
    qDebug() << "customEvent: " << int(event->type());
    switch (int(event->type())) {
    case FRONT_CONNECTED:
        login();
        break;
    case FRONT_DISCONNECTED:
    {
        auto *fevent = static_cast<FrontDisconnectedEvent*>(event);
        // TODO reset position maps, make update times invalid
        switch (fevent->getReason()) {
        case 0x1001: // 网络读失败
            break;
        case 0x1002: // 网络写失败
            break;
        case 0x2001: // 接收心跳超时
            break;
        case 0x2002: // 发送心跳失败
            break;
        case 0x2003: // 收到错误报文
            break;
        default:
            break;
        }
    }
        break;
    case HEARTBEAT_WARNING:
    {
        auto *hevent = static_cast<HeartBeatWarningEvent*>(event);
        emit heartBeatWarning(hevent->getLapseTime());
    }
        break;
    case RSP_USER_LOGIN:
    {
        auto *uevent = static_cast<UserLoginRspEvent*>(event);
        if (uevent->errorID == 0) {
            FrontID = uevent->rspUserLogin.FrontID;
            SessionID = uevent->rspUserLogin.SessionID;
            qDebug() << DATE_TIME << "OnUserLogin OK! FrontID = " << FrontID << ", SessionID = " << SessionID;
            confirmSettlementInfo();
            QTimer::singleShot(1000, this, SLOT(qryOrder()));
            QTimer::singleShot(2000, this, SLOT(qryPositionDetail()));
        } else {
            qDebug() << DATE_TIME << "OnUserLogin: ErrorID = " << uevent->errorID;
        }
    }
        break;
    case RSP_USER_LOGOUT:
        break;
    case RSP_ERROR:
        break;
    case RSP_SETTLEMENT_INFO:
    {
        auto *sevent = static_cast<SettlementInfoEvent*>(event);
        if (sevent->errorID == 0) {
            auto &list = sevent->settlementInfoList;
            QString msg;
            foreach (const auto & item, list) {
                msg += QTextCodec::codecForName("GBK")->toUnicode(item.Content);
            }
            qDebug() << msg;
        }
    }
        break;
    case RSP_TRADING_ACCOUNT:
    {
        auto *tevent = static_cast<TradingAccountEvent*>(event);
        double available = tevent->tradingAccount.Available;
        qDebug() << "available = " << available;
    }
        break;
    case RSP_QRY_INSTRUMENT_CR:
    {
        auto *qcevent = static_cast<RspQryInstrumentCommissionRateEvent*>(event);
    }
        break;
    case RSP_QRY_INSTRUMENT:
    {
        auto *qievent = static_cast<RspQryInstrumentEvent*>(event);
    }
        break;
    case RSP_DEPTH_MARKET_DATA:
    {
        auto *devent = static_cast<DepthMarketDataEvent*>(event);
        foreach (const auto &item, devent->depthMarketDataList) {
            QString instrument = item.InstrumentID;
            upper_lower_limit_map.insert(instrument, make_expires(qMakePair(item.UpperLimitPrice, item.LowerLimitPrice), getExpireTime()));
        }
    }
        break;
    case RSP_ORDER_INSERT:
    {
        auto *ievent = static_cast<RspOrderInsertEvent*>(event);
    }
        break;
    case RSP_ORDER_ACTION:
    {
        auto *aevent = static_cast<RspOrderActionEvent*>(event);
    }
        break;
    case ERR_RTN_ORDER_INSERT:
    {
        auto *eievent = static_cast<ErrRtnOrderInsertEvent*>(event);
    }
        break;
    case ERR_RTN_ORDER_ACTION:
    {
        auto *eaevent = static_cast<ErrRtnOrderActionEvent*>(event);
    }
        break;
    case RTN_ORDER:
    {
        auto *revent = static_cast<RtnOrderEvent*>(event);
        qDebug() << revent->orderField.InsertTime << revent->orderField.OrderStatus <<
                    QTextCodec::codecForName("GBK")->toUnicode(revent->orderField.StatusMsg);
    }
        break;
    case RTN_TRADE:
    {
        auto *tevent = static_cast<RtnTradeEvent*>(event);
        int volume = tevent->tradeField.Volume;
        if (tevent->tradeField.Direction == THOST_FTDC_D_Sell) {
            volume *= -1;
        }

        switch(tevent->tradeField.OffsetFlag) {
        case THOST_FTDC_OF_Open:            //开仓
            td_pos_map[tevent->tradeField.InstrumentID] += volume;
            break;
        case THOST_FTDC_OF_Close:           //平仓
            td_pos_map[tevent->tradeField.InstrumentID] += volume;  // TODO 区分
            break;
        case THOST_FTDC_OF_CloseToday:      //平今
            td_pos_map[tevent->tradeField.InstrumentID] += volume;
            break;
        case THOST_FTDC_OF_CloseYesterday:  //平昨
            yd_pos_map[tevent->tradeField.InstrumentID] += volume;
            break;
        default:
            td_pos_map[tevent->tradeField.InstrumentID] += volume;
            break;
        }

        emit dealMade(tevent->tradeField.InstrumentID, volume);
    }
        break;
    case RSP_QRY_ORDER:
    {
        auto *qoevent = static_cast<QryOrderEvent*>(event);
        order_map.clear();
        foreach (const auto &item, qoevent->orderList) {
            order_map.insert(item.InstrumentID, Expires<Order>(item, getExpireTime()));
            qDebug() << item.OrderStatus << QTextCodec::codecForName("GBK")->toUnicode(item.StatusMsg);
        }
    }
        break;
    case RSP_QRY_TRADE:
    {
        auto *qtevent = static_cast<QryTradeEvent*>(event);
    }
        break;
    case RSP_QRY_POSITION:
    {
        auto *pevent = static_cast<PositionEvent*>(event);
        yd_pos_map.clear();
        td_pos_map.clear();

        foreach (const auto &item, pevent->positionList) {
            int YdPosition = item.YdPosition;   // 昨仓数据不会更新
            int TdPosition = item.TodayPosition;
            if (item.PosiDirection == THOST_FTDC_PD_Short) {
                YdPosition *= -1;
                TdPosition *= -1;
            }
            yd_pos_map[item.InstrumentID] += YdPosition;
            td_pos_map[item.InstrumentID] += TdPosition;
        }
        pos_update_time = QDateTime::currentDateTime();
    }
        break;
    case RSP_QRY_POSITION_DETAIL:
    {
        auto *pevent = static_cast<PositionDetailEvent*>(event);

        QSet<QString> updateSet;
        foreach (const auto &item, pevent->positionDetailList) {
            updateSet.insert(item.InstrumentID);
        }

        foreach (const auto &item, updateSet) {
            yd_pos_map.remove(item);
            td_pos_map.remove(item);
        }

        foreach (const auto &item, pevent->positionDetailList) {
            qDebug() << item.InstrumentID << "position:" << item.Volume << item.OpenDate << item.TradingDay << item.ExchangeID;
            int volume = item.Volume;
            if (item.Direction == THOST_FTDC_D_Sell) {
                volume *= -1;
            }
            if (strcmp(item.OpenDate, item.TradingDay) == 0) {
                td_pos_map[item.InstrumentID] += volume;
            } else {
                yd_pos_map[item.InstrumentID] += volume;
            }
        }
        pos_update_time = QDateTime::currentDateTime();
    }
        break;
    case RSP_QRY_MAX_ORDER_VOL:
    {
        auto *qmevent = static_cast<QryMaxOrderVolumeEvent*>(event);
        qDebug() << qmevent->maxOrderVolumeField.InstrumentID << qmevent->maxOrderVolumeField.MaxVolume;
    }
        break;
    default:
        QObject::customEvent(event);
        break;
    }
}

/*!
 * \brief CtpExecuter::callTraderApi
 * 尝试调用pTraderApi, 如果失败(返回值不是0),
 * 就在一个新线程里反复调用pTraderApi, 直至成功
 *
 * \param pTraderApi CThostFtdcTraderApi类的成员函数指针
 * \param pField pTraderApi函数的第一个参数，成功调用pTraderApi或超时之后释放
 */
template<typename T>
int CtpExecuter::callTraderApi(int (CThostFtdcTraderApi::* pTraderApi)(T *,int), T * pField)
{
    int id = nRequestID.fetchAndAddRelaxed(1);

    if (traderApiMutex.tryLock()) {
        int ret = (pUserApi->*pTraderApi)(pField, id);
        traderApiMutex.unlock();
        if (ret == 0) {
            free(pField);
            return id;
        }
    }

    QtConcurrent::run([=]() -> void {
        int count_down = 100;
        while (count_down-- > 0) {
            _sleep(400 - count_down * 2);   // TODO 改进退避算法
            traderApiMutex.lock();
            int ret = (pUserApi->*pTraderApi)(pField, id);
            traderApiMutex.unlock();
            if (ret == 0) {
                break;
            }
        }
        free(pField);
    });

    return id;
}

/*!
 * \brief CtpExecuter::login
 * 用配置文件中的账号信息登陆交易端
 *
 * \return nRequestID
 */
int CtpExecuter::login()
{
    CThostFtdcReqUserLoginField reqUserLogin;
    memset(&reqUserLogin, 0, sizeof (CThostFtdcReqUserLoginField));
    strcpy(reqUserLogin.BrokerID, c_brokerID);
    strcpy(reqUserLogin.UserID, c_userID);
    strcpy(reqUserLogin.Password, c_password);

    int id = nRequestID.fetchAndAddRelaxed(1);
    traderApiMutex.lock();
    int ret = pUserApi->ReqUserLogin(&reqUserLogin, id);
    traderApiMutex.unlock();
    Q_UNUSED(ret);
    return id;
}

/*!
 * \brief CtpExecuter::qrySettlementInfo
 * 发送投资者结算结果查询请求
 *
 * \return nRequestID
 */
int CtpExecuter::qrySettlementInfo()
{
    auto *pField = (CThostFtdcQrySettlementInfoField*) malloc(sizeof(CThostFtdcQrySettlementInfoField));
    memset(pField, 0, sizeof (CThostFtdcQrySettlementInfoField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);

    return callTraderApi(&CThostFtdcTraderApi::ReqQrySettlementInfo, pField);
}

/*!
 * \brief CtpExecuter::confirmSettlementInfo
 * 发送投资者结算结果确认请求
 *
 * \return nRequestID
 */
int CtpExecuter::confirmSettlementInfo()
{
    CThostFtdcSettlementInfoConfirmField confirmField;
    memset(&confirmField, 0, sizeof (CThostFtdcSettlementInfoConfirmField));
    strcpy(confirmField.BrokerID, c_brokerID);
    strcpy(confirmField.InvestorID, c_userID);

    int id = nRequestID.fetchAndAddRelaxed(1);
    traderApiMutex.lock();
    int ret = pUserApi->ReqSettlementInfoConfirm(&confirmField, id);
    traderApiMutex.unlock();
    Q_UNUSED(ret);
    return id;
}

/*!
 * \brief CtpExecuter::qrySettlementInfoConfirm
 * 发送投资者结算结果确认查询请求
 *
 * \return nRequestID
 */
int CtpExecuter::qrySettlementInfoConfirm()
{
    auto *pField = (CThostFtdcQrySettlementInfoConfirmField*) malloc(sizeof(CThostFtdcQrySettlementInfoConfirmField));
    memset(pField, 0, sizeof (CThostFtdcQrySettlementInfoConfirmField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);

    return callTraderApi(&CThostFtdcTraderApi::ReqQrySettlementInfoConfirm, pField);
}

/*!
 * \brief CtpExecuter::qryTradingAccount
 * 发送查询资金账户请求
 *
 * \return nRequestID
 */
int CtpExecuter::qryTradingAccount()
{
    auto *pField = (CThostFtdcQryTradingAccountField*) malloc(sizeof(CThostFtdcQryTradingAccountField));
    memset(pField, 0, sizeof (CThostFtdcQryTradingAccountField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);

    return callTraderApi(&CThostFtdcTraderApi::ReqQryTradingAccount, pField);
}

/*!
 * \brief CtpExecuter::qryInstrumentCommissionRate
 * 发送查询手续费率请求
 *
 * \param instrument 合约代码(为空代表所有持仓合约)
 * \return nRequestID
 */
int CtpExecuter::qryInstrumentCommissionRate(const QString &instrument)
{
    auto * pField = (CThostFtdcQryInstrumentCommissionRateField*) malloc(sizeof(CThostFtdcQryInstrumentCommissionRateField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);
    strcpy(pField->InstrumentID, instrument.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryInstrumentCommissionRate, pField);
}

/*!
 * \brief CtpExecuter::qryInstrument
 * 发送查询合约请求
 *
 * \param instrument 合约代码(为空代表所有合约)
 * \param exchangeID 交易所代码(为空代表所有交易所)
 * \return nRequestID
 */
int CtpExecuter::qryInstrument(const QString &instrument, const QString &exchangeID)
{
    auto * pField = (CThostFtdcQryInstrumentField*) malloc(sizeof(CThostFtdcQryInstrumentField));
    strcpy(pField->InstrumentID, instrument.toLatin1().data());
    strcpy(pField->ExchangeID, exchangeID.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryInstrument, pField);
}

/*!
 * \brief CtpExecuter::qryDepthMarketData
 * 发送查询行情请求
 *
 * \param instrument 要查询的合约代码
 * \return nRequestID
 */
int CtpExecuter::qryDepthMarketData(const QString &instrument)
{
    auto *pField = (CThostFtdcQryDepthMarketDataField*) malloc(sizeof(CThostFtdcQryDepthMarketDataField));
    strcpy(pField->InstrumentID, instrument.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryDepthMarketData, pField);
}

/*!
 * \brief CtpExecuter::insertLimitOrder
 * 下限价单
 *
 * \param instrument 合约代码
 * \param price 价格(限价, 不得超出涨跌停范围)
 * \param volume 手数(非零整数, 正数代表做多, 负数代表做空)
 * \param open 开仓(true)/平仓(false)标志
 * \return nRequestID
 */
int CtpExecuter::insertLimitOrder(const QString &instrument, bool open, int volume, double price)
{
    Q_ASSERT(volume != 0 && price > 0.0);

    CThostFtdcInputOrderField inputOrder;
    memset(&inputOrder, 0, sizeof (CThostFtdcInputOrderField));
    strcpy(inputOrder.BrokerID, c_brokerID);
    strcpy(inputOrder.InvestorID, c_userID);
    strcpy(inputOrder.InstrumentID, instrument.toLatin1().data());
    strcpy(inputOrder.OrderRef, "");
//	sprintf(inputOrder.OrderRef, "%12d", orderRef);
//	orderRef++;

    inputOrder.Direction = volume > 0 ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell;
    inputOrder.CombOffsetFlag[0] = open ? THOST_FTDC_OF_Open : THOST_FTDC_OF_Close;
    inputOrder.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
    inputOrder.VolumeTotalOriginal = qAbs(volume);
    inputOrder.VolumeCondition = THOST_FTDC_VC_AV;
    inputOrder.MinVolume = 1;
    inputOrder.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
    inputOrder.IsAutoSuspend = 0;
    inputOrder.UserForceClose = 0;
    inputOrder.ContingentCondition = THOST_FTDC_CC_Immediately;
    inputOrder.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
    inputOrder.LimitPrice = price;
    inputOrder.TimeCondition = THOST_FTDC_TC_GFD;

    int id = nRequestID.fetchAndAddRelaxed(1);
    traderApiMutex.lock();
    int ret = pUserApi->ReqOrderInsert(&inputOrder, id);
    traderApiMutex.unlock();
    Q_UNUSED(ret);
    return id;
}

/*!
 * \brief CtpExecuter::cancelOrder
 * 撤单
 *
 * \param orderRef 报单饮用(TThostFtdcOrderRefType)
 * \param frontID 前置编号
 * \param sessionID 会话编号
 * \param instrument 合约代码
 * \return nRequestID
 */
int CtpExecuter::cancelOrder(char* orderRef, int frontID, int sessionID, const QString &instrument)
{
    CThostFtdcInputOrderActionField orderAction;
    memset(&orderAction, 0, sizeof(CThostFtdcInputOrderActionField));
    strcpy(orderAction.BrokerID, c_brokerID);
    strcpy(orderAction.InvestorID, c_userID);
    memcpy(orderAction.OrderRef, orderRef, sizeof(TThostFtdcOrderRefType));
    orderAction.FrontID = frontID;
    orderAction.SessionID = sessionID;
    orderAction.ActionFlag = THOST_FTDC_AF_Delete;
    strcpy(orderAction.InstrumentID, instrument.toLatin1().data());

    int id = nRequestID.fetchAndAddRelaxed(1);
    traderApiMutex.lock();
    int ret = pUserApi->ReqOrderAction(&orderAction, id);
    traderApiMutex.unlock();
    Q_UNUSED(ret);
    return id;
}

/*!
 * \brief CtpExecuter::qryMaxOrderVolume
 * 发送查询最大报单数量请求
 *
 * \param instrument 合约代码
 * \return nRequestID
 */
int CtpExecuter::qryMaxOrderVolume(const QString &instrument, bool buy, char offsetFlag)
{
    auto *pField = (CThostFtdcQueryMaxOrderVolumeField *) malloc(sizeof(CThostFtdcQueryMaxOrderVolumeField));
    memset(pField, 0, sizeof(CThostFtdcQueryMaxOrderVolumeField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);
    strcpy(pField->InstrumentID, instrument.toLatin1().data());
    pField->Direction = buy ? THOST_FTDC_D_Buy : THOST_FTDC_D_Sell;
    pField->OffsetFlag = offsetFlag;
    pField->HedgeFlag = THOST_FTDC_HF_Speculation;

    return callTraderApi(&CThostFtdcTraderApi::ReqQueryMaxOrderVolume, pField);
}

/*!
 * \brief CtpExecuter::qryOrder
 * 发送查询报单请求
 *
 * \param instrument 合约代码
 * \return nRequestID
 */
int CtpExecuter::qryOrder(const QString &instrument)
{
    auto *pField = (CThostFtdcQryOrderField *) malloc(sizeof(CThostFtdcQryOrderField));
    memset(pField, 0, sizeof(CThostFtdcQryOrderField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);
    strcpy(pField->InstrumentID, instrument.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryOrder, pField);
}

/*!
 * \brief CtpExecuter::qryTrade
 * 发送查询成交请求
 *
 * \param instrument 合约代码
 * \return nRequestID
 */
int CtpExecuter::qryTrade(const QString &instrument)
{
    auto *pField = (CThostFtdcQryTradeField *) malloc(sizeof(CThostFtdcQryTradeField));
    memset(pField, 0, sizeof(CThostFtdcQryTradeField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);
    strcpy(pField->InstrumentID, instrument.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryTrade, pField);
}

/*!
 * \brief CtpExecuter::qryPosition
 * 发送查询持仓请求
 *
 * \param instrument 合约代码
 * \return nRequestID
 */
int CtpExecuter::qryPosition(const QString &instrument)
{
    auto *pField = (CThostFtdcQryInvestorPositionField*) malloc(sizeof(CThostFtdcQryInvestorPositionField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);
    strcpy(pField->InstrumentID, instrument.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryInvestorPosition, pField);
}

/*!
 * \brief CtpExecuter::qryPositionDetail
 * 发送查询持仓明细请求
 *
 * \param instrument 合约代码
 * \return nRequestID
 */
int CtpExecuter::qryPositionDetail(const QString &instrument)
{
    auto *pField = (CThostFtdcQryInvestorPositionDetailField*) malloc(sizeof(CThostFtdcQryInvestorPositionDetailField));
    strcpy(pField->BrokerID, c_brokerID);
    strcpy(pField->InvestorID, c_userID);
    strcpy(pField->InstrumentID, instrument.toLatin1().data());

    return callTraderApi(&CThostFtdcTraderApi::ReqQryInvestorPositionDetail, pField);
}

/*!
 * \brief CtpExecuter::getExpireTime
 * 根据TradingDay生成过期时间(下午5点过期)
 *
 * \return 过期时间
 */
QDateTime CtpExecuter::getExpireTime() const
{
    QString tradingDay = pUserApi->GetTradingDay();
    QDateTime expireTime = QDateTime::fromString(tradingDay, "yyyyMMdd");
    if (expireTime.isValid()) {
        expireTime.setTime(QTime(17, 0));
    }
    return expireTime;
}

/*!
 * \brief CtpExecuter::operate
 * 操作合约(必要的话)使得其仓位与目标值一致
 *
 * \param instrument 合约代码
 */
void CtpExecuter::operate(const QString &instrument, int new_position)
{
    const double high = upper_lower_limit_map[instrument].originalValue().first;
    const double low = upper_lower_limit_map[instrument].originalValue().second;

    int position = getPosition(instrument);
    int pending_order_pos = getPendingOrderVolume(instrument);

    if (position != -INT_MAX) {
        if (position + pending_order_pos != new_position) {
            if (pending_order_pos == 0) {   // TODO 必须所有order的未成交volume都为0
                if (new_position >= 0 && position < 0) {
                    insertLimitOrder(instrument, false, -position, high);
                    position = 0;
                } else if (new_position <= 0 && position > 0) {
                    insertLimitOrder(instrument, false, -position, low);
                    position = 0;
                }

                int diff = new_position - position;

                if (diff < 0 && new_position > 0) {
                    insertLimitOrder(instrument, false, diff, low);
                } else if (diff < 0 && 0 >= position) {
                    insertLimitOrder(instrument, true, diff, low);
                } else if (diff > 0 && 0 <= position) {
                    insertLimitOrder(instrument, true, diff, high);
                } else if (diff > 0 && new_position < 0) {
                    insertLimitOrder(instrument, false, diff, high);
                }
            }
        }
    }
}

/*!
 * \brief CtpExecuter::getTradingDay
 * 获取交易日
 *
 * \return 交易日(YYYYMMDD)
 */
QString CtpExecuter::getTradingDay() const
{
    return pUserApi->GetTradingDay();
}

/*!
 * \brief CtpExecuter::setPosition
 * 为该合约设置一个新的目标仓位, 如果与原仓位不同, 则执行相应操作以达成目标
 *
 * \param instrument 合约代码
 * \param new_position 新目标仓位
 */
void CtpExecuter::setPosition(const QString& instrument, int new_position)
{
    target_pos_map.insert(instrument, new_position);
    if (upper_lower_limit_map[instrument].expired()) {
        qryDepthMarketData(instrument);
        // TODO postEvent, call operate() in customEvent()
    } else {
        operate(instrument, new_position);
    }
}

/*!
 * \brief CtpExecuter::getPosition
 * 获取实盘仓位
 *
 * \param instrument 被查询的合约代码
 * \return 该合约实盘仓位, 如果查询结果已经过期返回-INT_MAX
 */
int CtpExecuter::getPosition(const QString& instrument) const
{
    if (pos_update_time.isNull()) {
        return -INT_MAX;
    } else {
        return yd_pos_map.value(instrument) + td_pos_map.value(instrument);
    }
}

/*!
 * \brief CtpExecuter::getPendingOrderVolume
 * 获取该合约未成交订单的仓位
 * 该函数必须在成功登陆并更新订单表之后调用
 *
 * \param instrument 被查询的合约代码
 * \return 该合约未成交订单的仓位之和
 */
int CtpExecuter::getPendingOrderVolume(const QString &instrument) const
{
    int sum = 0;
    const auto orderList = order_map.values(instrument);
    foreach (const auto& item, orderList) {
        if (item.expired()) {
            continue;
        }
        auto order = item.originalValue();
        if (order.status == THOST_FTDC_OST_PartTradedQueueing ||
                order.status == THOST_FTDC_OST_NoTradeQueueing ||
                order.status == THOST_FTDC_OST_Unknown)
        {
            sum += order.remainVolume();
        }
    }
    return sum;
}

/*!
 * \brief CtpExecuter::quit
 * 退出
 */
void CtpExecuter::quit()
{
    QCoreApplication::quit();
}
