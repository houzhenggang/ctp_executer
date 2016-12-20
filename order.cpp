#include <string.h>
#include <QString>

#include "order.h"

Order::Order()
{
}

Order::Order(const CThostFtdcOrderField &field)
{
    vol = field.VolumeTotalOriginal;
    vol_remain = field.VolumeTotal;
    memcpy(ref, field.OrderRef, sizeof(TThostFtdcOrderRefType));
    front = field.FrontID;
    session = field.SessionID;
    instrument = field.InstrumentID;
    status = field.OrderStatus;
    direction = field.Direction;
}

Order::Order(const Order &other)
{
    vol = other.vol;
    vol_remain = other.vol_remain;
    memcpy(ref, other.ref, sizeof(TThostFtdcOrderRefType));
    front = other.front;
    session = other.session;
    instrument = other.instrument;
    status = other.status;
    direction = other.direction;
}
