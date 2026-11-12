#ifndef STARTUP_HOST_DEVCORE_H
#define STARTUP_HOST_DEVCORE_H
#include <cassert>
#include <cstdint>
#include <cstring>
#define COLOR_24BIT
using color_t = uint32_t;
enum { COLOR_BLACK = 0, COLOR_WHITE = 0xFFFFFF, COLOR_GREY = 0x808080,
       COLOR_DARKGREY = 0x404040 };
namespace Host { extern uint32_t now_ms; extern int shown; extern int hidden; }
struct Font_8x12 { static Font_8x12& GetInstance() { static Font_8x12 f; return f; } };
struct Visual
{
  void Show(uint32_t) { ++Host::shown; }
  void Hide() { ++Host::hidden; }
};
struct ImagePalette : Visual
{
  void SetImage(const uint8_t* p, int32_t w, int32_t h, const color_t* palette)
  {
    assert(w == 256 && h == 256 && p && palette);
    for(int32_t i = 0; i < w*h; ++i) assert(p[i] < 64);
  }
  void Move(int32_t x, int32_t y) { assert(x == 32 && y == 96); }
};
struct String : Visual
{
  void SetParams(const char* s, int32_t x, int32_t y, color_t, Font_8x12&)
  {
    assert(x == (320 - int32_t(std::strlen(s))*8)/2);
    assert(y == 400 || y == 420);
  }
};
struct DisplayDrv
{
  color_t background = COLOR_DARKGREY;
  unsigned updates = 0;
  int32_t GetScreenW() { return 320; }
  int32_t GetScreenH() { return 480; }
  void SetBackgroundColor(color_t c) { background = c; }
  void InvalidateDisplay() {}
  void UpdateDisplay() { ++updates; }
};
struct RtosTick
{
  static uint32_t GetTimeMs() { return Host::now_ms; }
  static bool CheckTimeDifferenceMs(uint32_t start, uint32_t period)
  { return uint32_t(Host::now_ms - start) >= period; }
  // Delays are deliberately absent: a Setup-time sleep would block the
  // real AppTask startup barrier and cannot be used by this implementation.
};
#endif
