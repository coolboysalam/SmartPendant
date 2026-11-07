#pragma once
#include "DevCfg.h"
class ITouchscreen {
public:
 enum Rotation {ROTATION_TOP=0,ROTATION_LEFT,ROTATION_BOTTOM,ROTATION_RIGHT,ROTATION_CNT};
 virtual ~ITouchscreen() = default;
 virtual Result Init()=0;
 virtual bool IsTouched()=0;
 virtual bool GetRawXY(int32_t&,int32_t&)=0;
 virtual bool GetXY(int32_t&,int32_t&)=0;
 virtual Result SetCalibrationConsts(int32_t,int32_t,int32_t,int32_t) { return Result::ERR_NOT_IMPLEMENTED; }
 virtual void SetRotation(Rotation rot) { rotation = rot>orientation ? (Rotation)(rot-orientation) : (Rotation)(rot + ROTATION_CNT - orientation); }
protected:
 Rotation orientation=ROTATION_TOP,rotation=ROTATION_TOP;
 uint16_t width=0,height=0;
};
