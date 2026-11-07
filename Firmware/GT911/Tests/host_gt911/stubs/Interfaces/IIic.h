#pragma once
#include "DevCfg.h"
class IIic {
public:
 virtual ~IIic()=default;
 virtual Result Enable()=0;
 virtual Result Reset()=0;
 virtual Result IsDeviceReady(uint16_t,uint8_t=1)=0;
 virtual Result Transfer(uint16_t,uint8_t*,uint32_t,uint8_t*,uint32_t)=0;
 virtual Result Write(uint16_t,uint8_t*,uint32_t)=0;
 virtual void SetTxTimeout(uint16_t) {}
 virtual void SetRxTimeout(uint16_t) {}
};
