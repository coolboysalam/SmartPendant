#include "DevCore.h"
#include "StartupSplash.h"
#include <cstdio>
namespace Host { uint32_t now_ms = 100u; int shown = 0; int hidden = 0; }
int main()
{
  DisplayDrv display;
  BeginStartupSplash(display);
  assert(Host::now_ms == 100u && Host::shown == 3 && Host::hidden == 0);
  assert(display.background == COLOR_BLACK);
  // All other tasks may take arbitrarily long to initialize. The interval
  // must start after the barrier, at the first application timer callback.
  Host::now_ms = 10000u;
  assert(UpdateStartupSplash(display));
  Host::now_ms = 11999u;
  assert(UpdateStartupSplash(display) && Host::hidden == 0);
  Host::now_ms = 12000u;
  assert(!UpdateStartupSplash(display) && Host::hidden == 3);
  assert(display.background == COLOR_DARKGREY);

  BeginStartupSplash(display);
  Host::now_ms = UINT32_MAX - 500u;
  assert(UpdateStartupSplash(display));
  Host::now_ms = 1498u;
  assert(UpdateStartupSplash(display));
  Host::now_ms = 1499u;
  assert(!UpdateStartupSplash(display) && Host::hidden == 6);
  std::puts("PASS: no startup delay, post-barrier 2000ms interval, removal, clock wrap, indexed image bounds");
}
