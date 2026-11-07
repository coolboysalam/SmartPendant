// GT911 touchscreen driver adapted to DevCore IIic / ITouchscreen.
// SPDX-License-Identifier: BSD-3-Clause
#include "GT911.h"
#include "FreeRtosWrapper/RtosTick.h"

namespace {
constexpr uint16_t REG_PRODUCT_ID = 0x8140u;
constexpr uint16_t REG_STATUS     = 0x814Eu;
constexpr uint16_t REG_POINT1     = 0x814Fu;
}

GT911::GT911(IIic& bus, GPIO_TypeDef* reset_port, uint16_t reset_pin,
             GPIO_TypeDef* int_port, uint16_t int_pin,
             Rotation native_orientation, uint16_t native_width, uint16_t native_height)
  : bus_(bus), reset_port_(reset_port), reset_pin_(reset_pin),
    int_port_(int_port), int_pin_(int_pin)
{
  orientation = native_orientation;
  width = native_width;
  height = native_height;
  SetRotation(ROTATION_TOP);
}

void GT911::ResetForAddress5D()
{
  // GT911 samples INT during reset. LOW selects 7-bit 0x5D.
  // PB6 is driven only if probing the original FT6236 failed.
  HAL_GPIO_WritePin(reset_port_, reset_pin_, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(int_port_, int_pin_, GPIO_PIN_RESET);
  GPIO_InitTypeDef gpio = {};
  gpio.Pin = int_pin_;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(int_port_, &gpio);
  RtosTick::DelayMs(20u);
  HAL_GPIO_WritePin(reset_port_, reset_pin_, GPIO_PIN_SET);
  RtosTick::DelayMs(60u);
  gpio.Mode = GPIO_MODE_INPUT;
  gpio.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(int_port_, &gpio);
  RtosTick::DelayMs(50u);
}

Result GT911::ReadReg(uint16_t reg, uint8_t* dest, uint32_t size)
{
  // DevCore StHalIicThreadSafe::Transfer expects the register argument
  // low byte first and then invokes HAL_I2C_Mem_Read (16-bit address).
  uint8_t addr[2] = {static_cast<uint8_t>(reg), static_cast<uint8_t>(reg >> 8u)};
  return bus_.Transfer(address_, addr, sizeof(addr), dest, size);
}

Result GT911::WriteReg8(uint16_t reg, uint8_t value)
{
  // IIic::Write sends bytes as given, hence 16-bit address high byte first.
  uint8_t data[3] = {static_cast<uint8_t>(reg >> 8u),
                     static_cast<uint8_t>(reg), value};
  return bus_.Write(address_, data, sizeof(data));
}

Result GT911::Init()
{
  initialized_ = false;
  points_ = 0u;
  bus_.SetTxTimeout(15u);
  bus_.SetRxTimeout(15u);
  Result result = bus_.Enable();
  if(result.IsBad()) return result;

  ResetForAddress5D();

  const uint8_t candidates[2] = {0x5Du, 0x14u};
  for(uint8_t candidate : candidates)
  {
    address_ = candidate;
    if(bus_.IsDeviceReady(address_, 2u).IsBad()) continue;
    uint8_t product[4] = {};
    if(ReadReg(REG_PRODUCT_ID, product, sizeof(product)).IsGood() &&
       product[0] == '9' && product[1] == '1' && product[2] == '1')
    {
      initialized_ = true;
      return Result::RESULT_OK;
    }
  }
  return Result::ERR_INVALID_ITEM;
}

bool GT911::IsTouched() { return initialized_ && points_ != 0u; }

bool GT911::Poll()
{
  if(!initialized_) return false;
  uint8_t status = 0u;
  if(ReadReg(REG_STATUS, &status, 1u).IsBad())
  {
    points_ = 0u;
    bus_.Reset();
    return false;
  }
  if((status & 0x80u) == 0u) return points_ != 0u;
  uint8_t n = status & 0x0Fu;
  bool valid = n <= 5u;
  uint16_t new_x = raw_x_, new_y = raw_y_;
  if(valid && n != 0u)
  {
    // GT911 point 1 starts at 0x814F: track ID, X low/high, Y low/high.
    uint8_t point[7] = {};
    if(ReadReg(REG_POINT1, point, sizeof(point)).IsGood())
    {
      new_x = static_cast<uint16_t>(point[1] | (point[2] << 8u));
      new_y = static_cast<uint16_t>(point[3] | (point[4] << 8u));
    }
    else valid = false;
  }
  if(WriteReg8(REG_STATUS, 0u).IsBad()) valid = false;
  if(!valid)
  {
    points_ = 0u;
    bus_.Reset();
    return false;
  }
  points_ = n;
  if(n != 0u) { raw_x_ = new_x; raw_y_ = new_y; }
  return points_ != 0u;
}

bool GT911::GetRawXY(int32_t& x, int32_t& y)
{
  if(!Poll()) return false;
  x = raw_x_;
  y = raw_y_;
  return true;
}

bool GT911::GetXY(int32_t& x, int32_t& y)
{
  if(!GetRawXY(x, y)) return false;
  // Same rotation rules as DevCore FT6236 (verify real panel orientation).
  int32_t temp;
  switch(rotation)
  {
    case ROTATION_BOTTOM: x = width - x; y = height - y; break;
    case ROTATION_RIGHT: temp = y; y = width - x; x = temp; break;
    case ROTATION_LEFT: temp = x; x = height - y; y = temp; break;
    case ROTATION_TOP: default: break;
  }
  return true;
}
