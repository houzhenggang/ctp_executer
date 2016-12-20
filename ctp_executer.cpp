#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <functional>
#include <QSettings>
#include <QtConcurrentRun>

#include "ctp_executer.h"
#include "ctp_executer_adaptor.h"
#include "trade_handler.h"
#include "order.h"

#ifdef Q_OS_WIN
#include <Windows.h>
#endif

/*!
 * \brief _sleep
 * 暂停当前线程. 代码拷贝自QTestLib模块, 不要在主线程中调用.
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
        // TODO
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
        } else {
            qDebug() << DATE_TIME << "OnUserLogin: ErrorID = " << uevent->errorID;
        }
    }
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
    case RSP_DEPTH_MARKET_DATA:
    {
        auto *devent = static_cast<DepthMarketDataEvent*>(event);
        QString instrument = devent->depthMarketDataField.InstrumentID;
        double lastPrice = devent->depthMarketDataField.LastPrice;
        qDebug() << instrument << ", lastPrice = " << lastPrice;
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
        emit dealMade(tevent->tradeField.InstrumentID, volume);
    }
        break;
    case RSP_QRY_ORDER:
    {
        auto *qoevent = static_cast<QryOrderEvent*>(event);
        order_map.clear();
        foreach (const auto &item, qoevent->orderList) {
            order_map.insert(item.InstrumentID, item);
            qDebug() << item.OrderStatus << QTextCodec::codecForName("GBK")->toUnicode(item.StatusMsg);
        }
        order_update_time = QDateTime::currentDateTime();
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
        foreach (const auto &item, pevent->positionList) {
            qDebug() << item.InstrumentID << item.Position;
        }
    }
        break;
    case RSP_QRY_POSITION_DETAIL:
    {
        auto *pevent = static_cast<PositionDetailEvent*>(event);
        real_pos_map.clear();
        foreach (const auto &item, pevent->positionDetailList) {
            real_pos_map[item.InstrumentID] = item.Volume;
        }
        pos_update_time = QDateTime::currentDateTime();
    }
        break;
    default:
        QObject::customEvent(event);
        break;
    }
}

/*!
 * \brief CtpExecuter::callTraderApi
 * 尝试调用traderApi, 如果失败(返回值不是0),
 * 就在一个新线程里反复调用traderApi, 直至成功
 *
 * \param traderApi 无参函数对象
 * \param ptr 成功调用traderApi或超时之后释放
 */
template<typename Fn>
void CtpExecuter::callTraderApi(Fn &traderApi, void * ptr)
{
    if (traderApiMutex.tryLock()) {
        int ret = traderApi();
        traderApiMutex.unlock();
        if (ret == 0) {
            free(ptr);
            return;
        }
    }

    QtConcurrent::run([=]() -> void {
        int count_down = 100;
        while (count_down-- > 0) {
            _sleep(400 - count_down * 2);   // TODO 改进退避算法
            traderApiMutex.lock();
            int ret = traderApi();
            traderApiMutex.unlock();
            if (ret == 0) {
                break;
            }
        }
        free(ptr);
    });
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
    auto *pInfoField = (CThostFtdcQrySettlementInfoField*) malloc(sizeof (CThostFtdcQrySettlementInfoField));
    memset(pInfoField, 0, sizeof (CThostFtdcQrySettlementInfoField));
    strcpy(pInfoField->BrokerID, c_brokerID);
    strcpy(pInfoField->InvestorID, c_userID);

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQrySettlementInfo, pUserApi, pInfoField, id);
    callTraderApi(traderApi, pInfoField);

    return id;
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
    auto *pConfirmField = (CThostFtdcQrySettlementInfoConfirmField*) malloc(sizeof (CThostFtdcQrySettlementInfoConfirmField));
    memset(pConfirmField, 0, sizeof (CThostFtdcQrySettlementInfoConfirmField));
    strcpy(pConfirmField->BrokerID, c_brokerID);
    strcpy(pConfirmField->InvestorID, c_userID);

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQrySettlementInfoConfirm, pUserApi, pConfirmField, id);
    callTraderApi(traderApi, pConfirmField);

    return id;
}

/*!
 * \brief CtpExecuter::qryTradingAccount
 * 发送查询资金账户请求
 *
 * \return nRequestID
 */
int CtpExecuter::qryTradingAccount()
{
    auto *pAccountField = (CThostFtdcQryTradingAccountField*) malloc(sizeof (CThostFtdcQryTradingAccountField));
    memset(pAccountField, 0, sizeof (CThostFtdcQryTradingAccountField));
    strcpy(pAccountField->BrokerID, c_brokerID);
    strcpy(pAccountField->InvestorID, c_userID);

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQryTradingAccount, pUserApi, pAccountField, id);
    callTraderApi(traderApi, pAccountField);

    return id;
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

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQryDepthMarketData, pUserApi, pField, id);
    callTraderApi(traderApi, pField);

    return id;
}

/*!
 * \brief CtpExecuter::limitOrder
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
 * \brief CtpExecuter::qryOrder
 * 查询报单
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

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQryOrder, pUserApi, pField, id);
    callTraderApi(traderApi, pField);

    return id;
}

/*!
 * \brief CtpExecuter::qryOrder
 * 查询成交
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

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQryTrade, pUserApi, pField, id);
    callTraderApi(traderApi, pField);

    return id;
}

/*!
 * \brief CtpExecuter::qryPosition
 * 查询持仓
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

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQryInvestorPosition, pUserApi, pField, id);
    callTraderApi(traderApi, pField);

    return id;
}

/*!
 * \brief CtpExecuter::qryPositionDetail
 * 查询持仓明细
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

    int id = nRequestID.fetchAndAddRelaxed(1);
    auto traderApi = std::bind(&CThostFtdcTraderApi::ReqQryInvestorPositionDetail, pUserApi, pField, id);
    callTraderApi(traderApi, pField);

    return id;
}

/*!
 * \brief CtpExecuter::setPosition
 * 为该合约设置一个新的仓位, 如果与原仓位不同, 则执行操作
 *
 * \param instrument 合约代码
 * \param position 新仓位
 */
void CtpExecuter::setPosition(const QString& instrument, int position)
{
    qryDepthMarketData(instrument); // TODO 建立缓存机制, 不必每次查询
    target_pos_map.insert(instrument, position);

    int real_pos = getPosition(instrument);
    if (real_pos != -INT_MAX) {
        int pending_order_pos = getPendingOrderPosition(instrument);
        if (real_pos + pending_order_pos != position) {
            // 执行操作
        }
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
    if (pos_update_time.isValid()) {
        return real_pos_map.value(instrument);
    } else {
        return -INT_MAX;
    }
}

/*!
 * \brief CtpExecuter::getPendingOrderPosition
 * 获取该合约未成交订单的仓位
 *
 * \param instrument 被查询的合约代码
 * \return 该合约未成交订单的仓位之和, 如果查询结果已经过期返回-INT_MAX
 */
int CtpExecuter::getPendingOrderPosition(const QString &instrument) const
{
    if (!order_update_time.isValid()) {
        return -INT_MAX;
    }
    int sum = 0;
    const auto orderList = order_map.values(instrument);
    foreach (const auto& order, orderList) {
        if (order.status == THOST_FTDC_OST_PartTradedQueueing ||
                order.status == THOST_FTDC_OST_NoTradeQueueing ||
                order.status == THOST_FTDC_OST_Unknown)
        {
            int position = order.vol_remain;
            if (order.direction == THOST_FTDC_D_Sell) {
                position *= -1;
            }
            sum += position;
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
