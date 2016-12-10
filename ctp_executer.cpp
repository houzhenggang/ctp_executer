#include <QSettings>

#include "ctp_executer.h"
#include "ctp_executer_adaptor.h"
#include "trade_handler.h"

#define DATE_TIME (QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))

CtpExecuter::CtpExecuter(QObject *parent) :
    QObject(parent)
{
    nRequestID = 0;
    FrontID = 0;
    SessionID = 0;

    pUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi();    // TODO Add parameter
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

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "ctp", "ctp_executer");
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
            FrontID = uevent->RspUserLogin.FrontID;
            SessionID = uevent->RspUserLogin.SessionID;
            qDebug() << DATE_TIME << "OnUserLogin OK! FrontID = " << FrontID << ", SessionID = " << SessionID;
        } else {
            qDebug() << DATE_TIME << "OnUserLogin: ErrorID = " << uevent->errorID;
        }
    }
        break;
    default:
        QObject::customEvent(event);
        break;
    }
}

void CtpExecuter::login()
{
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "ctp", "ctp_executer");
    settings.beginGroup("AccountInfo");
    QString brokerID = settings.value("BrokerID").toString();
    QString userID = settings.value("UserID").toString();
    QString password = settings.value("Password").toString();
    settings.endGroup();

    CThostFtdcReqUserLoginField reqUserLogin;
    strcpy(reqUserLogin.BrokerID, brokerID.toLatin1().data());
    strcpy(reqUserLogin.UserID, userID.toLatin1().data());
    strcpy(reqUserLogin.Password, password.toLatin1().data());

    pUserApi->ReqUserLogin(&reqUserLogin, nRequestID.fetchAndAddRelaxed(1));
}

void CtpExecuter::setPosition(const QString& instrument, int position)
{
    qDebug() << "setPosition" << instrument << "\t" << position;
    target_pos_map.insert(instrument, position);
}

int CtpExecuter::getPosition(const QString& instrument) const
{
    // TODO check update time
    return real_pos_map.value(instrument);
}

void CtpExecuter::quit() const
{
    QCoreApplication::quit();
}
