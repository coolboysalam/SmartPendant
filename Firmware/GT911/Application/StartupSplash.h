#ifndef OMID_STARTUP_SPLASH_H
#define OMID_STARTUP_SPLASH_H

class DisplayDrv;

// Begin in Setup, then advance from the application timer. AppTask has a
// shared startup barrier: delaying inside Setup would prevent ALL task loops.
void BeginStartupSplash(DisplayDrv& display);
// True while the splash is visible; false once it has been removed.
bool UpdateStartupSplash(DisplayDrv& display);

#endif
