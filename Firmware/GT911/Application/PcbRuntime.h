#ifndef OMID_PCB_RUNTIME_H
#define OMID_PCB_RUNTIME_H
#include "PcbHeightMap.h"
bool PcbReadContext(Pcb::Context& context,int32_t* work = nullptr);
bool PcbJobAllowed(uint32_t hash);
void PcbBindMap(Pcb::HeightMap* map);
void PcbInvalidateSession();
// Returns true for the protected generated-job signature, even if malformed.
bool PcbReadJobHeader(const char* line,uint32_t& hash);
#endif
