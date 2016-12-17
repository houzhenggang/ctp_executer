#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <functional>
#include <QSettings>

#include "ctp_executer.h"
#include "ctp_executer_adaptor.h"
#include "trade_handler.h"

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
    QByteArray flowPath = settings.value("FlowPath").toString().toLatin1();

    settings.beginGroup("AccountInfo");
    brokerID = settings.value("BrokerID").toString().toLatin1();
    userID = settings.value("UserID").toString().toLatin1();
    password = settings.value("Password").toString().toLatin1();
    settings.endGroup();

    // Pre-convert QString to char*
    c_brokerID = brokerID.data();
    c_userID = userID.data();
    c_password = password.data();

    pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi(flowPath.data());
    pHandler = new CTradeHandler(this);
    pUserApi->RegisterSpi(pHandler);

    // 订阅私有流
    // TERT_RESTART:从本交易日开始重传
    // TERT_RESUME:从上次收到的续传
    // TERT_QUICK:只传送登录后私有流的内容
    pUserApi->SubscribePrivateTopic(THOST_TERT_QUICK);
    // 订阅公共流
    // TERT_RESTART:从本交易日开始重传
    // TERT_RESUME:从上次收到的续传
    // TERT_QUICK:只传送登录后公共流的内容
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
        FrontDisconnectedEvent *fevent = static_cast<FrontDisconnectedEvent*>(event);
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
    case RSP_USER_LOGIN:
    {
        UserLoginRspEvent *uevent = static_cast<UserLoginRspEvent*>(event);
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
        SettlementInfoEvent *sevent = static_cast<SettlementInfoEvent*>(event);
        if (sevent->errorID == 0) {
            auto list = sevent->settlementInfoList;
            QString msg;
            foreach (const auto & item, list) {
                msg += item.Content;
            }
            qDebug() << msg;
        }
    }
        break;
    case RSP_TRADING_ACCOUNT:
    {
        TradingAccountEvent *tevent = static_cast<TradingAccountEvent*>(event);
        double available = tevent->tradingAccount.Available;
        qDebug() << "available = " << available;
    }
        break;
    case RSP_DEPTH_MARKET_DATA:
    {
        DepthMarketDataEvent *devent = static_cast<DepthMarketDataEvent*>(event);
        double lastPrice = devent->depthMarketDataField.LastPrice;
        qDebug() << "lastPrice = " << lastPrice;
    }
        break;
    default:
        QObject::customEvent(event);
        break;
    }
}

/*!
 * \brief CtpExecuter::callTraderApi
 * 尝试调用traderApi, 如果失败就
 * 在一个新线程里反复调用traderApi, 直至成功(返回0)
 *
 * \param traderApi 无参函数对象
 * \param ptr 成功调用traderApi或超时之后释放
 */
template<typename Fn>
void CtpExecuter::callTraderApi(Fn &traderApi, void * ptr)
{
    if (reqMutex.tryLock()) {
        int ret = traderApi();
        reqMutex.unlock();
        if (ret == 0) {
            free(ptr);
            return;
        }
    }

    QtConcurrent::run([=]() -> void {
        int count_down = 100;
        while (count_down-- > 0) {
            _sleep(400 - count_down * 2);   // TODO 改进退避算法
            reqMutex.lock();
            int ret = traderApi();
            reqMutex.unlock();
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
    reqMutex.lock();
    int ret = pUserApi->ReqUserLogin(&reqUserLogin, id);
    reqMutex.unlock();
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
    reqMutex.lock();
    int ret = pUserApi->ReqSettlementInfoConfirm(&confirmField, id);
    reqMutex.unlock();
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
//	strcpy(inputOrder.OrderRef, orderRef);
//	inputOrder.OrderRef[0]++;

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
    reqMutex.lock();
    int ret = pUserApi->ReqOrderInsert(&inputOrder, id);
    reqMutex.unlock();
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
    reqMutex.lock();
    int ret = pUserApi->ReqOrderAction(&orderAction, id);
    reqMutex.unlock();
    Q_UNUSED(ret);
    return id;
}

void CtpExecuter::setPosition(const QString& instrument, int position)
{
    qryDepthMarketData(instrument); // TODO 建立缓存机制, 不必每次查询
    target_pos_map.insert(instrument, position);
    // TODO postEvent to message loop
}

/*!
 * \brief CtpExecuter::getPosition
 * \param instrument 合约代码
 * \return 实盘仓位
 */
int CtpExecuter::getPosition(const QString& instrument) const
{
    // TODO check update time
    return real_pos_map.value(instrument);
}

void CtpExecuter::quit()
{
    QCoreApplication::quit();
}
