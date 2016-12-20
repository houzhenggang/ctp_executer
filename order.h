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
};

#endif // ORDER_H
