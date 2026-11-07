#include <cassert>
#include <algorithm>
#include <iostream>
#include <vector>
#include "AutoTouch.h"
#include "FreeRtosWrapper/RtosTick.h"
struct GPIOAction {int port;int pin;int val;};
std::vector<GPIOAction> logpins;
std::vector<uint32_t> delays;
void HAL_GPIO_WritePin(GPIO_TypeDef* p,uint16_t pin, GPIO_PinState v){ logpins.push_back({p->port,pin,(int)v}); }
void HAL_GPIO_Init(GPIO_TypeDef* p,GPIO_InitTypeDef* g){logpins.push_back({p->port,(int)g->Pin,(int)g->Mode+10});}
void RtosTick::DelayMs(uint32_t x){delays.push_back(x);}
struct MockIic : IIic {
 bool has_ft=false, has_5d=true,has_14=false, fail=false;
 int resets=0,acks=0; uint8_t status=0;uint16_t x=0,y=0;
 std::vector<uint16_t> reads;
 Result Enable() override {return Result::RESULT_OK;}
 Result Reset() override {resets++;return Result::RESULT_OK;}
 Result IsDeviceReady(uint16_t addr,uint8_t) override {
   return ((addr==0x38 && has_ft)||(addr==0x5d && has_5d)||(addr==0x14 && has_14)) ? Result::RESULT_OK : Result::ERR_I2C_TIMEOUT;
 }
 Result Transfer(uint16_t addr,uint8_t* tx,uint32_t len,uint8_t* rx,uint32_t count) override {
    if(fail) return Result::ERR_I2C_TIMEOUT;
    assert(len==2); assert(addr==0x5d || addr==0x14);
    uint16_t reg=tx[0] | (uint16_t(tx[1])<<8);
    reads.push_back(reg);
    std::fill(rx,rx+count,0);
    if(reg==0x8140) { assert(count==4); rx[0]='9';rx[1]='1';rx[2]='1';rx[3]=0; }
    if(reg==0x814e) { assert(count==1);rx[0]=status; }
    if(reg==0x814f) { assert(count==7);rx[0]=0;rx[1]=x&255;rx[2]=x>>8;rx[3]=y&255;rx[4]=y>>8;}
    return Result::RESULT_OK;
 }
 Result Write(uint16_t addr,uint8_t* dat,uint32_t size) override {
   assert(addr==0x5d || addr==0x14);
   assert(size==3 && dat[0]==0x81 && dat[1]==0x4e && dat[2]==0);
   acks++;status=0; return fail ? Result::ERR_I2C_TIMEOUT : Result::RESULT_OK;
 }
};
int main() {
 GPIO_TypeDef b{1};int32_t x=0,y=0;
 {
   MockIic i;GT911 gt(i,&b,1<<5,&b,1<<6,ITouchscreen::ROTATION_LEFT,320,480);
   gt.SetRotation(ITouchscreen::ROTATION_TOP);
   assert(gt.Init().IsGood());assert(gt.Address()==0x5d);assert(!gt.IsTouched());
   i.x=112;i.y=221;i.status=0x81;
   assert(gt.GetXY(x,y));assert(x==221 && y==208);
   assert(gt.IsTouched() && i.acks==1);
   assert(gt.GetXY(x,y) && i.acks==1);
   i.status=0x80;assert(!gt.GetXY(x,y));assert(!gt.IsTouched());assert(i.acks==2);
   assert(i.reads.front()==0x8140);assert(i.reads[1]==0x814e);
   assert(delays.size()==3 && delays[0]==20 && delays[1]==60 && delays[2]==50);
   assert(logpins.size()>=4);
 }
 {
   MockIic i;i.has_5d=false;i.has_14=true;
   GT911 gt(i,&b,1<<5,&b,1<<6);
   assert(gt.Init().IsGood() && gt.Address()==0x14);
   i.status=0x82;i.x=5;i.y=6;
   assert(gt.GetRawXY(x,y) && x==5 && y==6);
   i.status=0x8f;assert(!gt.GetRawXY(x,y));assert(i.resets==1);
 }
 {
   MockIic i;i.has_ft=true;logpins.clear();delays.clear();
   AutoTouch auto_touch(i,&b,1<<5,&b,1<<6,ITouchscreen::ROTATION_LEFT,320,480);
   assert(auto_touch.Init().IsGood());assert(!auto_touch.UsingGT911());
   assert(logpins.empty() && delays.empty());
 }
 {
   MockIic i;i.has_ft=false;
   AutoTouch auto_touch(i,&b,1<<5,&b,1<<6,ITouchscreen::ROTATION_LEFT,320,480);
   assert(auto_touch.Init().IsGood());assert(auto_touch.UsingGT911());
   i.status=0x81;i.x=200;i.y=100;
   assert(auto_touch.GetXY(x,y));assert(x==100 && y==120);
   i.fail=true;assert(!auto_touch.GetXY(x,y) && !auto_touch.IsTouched());
 }
 std::cout << "PASS: ID/address, endian read, first point, release, cached status, reset, FT fallback, I2C error\n";
}
