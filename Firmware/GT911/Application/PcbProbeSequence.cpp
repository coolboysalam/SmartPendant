#include "PcbProbeSequence.h"
#include <algorithm>
#include <cstdlib>
namespace Pcb
{
unsigned ProbeSequence::Row() const { return verify ? 0:point/map->nx; }
unsigned ProbeSequence::Col() const
{
  if(verify) return 0;
  unsigned col=point%map->nx;
  return (Row()%2) ? map->nx-1-col:col;
}
bool ProbeSequence::Begin(HeightMap& target,const Context& ctx,int32_t current_z,
                           const ProbeSettings& config,bool verify_only)
{
  map=&target; expected=ctx; settings=config; verify=verify_only;
  error=nullptr; pending=false; point=0; safe=current_z; lower=safe-settings.travel;
  if(settings.clearance<500||settings.clearance>20000||settings.travel<100||
     settings.travel>20000||settings.feed<5||settings.feed>300||
     (verify && (!map->Valid()||!map->context.Matches(ctx,false))))
  { Abort("Map context or probe settings invalid"); return false; }
  if(!verify && (map->nx<2||map->ny<2)) { Abort("Grid is not configured"); return false; }
  if(!verify) map->Invalidate();
  state=Safe; return true;
}
void ProbeSequence::Abort(const char* reason)
{
  error=reason; pending=false; state=Finished;
  if(map && !verify) map->Invalidate();
}
bool ProbeSequence::Next(ProbeCommand& command)
{
  if(error||Done()||pending) return false;
  command={};
  switch(state)
  {
    case Safe: case Exit: command.kind=ProbeCommand::Raise; command.z=safe; break;
    case XY: command.kind=ProbeCommand::Travel; command.x=map->X(Col()); command.y=map->Y(Row()); break;
    case Fast: command.kind=ProbeCommand::Touch; command.z=lower; command.feed=settings.feed; break;
    case Release: command.kind=ProbeCommand::Raise; command.z=std::min(safe,fast_z+500); break;
    case Slow: command.kind=ProbeCommand::Touch; command.z=std::max(lower,fast_z-500); command.feed=std::max<uint32_t>(5u,settings.feed/3); break;
    case Park: command.kind=ProbeCommand::Travel; command.x=map->X(0); command.y=map->Y(0); break;
    case Finished: return false;
  }
  last=command; pending=true; return true;
}
bool ProbeSequence::Accept(const ProbeObservation& observation)
{
  if(!pending||error) return false;
  if(!observation.context.Matches(expected)||!observation.acknowledged||
     !observation.fresh_status||!observation.idle)
  { Abort("Command or coordinate context changed"); return false; }
  if(last.kind==ProbeCommand::Touch)
  {
    if(!observation.probe_report||!observation.probe_success)
    { Abort("Fresh successful probe report missing"); return false; }
    if(std::abs(int64_t(observation.probe[0])-map->X(Col()))>10 ||
       std::abs(int64_t(observation.probe[1])-map->Y(Row()))>10 ||
       observation.probe[2]<last.z-20 || observation.probe[2]>safe-200)
    { Abort("Unexpected contact or insufficient clearance"); return false; }
  }
  else if(observation.probe_pin)
  { Abort("Probe remains triggered after retract"); return false; }
  pending=false;
  switch(state)
  {
    case Safe: state=XY; break;
    case XY: state=Fast; break;
    case Fast:
      fast_z=observation.probe[2];
      if(fast_z-lower<50) { Abort("Contact too close to probe limit"); return false; }
      state=Release; break;
    case Release: state=Slow; break;
    case Slow:
      if(std::abs(int64_t(observation.probe[2])-fast_z)>50)
      { Abort("Fast and slow contacts differ >0.05 mm"); return false; }
      if(verify)
      {
        if(std::abs(int64_t(observation.probe[2])-map->Height(0,0))>20)
        { Abort("Reference moved >0.02 mm; scan again"); return false; }
      }
      else if(!map->Set(Col(),Row(),observation.probe[2]))
      { Abort("Invalid or repeated grid sample"); return false; }
      if(point==0)
      {
        safe=std::max(safe,observation.probe[2]+settings.clearance);
        lower=safe-settings.travel;
      }
      ++point; state=Exit; break;
    case Exit: state=point>=Total() ? Park:XY; break;
    case Park:
      if(!verify && !map->Complete()) { Abort("Grid is incomplete"); return false; }
      map->context.epoch=expected.epoch;
      state=Finished; break;
    case Finished: return false;
  }
  return true;
}
}
