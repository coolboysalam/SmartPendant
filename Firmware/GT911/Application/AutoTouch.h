// Runtime selection between original FT6236 and new GT911.
// SPDX-License-Identifier: BSD-3-Clause
#ifndef SMARTPENDANT_AUTOTOUCH_H
#define SMARTPENDANT_AUTOTOUCH_H
#include "Display/FT6236.h"
#include "GT911.h"
#include <atomic>

const char* PendantTouchStatus();
class AutoTouch final : public ITouchscreen
{
public:
  AutoTouch(IIic& bus, GPIO_TypeDef* reset_port, uint16_t reset_pin,
            GPIO_TypeDef* int_port, uint16_t int_pin,
            Rotation native_orientation, uint16_t native_width, uint16_t native_height);
  Result Init() override;
  bool IsTouched() override { return active_ && active_->IsTouched(); }
  bool GetRawXY(int32_t& x, int32_t& y) override
    { return active_ && active_->GetRawXY(x, y); }
  bool GetXY(int32_t& x, int32_t& y) override
    { return active_ && active_->GetXY(x, y); }
  void SetRotation(Rotation rot) override;
  Result SetCalibrationConsts(int32_t kx, int32_t ky, int32_t bx, int32_t by) override
  {
    if(active_) return active_->SetCalibrationConsts(kx, ky, bx, by);
    return Result::ERR_INVALID_ITEM;
  }
  const char* Status() const { return status_.load(); }
  bool UsingGT911() const { return active_ == &gt911_; }
private:
  std::atomic<const char*> status_{"Touch pending"};
  IIic& bus_;
  FT6236 ft6236_;
  GT911 gt911_;
  ITouchscreen* active_ = nullptr;
};
#endif
