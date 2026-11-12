#ifndef OMID_PCB_FILES_H
#define OMID_PCB_FILES_H
#include "PcbHeightMap.h"
#include "fatfs.h"
bool PcbSaveMap(const Pcb::HeightMap& map);
bool PcbLoadMap(Pcb::HeightMap& map);
class PcbFileProcessor
{
public:
  static constexpr const char* JOB_NAME = "PCB_LEV.NC";
  explicit PcbFileProcessor(const Pcb::HeightMap& map,int32_t clearance);
  ~PcbFileProcessor();
  bool Open(const char* input);
  // Bounded work per application timer callback; no physical motion.
  bool Tick(unsigned budget = 24);
  bool Done() const { return done; }
  const char* Error() const { return error; }
  uint32_t SourceLine() const { return source_line; }
  uint32_t OutputLines() const { return output_lines; }
  void Cancel();
private:
  const Pcb::HeightMap& map;
  Pcb::Compensator engine;
  FIL source={},target={};
  bool source_open=false,target_open=false,done=false;
  uint32_t source_line=0,output_lines=0;
  int32_t clearance;
  const char* error=nullptr;
  bool Write(const char* text);
  bool Fail(const char* message);
  bool Publish();
};
#endif
