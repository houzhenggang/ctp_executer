#ifndef ORDER_H
#define ORDER_H

#include "ThostFtdcUserApiDataType.h"
#include "ThostFtdcUserApiStruct.h"

class QString;

class Order {
    Order();

public:
    int vol;
    int vol_remain;
    TThostFtdcOrderRefType ref;
    int front;
    int session;
    QString instrument;
    TThostFtdcDirectionType direction;
    TThostFtdcOrderStatusType status;

    Order(const CThostFtdcOrderField &field);
    Order(const Order &other);

    bool isBuy() const {
        return direction == THOST_FTDC_D_Buy;
    }

    int remainVolume() const {
        if (isBuy()) {
            return vol_remain;
        } else {
            return -vol_remain;
        }
    }
};

#endif // ORDER_H
