// Runtime FT6236 / GT911 selection for SmartPendant.
// SPDX-License-Identifier: BSD-3-Clause
#include "AutoTouch.h"

AutoTouch::AutoTouch(IIic& bus, GPIO_TypeDef* reset_port, uint16_t reset_pin,
                     GPIO_TypeDef* int_port, uint16_t int_pin,
                     Rotation native_orientation, uint16_t native_width,
                     uint16_t native_height)
 : bus_(bus), ft6236_(bus, native_orientation, native_width, native_height),
   gt911_(bus, reset_port, reset_pin, int_port, int_pin,
          native_orientation, native_width, native_height)
{
  orientation = native_orientation;
  width = native_width;
  height = native_height;
  SetRotation(ROTATION_TOP);
}

Result AutoTouch::Init()
{
  active_ = nullptr;
  Result result = bus_.Enable();
  if(result.IsBad())
  {
    status_.store("I2C error");
    return result;
  }
  if(bus_.IsDeviceReady(0x38u, 1u).IsGood())
  {
    result = ft6236_.Init();
    if(result.IsGood())
    {
      active_ = &ft6236_;
      status_.store("FT6236");
      return result;
    }
  }
  result = gt911_.Init();
  if(result.IsGood())
  {
    active_ = &gt911_;
    status_.store(gt911_.Address() == 0x5Du ? "GT911 0x5D" : "GT911 0x14");
  }
  else
  {
    status_.store("No touch");
  }
  return result;
}

void AutoTouch::SetRotation(Rotation rot)
{
  ITouchscreen::SetRotation(rot);
  ft6236_.SetRotation(rot);
  gt911_.SetRotation(rot);
}
