#include "StartupSplash.h"
#include "DevCore.h"
#include "StartupLogoData.h"

namespace
{
struct SplashView
{
  ImagePalette logo;
  String title;
  String machine;
  uint32_t start_ms = 0u;
  bool clock_started = false;
};

SplashView& View()
{
  static SplashView view;
  return view;
}
}

void BeginStartupSplash(DisplayDrv& display)
{
  SplashView& view = View();
  const int32_t screen_w = display.GetScreenW();
  const int32_t screen_h = display.GetScreenH();
  display.SetBackgroundColor(COLOR_BLACK);

  // Pixel indices and palette remain in flash; no full-screen RAM allocation.
  view.logo.SetImage(STARTUP_LOGO_PIXELS, STARTUP_LOGO_WIDTH,
                      STARTUP_LOGO_HEIGHT, STARTUP_LOGO_PALETTE);
  view.logo.Move((screen_w - STARTUP_LOGO_WIDTH) / 2,
                  (screen_h - STARTUP_LOGO_HEIGHT) / 2 - 16);
  view.title.SetParams("SmartPendant", (screen_w - 12 * 8) / 2,
                  screen_h - 80, COLOR_WHITE, Font_8x12::GetInstance());
  view.machine.SetParams("Voron Cascade", (screen_w - 13 * 8) / 2,
                    screen_h - 60, COLOR_GREY, Font_8x12::GetInstance());

  view.clock_started = false;
  view.logo.Show(100u);
  view.title.Show(101u);
  view.machine.Show(102u);
  display.InvalidateDisplay();
  display.UpdateDisplay();
}

bool UpdateStartupSplash(DisplayDrv& display)
{
  SplashView& view = View();
  if(!view.clock_started)
  {
    // Timer callbacks run only after every task has crossed the startup
    // barrier. Start the display interval here, not during initialization.
    view.start_ms = RtosTick::GetTimeMs();
    view.clock_started = true;
  }
  if(!RtosTick::CheckTimeDifferenceMs(view.start_ms, 2000u)) return true;

  // Removing visual objects uses the display line mutex. Their data stays
  // alive permanently, even if a display update is still finishing.
  view.machine.Hide();
  view.title.Hide();
  view.logo.Hide();
  display.SetBackgroundColor(COLOR_DARKGREY);
  display.InvalidateDisplay();
  return false;
}
