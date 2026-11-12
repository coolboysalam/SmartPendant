#include "PcbFiles.h"
#include "PcbRuntime.h"
#include <cstdio>
#include <cstring>
namespace
{
const char* MAP="PCB_MAP.CSV";
const char* MAP_TMP="PCB_MAP.TMP";
const char* MAP_BACKUP="PCB_MAP.BAK";
const char* JOB_TMP="PCB_TMP.NC";
const char* JOB_BACKUP="PCB_LEV.BAK";
bool ReplaceFile(const char* temporary,const char* final,const char* backup)
{
  FILINFO info={};
  FRESULT exists=f_stat(final,&info);
  if(exists!=FR_OK&&exists!=FR_NO_FILE) return false;
  bool old=exists==FR_OK;
  if(old)
  {
    FRESULT removed=f_unlink(backup);
    if(removed!=FR_OK&&removed!=FR_NO_FILE) return false;
    if(f_rename(final,backup)!=FR_OK) return false;
  }
  if(f_rename(temporary,final)==FR_OK) return true;
  if(old) f_rename(backup,final);
  return false;
}
Pcb::Position Initial(const Pcb::HeightMap& map,int32_t clearance)
{
  return {map.x0/1000.0,map.y0/1000.0,
          map.MaxZ()+clearance/1000.0-map.Height(0,0)/1000.0};
}
}
bool PcbSaveMap(const Pcb::HeightMap& map)
{
  if(!map.Valid()) return false;
  FIL file={};
  if(f_open(&file,MAP_TMP,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK) return false;
  bool ok=true; char line[96];
  for(unsigned i=0;i<map.RecordCount()&&ok;++i)
  {
    UINT written=0;
    ok=map.WriteRecord(i,line,sizeof(line));
    if(ok) ok=f_write(&file,line,std::strlen(line),&written)==FR_OK&&written==std::strlen(line);
  }
  if(ok) ok=f_sync(&file)==FR_OK;
  if(f_close(&file)!=FR_OK) ok=false;
  if(ok) ok=ReplaceFile(MAP_TMP,MAP,MAP_BACKUP);
  if(!ok) f_unlink(MAP_TMP);
  return ok;
}
bool PcbLoadMap(Pcb::HeightMap& map)
{
  PcbInvalidateSession(); map.Invalidate();
  FIL file={}; if(f_open(&file,MAP,FA_READ)!=FR_OK) return false;
  unsigned record=0; char line[96]; bool ok=true;
  while(f_gets(line,sizeof(line),&file))
  {
    if(record> Pcb::HeightMap::MAX_POINTS+3 || !map.ReadRecord(record++,line)) { ok=false; break; }
  }
  ok=ok && f_eof(&file) && map.Valid() && record==map.RecordCount();
  if(f_close(&file)!=FR_OK) ok=false;
  if(!ok) map.Invalidate();
  return ok;
}
PcbFileProcessor::PcbFileProcessor(const Pcb::HeightMap& input,int32_t lift)
 : map(input),engine(input,Initial(input,lift)),clearance(lift) {}
PcbFileProcessor::~PcbFileProcessor() { Cancel(); }
void PcbFileProcessor::Cancel()
{
  if(source_open) { f_close(&source); source_open=false; }
  if(target_open) { f_close(&target); target_open=false; f_unlink(JOB_TMP); }
}
bool PcbFileProcessor::Fail(const char* message) { error=message; Cancel(); return false; }
bool PcbFileProcessor::Write(const char* text)
{
  UINT written=0; size_t len=std::strlen(text);
  if(f_write(&target,text,len,&written)!=FR_OK||written!=len) return Fail("SD write failed");
  return true;
}
bool PcbFileProcessor::Open(const char* name)
{
  if(!map.Valid()||!PcbJobAllowed(map.Hash())) return Fail("Map is not verified or context changed");
  if(!std::strcmp(name,JOB_NAME)||!std::strcmp(name,JOB_TMP)) return Fail("Cannot compensate output twice");
  if(f_open(&source,name,FA_READ)!=FR_OK) return Fail("Cannot read source file");
  source_open=true;
  char first[96]={}; uint32_t hash=0;
  if(f_gets(first,sizeof(first),&source)&&PcbReadJobHeader(first,hash)) return Fail("Source is already compensated");
  if(f_lseek(&source,0)!=FR_OK) return Fail("Cannot rewind source file");
  if(f_open(&target,JOB_TMP,FA_CREATE_ALWAYS|FA_WRITE)!=FR_OK) return Fail("Cannot create output file");
  target_open=true;
  char header[96];
  std::snprintf(header,sizeof(header),";PCBHM1 %08lX\n;G54 single tool, source Z0 is copper\nG21G90G17G94G54\n",(unsigned long)map.Hash());
  if(!Write(header)) return false;
  if(std::strlen(name)>63) return Fail("Source filename exceeds 63 characters");
  std::snprintf(header,sizeof(header),";SOURCE:%s\n",name);
  if(!Write(header)) return false;
  int32_t safe=int32_t(map.MaxZ()*1000.0+0.5)+clearance;
  auto format=[](char* out,size_t size,int32_t v)
  { int64_t m=v<0 ? -int64_t(v):v; std::snprintf(out,size,"%s%ld.%03ld",v<0 ? "-":"",long(m/1000),long(m%1000)); };
  char x[24],y[24],z[24]; format(x,sizeof(x),map.x0); format(y,sizeof(y),map.y0); format(z,sizeof(z),safe);
  std::snprintf(header,sizeof(header),"G0Z%s\nG0X%sY%s\n",z,x,y);
  return Write(header);
}
bool PcbFileProcessor::Publish()
{
  if(!PcbJobAllowed(map.Hash())) return Fail("Coordinates changed while preparing file");
  if(f_sync(&target)!=FR_OK) return Fail("Cannot sync output to SD");
  FRESULT close_target=f_close(&target); target_open=false;
  FRESULT close_source=f_close(&source); source_open=false;
  if(close_target!=FR_OK||close_source!=FR_OK) { f_unlink(JOB_TMP); return Fail("SD close failed"); }
  if(!ReplaceFile(JOB_TMP,JOB_NAME,JOB_BACKUP)) { f_unlink(JOB_TMP); return Fail("Cannot publish output file"); }
  done=true; return true;
}
bool PcbFileProcessor::Tick(unsigned budget)
{
  if(error||done) return error==nullptr;
  if(!source_open||!target_open) return Fail("No input/output file");
  if(!PcbJobAllowed(map.Hash())) return Fail("Map context changed; output discarded");
  char output[80],input[192];
  while(budget--)
  {
    int next=engine.Next(output,sizeof(output));
    if(next<0) return Fail(engine.Error());
    if(next>0)
    {
      if(++output_lines>9999900u) return Fail("Output exceeds controller line limit");
      if(!Write(output)||!Write("\n")) return false;
      continue;
    }
    if(!f_gets(input,sizeof(input),&source))
    {
      if(!f_eof(&source)) return Fail("SD read failed");
      return Publish();
    }
    ++source_line;
    if(!std::strchr(input,'\n')&&!f_eof(&source)) return Fail("Source line exceeds 190 characters");
    if(!engine.BeginLine(input)) return Fail(engine.Error());
  }
  return true;
}
