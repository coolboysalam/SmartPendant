// GT911 capacitive touchscreen driver for SmartPendant.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef SMARTPENDANT_GT911_H
#define SMARTPENDANT_GT911_H

#include "Interfaces/ITouchscreen.h"
#include "Interfaces/IIic.h"
#include "main.h"

class GT911 final : public ITouchscreen
{
public:
  GT911(IIic& bus, GPIO_TypeDef* reset_port, uint16_t reset_pin,
        GPIO_TypeDef* int_port, uint16_t int_pin,
        Rotation native_orientation = ROTATION_TOP,
        uint16_t native_width = 320u, uint16_t native_height = 480u);

  Result Init() override;
  bool IsTouched() override;
  bool GetRawXY(int32_t& x, int32_t& y) override;
  bool GetXY(int32_t& x, int32_t& y) override;
  Result SetCalibrationConsts(int32_t, int32_t, int32_t, int32_t) override
    { return Result::ERR_NOT_IMPLEMENTED; }

  uint8_t Address() const { return address_; }

private:
  Result ReadReg(uint16_t reg, uint8_t* dest, uint32_t size);
  Result WriteReg8(uint16_t reg, uint8_t value);
  bool Poll();
  void ResetForAddress5D();

  IIic& bus_;
  GPIO_TypeDef* reset_port_;
  uint16_t reset_pin_;
  GPIO_TypeDef* int_port_;
  uint16_t int_pin_;
  uint8_t address_ = 0x5Du;
  bool initialized_ = false;
  uint8_t points_ = 0u;
  uint16_t raw_x_ = 0u;
  uint16_t raw_y_ = 0u;
};

#endif
