#include "PcbRuntime.h"
#include "GrblComm.h"
#include <cstring>
#include <cstdlib>
namespace { Pcb::HeightMap* active_map=nullptr; }
bool PcbReadContext(Pcb::Context& context,int32_t* work)
{
  int32_t local[3]={};
  return GrblComm::GetInstance().GetPcbContext(work ? work:local,context.offset,
                                              context.tool,context.tlo,context.epoch);
}
void PcbBindMap(Pcb::HeightMap* map) { active_map=map; }
void PcbInvalidateSession() { active_map=nullptr; }
bool PcbJobAllowed(uint32_t hash)
{
  Pcb::Context context;
  if(!active_map||!active_map->Valid()||active_map->Hash()!=hash||!PcbReadContext(context)||
     !active_map->context.Matches(context)||!GrblComm::GetInstance().PcbMachineReady())
  { active_map=nullptr; return false; }
  return true;
}
bool PcbReadJobHeader(const char* line,uint32_t& hash)
{
  hash=0;
  if(std::strncmp(line,";PCBHM1",7)) return false;
  if(line[7]!=' ') return true;
  const char* p=line+8;
  uint32_t parsed=0;
  for(unsigned i=0;i<8;++i)
  {
    char c=p[i]; unsigned v;
    if(c>='0'&&c<='9') v=c-'0';
    else if(c>='A'&&c<='F') v=c-'A'+10;
    else if(c>='a'&&c<='f') v=c-'a'+10;
    else return true;
    parsed=(parsed<<4)|v;
  }
  if(!p[8] || p[8]=='\r' || p[8]=='\n') hash=parsed;
  return true;
}
