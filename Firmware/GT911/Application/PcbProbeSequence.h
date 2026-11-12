#ifndef OMID_PCB_PROBE_SEQUENCE_H
#define OMID_PCB_PROBE_SEQUENCE_H
#include "PcbHeightMap.h"
namespace Pcb
{
struct ProbeSettings
{
  int32_t clearance = 1000; // um above the first contact
  int32_t travel = 2000;    // bounded probe travel from the safe plane
  uint32_t feed = 30;       // mm/min, slow touch uses one third
};
struct ProbeCommand
{
  enum Kind { Raise, Travel, Touch } kind = Raise;
  int32_t x=0,y=0,z=0;
  uint32_t feed=0;
};
struct ProbeObservation
{
  Context context;
  bool acknowledged=false, fresh_status=false, idle=false;
  bool probe_report=false, probe_success=false, probe_pin=false;
  int32_t probe[3]={}; // work coordinates in um
};
class ProbeSequence
{
public:
  bool Begin(HeightMap& map,const Context& ctx,int32_t current_z,
             const ProbeSettings& settings,bool verify_only);
  bool Next(ProbeCommand& command);
  bool Accept(const ProbeObservation& observation);
  void Abort(const char* reason);
  bool Done() const { return state==Finished; }
  bool Failed() const { return error!=nullptr; }
  const char* Error() const { return error; }
  unsigned Point() const { return point; }
  unsigned Total() const { return verify ? 1:map->Count(); }
private:
  enum State { Safe, XY, Fast, Release, Slow, Exit, Park, Finished } state=Finished;
  HeightMap* map=nullptr;
  Context expected;
  ProbeSettings settings;
  bool verify=false,pending=false;
  unsigned point=0;
  int32_t safe=0,lower=0,fast_z=0;
  ProbeCommand last;
  const char* error=nullptr;
  unsigned Col() const;
  unsigned Row() const;
};
}
#endif
