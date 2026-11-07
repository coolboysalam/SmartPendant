#pragma once
#include "Interfaces/ITouchscreen.h"
#include "Interfaces/IIic.h"
struct FT6236 : ITouchscreen {
 FT6236(IIic&, Rotation orient=ROTATION_TOP,uint16_t w=320,uint16_t h=480) {orientation=orient;width=w;height=h;SetRotation(ROTATION_TOP);}
 Result Init() override {return Result::RESULT_OK;}
 bool IsTouched() override {return false;}
 bool GetRawXY(int32_t&,int32_t&) override {return false;}
 bool GetXY(int32_t&,int32_t&) override {return false;}
};
