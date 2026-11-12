#include "PcbMapScr.h"
#include "PcbRuntime.h"
#include "Application.h"
#include "ProbeScr.h"
#include <new>
#include <cstdio>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace
{
const char* LABELS[]={"Width X","Length Y","Points X","Points Y","Retract","Probe travel","Probe feed"};
bool MountCard()
{
  BSP_SD_Init();
  return f_mount(&SDFatFS,(TCHAR const*)SDPath,0)==FR_OK;
}
void Millimetres(char* out,size_t size,int32_t value)
{
  int64_t m=value<0 ? -int64_t(value):value;
  std::snprintf(out,size,"%s%ld.%03ld",value<0 ? "-":"",long(m/1000),long(m%1000));
}
void Wrap(char* out,size_t size,const char* text)
{
  size_t used=0; unsigned col=0;
  while(*text&&used+1<size)
  {
    if(*text=='\n') { out[used++]='\n'; col=0; ++text; continue; }
    if(*text==' ') { if(col&&used+1<size) { out[used++]=' '; ++col; } ++text; continue; }
    size_t word=0; while(text[word]&&text[word]!=' '&&text[word]!='\n') ++word;
    if(col&&col+word>38) { out[used++]='\n'; col=0; }
    for(size_t i=0;i<word&&used+1<size;++i)
    {
      if(col>=38&&used+1<size) { out[used++]='\n'; col=0; }
      if(used+1<size) { out[used++]=text[i]; ++col; }
    }
    text+=word;
  }
  out[used]=0;
}
bool ProgramFile(const FILINFO& info)
{
  if(info.fattrib&AM_DIR) return false;
  if(std::strlen(info.fname)>=64) return false;
  char lower[64]; unsigned i=0;
  for(;info.fname[i];++i) lower[i]=std::tolower((unsigned char)info.fname[i]);
  lower[i]=0;
  if(!std::strcmp(lower,"pcb_lev.nc")||!std::strcmp(lower,"pcb_tmp.nc")) return false;
  const char* dot=std::strrchr(lower,'.');
  return dot&&(!std::strcmp(dot,".nc")||!std::strcmp(dot,".ngc")||!std::strcmp(dot,".gcode"));
}
}
PcbMapScr* PcbMapScr::GetInstance()
{
  // Allocate from the RTOS heap: F411 has little unallocated static RAM.
  static PcbMapScr* instance=nullptr;
  if(!instance) instance=new(std::nothrow) PcbMapScr;
  return instance;
}
Result PcbMapScr::Setup(int32_t y,int32_t)
{
  for(unsigned i=0;i<PARAM_COUNT;++i)
  {
    int32_t x=4+(i%2)*158,top=y+4+(i/2)*52;
    captions[i].SetParams(LABELS[i],x,top,COLOR_WHITE,Font_8x12::GetInstance());
    params[i].SetParams(x,top+13,154,34,7,(i<2||i==CLEARANCE||i==TRAVEL)?3:0);
    params[i].SetBorder(2,COLOR_DARKBLUE);
    params[i].SetDataFont(Font_12x16::GetInstance());
    params[i].SetUnits(i==FEED ? "mm/m":((i==COLS||i==ROWS)?"":"mm"),DataWindow::RIGHT,Font_6x8::GetInstance());
    params[i].SetCallback(AppTask::GetCurrent());
  }
  origin_btn.SetParams("Set XY here",162,y+173,154,34);
  load_btn.SetParams("Load map",4,y+216,101,36);
  verify_btn.SetParams("Verify",109,y+216,101,36);
  file_btn.SetParams("PCB file",214,y+216,102,36);
  for(UiButton* button : {&origin_btn,&load_btn,&verify_btn,&file_btn}) button->SetCallback(AppTask::GetCurrent());
  status.SetParams(rendered_status,4,y+262,COLOR_WHITE,Font_8x12::GetInstance());
  files.SetCallback(AppTask::GetCurrent(),this,reinterpret_cast<CallbackPtr>(SelectFile),reinterpret_cast<CallbackPtr>(CancelFile));
  for(unsigned i=0;i<MAX_FILES;++i) { items[i].text=file_names[i]; items[i].n=sizeof(file_names[i]); }
  files.Setup(items,MAX_FILES,0,y,320,410-y);
  if(!status_text[0]) std::snprintf(status_text,sizeof(status_text),"G54 / mm / XYZ; home + soft limits.\nZ above PCB. Clip tool to probe.\nScan uses two touches per point.");
  return Result::RESULT_OK;
}
void PcbMapScr::Refresh()
{
  bool busy=phase!=Idle;
  for(unsigned i=0;i<PARAM_COUNT;++i) { params[i].SetNumber(values[i]); params[i].SetActive(!busy); }
  for(UiButton* b : {&origin_btn,&load_btn,&verify_btn,&file_btn}) { if(busy) b->Disable(); else b->Enable(); }
  Application::GetInstance().GetLeftButton().SetString(busy ? "Cancel":"Scan");
  char wrapped[192]; Wrap(wrapped,sizeof(wrapped),status_text);
  status.SetString(rendered_status,sizeof(rendered_status),"%s",wrapped);
}
Result PcbMapScr::Show()
{
  visible=true;
  for(unsigned i=0;i<PARAM_COUNT;++i) { captions[i].Show(2100); params[i].Show(2101); }
  for(UiButton* b : {&origin_btn,&load_btn,&verify_btn,&file_btn}) b->Show(2101);
  status.Show(2101);
  Application::GetInstance().GetLeftButton().Show(2102);
  Application::GetInstance().GetRightButton().Show(2102);
  Refresh(); return Result::RESULT_OK;
}
Result PcbMapScr::Hide()
{
  if(phase!=Idle) Stop();
  Application::GetInstance().GetChangeValueBox().Hide();
  Application::GetInstance().GetMsgBox().Hide();
  confirm=0;
  CloseFileMenu(); visible=false;
  for(unsigned i=0;i<PARAM_COUNT;++i) { captions[i].Hide(); params[i].Hide(); }
  for(UiButton* b : {&origin_btn,&load_btn,&verify_btn,&file_btn}) b->Hide();
  status.Hide();
  return Result::RESULT_OK;
}
void PcbMapScr::Message(const char* title,const char* text)
{
  Wrap(dialog_text,sizeof(dialog_text),text);
  auto& box=Application::GetInstance().GetMsgBox(); box.Setup(title,dialog_text,1u); box.Show(10000u);
  ProbeScr::GetInstance().DisableScreenChange();
}
void PcbMapScr::Fail(const char* reason,bool cancel_motion)
{
  GrblComm& grbl=GrblComm::GetInstance();
  if(cancel_motion&&phase!=Idle&&phase!=Processing) grbl.Stop();
  if(phase==Scanning) sequence.Abort(reason);
  PcbInvalidateSession();
  delete processor; processor=nullptr; phase=Idle; command_id=0;
  ProbeScr::GetInstance().EnableScreenChange();
  std::snprintf(status_text,sizeof(status_text),"Stopped: %s\nVerify reference or scan again.",reason);
  Refresh();
}
void PcbMapScr::Stop() { if(phase!=Idle) Fail("Cancelled by operator"); }
void PcbMapScr::Begin(bool verify_only)
{
  auto& grbl=GrblComm::GetInstance();
  if(!grbl.IsInControl()||grbl.GetState()!=GrblComm::IDLE||!grbl.IsMetric()||
     grbl.GetStatusCode()!=GrblComm::Status_OK||grbl.IsProbeTriggered()||
     grbl.IsSpindleRunning()||grbl.GetSpindleActualSpeed()>1)
  { Message("PCB MAP","Need MPG control, Idle, metric reports.\nStop spindle; release probe contact.\nHome machine and enable soft limits."); return; }
  if(verify_only&&!map.Valid()) { Message("PCB MAP","Load a complete map or scan first."); return; }
  ProgramSender::GetInstance().ReleaseDataPointer();
  AppTask::GetCurrent()->StopTimer();
  bool card_ready=MountCard();
  AppTask::GetCurrent()->StartTimer();
  if(!card_ready) { Message("PCB MAP","Insert a readable FAT SD card first."); return; }
  PcbInvalidateSession(); verify=verify_only;
  if(!verify) map.Invalidate();
  ProbeScr::GetInstance().DisableScreenChange();
  phase=QueryModal; command_started=RtosTick::GetTimeMs(); command_timeout=5000;
  if(grbl.RefreshPcbContext(command_id).IsBad()) { Fail("Cannot request controller context"); return; }
  std::snprintf(status_text,sizeof(status_text),"Reading G54 / probe context..."); Refresh();
}
bool PcbMapScr::CommandComplete()
{
  auto& grbl=GrblComm::GetInstance();
  auto result=grbl.GetCmdResult(command_id);
  if(result!=GrblComm::Status_OK&&result!=GrblComm::Status_Cmd_Not_Executed_Yet)
  { Fail("Command rejected, reset or abandoned"); return false; }
  if(uint32_t(RtosTick::GetTimeMs()-command_started)>command_timeout)
  { Fail("Command/report timed out"); return false; }
  return result==GrblComm::Status_OK&&grbl.IsStatusReceivedAfterCmd(command_id)&&grbl.GetState()==GrblComm::IDLE;
}
bool PcbMapScr::Submit(const Pcb::ProbeCommand& command)
{
  auto& grbl=GrblComm::GetInstance(); Result result=Result::ERR_CANNOT_EXECUTE;
  if(command.kind==Pcb::ProbeCommand::Touch)
    result=grbl.ProbeAxisTowardWorkpiece(GrblComm::AXIS_Z,command.z,command.feed*100u,command_id,true);
  else
  {
    char cmd[80],x[24],y[24],z[24];
    if(command.kind==Pcb::ProbeCommand::Raise)
    { Millimetres(z,sizeof(z),command.z); std::snprintf(cmd,sizeof(cmd),"G21G90G0Z%s\r",z); }
    else
    { Millimetres(x,sizeof(x),command.x); Millimetres(y,sizeof(y),command.y); std::snprintf(cmd,sizeof(cmd),"G21G90G0X%sY%s\r",x,y); }
    result=grbl.SendCmd(cmd,command_id);
  }
  if(result.IsBad()) { Fail("Cannot queue probe/move command"); return false; }
  command_started=RtosTick::GetTimeMs();
  command_timeout=command.kind==Pcb::ProbeCommand::Touch ?
    std::max<uint32_t>(10000u,uint32_t(values[TRAVEL])*60u/std::max<uint32_t>(5u,command.feed)+5000u):60000u;
  return true;
}
Result PcbMapScr::TimerExpired(uint32_t)
{
  auto& grbl=GrblComm::GetInstance();
  if(phase==Idle) return Result::RESULT_OK;
  auto state=grbl.GetState();
  if(!grbl.IsInControl()||(state!=GrblComm::IDLE&&state!=GrblComm::RUN)||
     (phase==Processing&&state!=GrblComm::IDLE)||
     grbl.GetStatusCode()!=GrblComm::Status_OK)
  { Fail("Control/link/state changed"); return Result::RESULT_OK; }
  if(phase==Processing)
  {
    bool ok=processor->Tick();
    if(!ok)
    {
      char error[128]; std::snprintf(error,sizeof(error),"Line %lu: %s",(unsigned long)processor->SourceLine(),processor->Error());
      Fail(error,false); Message("PCB FILE ERROR",error);
    }
    else if(processor->Done())
    {
      uint32_t lines=processor->OutputLines(); delete processor; processor=nullptr;
      phase=Idle; ProbeScr::GetInstance().EnableScreenChange();
      std::snprintf(status_text,sizeof(status_text),"PCB_LEV.NC ready (%lu blocks).\nOpen it in GCODE SENDER.\nKeep PCB, tool and G54 unchanged.",(unsigned long)lines); Refresh();
    }
    else { std::snprintf(status_text,sizeof(status_text),"Preparing PCB_LEV.NC\nSource line %lu / output %lu",(unsigned long)processor->SourceLine(),(unsigned long)processor->OutputLines()); Refresh(); }
    return Result::RESULT_OK;
  }
  if(grbl.IsSpindleRunning()||grbl.GetSpindleActualSpeed()>1)
  { Fail("Spindle must stay stopped"); return Result::RESULT_OK; }
  if(!CommandComplete()) return Result::RESULT_OK;
  if(phase==QueryModal)
  {
    phase=QueryParameters; command_started=RtosTick::GetTimeMs();
    if(grbl.SendCmd("$#\r",command_id).IsBad()) Fail("Cannot read tool/offset context");
    return Result::RESULT_OK;
  }
  Pcb::Context context; int32_t pos[3]={};
  if(!PcbReadContext(context,pos)||!grbl.PcbMachineReady())
  { Fail("Need G54, XYZ/mm, WCO, homed and $20=1"); return Result::RESULT_OK; }
  if(phase==QueryParameters)
  {
    if(!origin_set) { origin_x=pos[0]; origin_y=pos[1]; origin_set=true; }
    if(!verify&&!map.Configure(origin_x,origin_y,values[WIDTH],values[HEIGHT],values[COLS],values[ROWS],context))
    { Fail("Invalid grid settings"); return Result::RESULT_OK; }
    Pcb::ProbeSettings settings; settings.clearance=values[CLEARANCE]; settings.travel=values[TRAVEL]; settings.feed=values[FEED];
    if(!sequence.Begin(map,context,pos[2],settings,verify)) { Fail(sequence.Error()); return Result::RESULT_OK; }
    phase=Scanning;
  }
  else
  {
    Pcb::ProbeObservation observation; observation.context=context;
    observation.acknowledged=true; observation.fresh_status=true; observation.idle=true;
    observation.probe_pin=grbl.IsProbeTriggered();
    observation.probe_report=grbl.IsProbeDataReceived(); observation.probe_success=grbl.IsProbeSucceed();
    for(unsigned a=0;a<3;++a) observation.probe[a]=grbl.GetProbePosition(a);
    if(!sequence.Accept(observation)) { Fail(sequence.Error()); return Result::RESULT_OK; }
  }
  if(sequence.Done())
  {
    PcbBindMap(&map); phase=Idle; command_id=0;
    AppTask::GetCurrent()->StopTimer();
    bool saved=verify||PcbSaveMap(map);
    AppTask::GetCurrent()->StartTimer();
    ProbeScr::GetInstance().EnableScreenChange();
    char range[24]; Millimetres(range,sizeof(range),int32_t((map.MaxZ()-map.MinZ())*1000+0.5));
    std::snprintf(status_text,sizeof(status_text),"Map verified: %ux%u, range %s mm\n%s\nSelect PCB file to compensate.",map.nx,map.ny,range,saved ? "PCB_MAP.CSV stored on SD":"Map ready; SD save FAILED"); Refresh();
  }
  else if(sequence.Next(probe_command))
  {
    if(!Submit(probe_command)) return Result::RESULT_OK;
    std::snprintf(status_text,sizeof(status_text),"%s: point %u / %u\nKeep clip attached; spindle OFF.\nCancel/Stop aborts the sequence.",verify ? "Verify":"Scan",std::min(sequence.Point()+1,sequence.Total()),sequence.Total()); Refresh();
  }
  return Result::RESULT_OK;
}
void PcbMapScr::OpenFiles()
{
  if(!map.Valid()||!PcbJobAllowed(map.Hash())) { Message("PCB MAP","Scan first, or Load then Verify.\nKeep G54/tool/PCB unchanged."); return; }
  ProgramSender::GetInstance().ReleaseDataPointer();
  AppTask::GetCurrent()->StopTimer(); file_count=0;
  if(MountCard())
  {
    DIR dir;
    if(f_opendir(&dir,"/")==FR_OK)
    {
      FILINFO info;
      while(f_readdir(&dir,&info)==FR_OK&&info.fname[0])
      {
        if(ProgramFile(info)&&file_count<MAX_FILES)
        {
          std::snprintf(file_names[file_count],sizeof(file_names[file_count]),"%s",info.fname);
          ++file_count;
        }
      }
      f_closedir(&dir);
    }
  }
  AppTask::GetCurrent()->StartTimer();
  if(!file_count) { Message("PCB FILE","No .nc/.ngc/.gcode in SD root.\nNames must be shorter than 64 chars.\nOutput PCB_LEV.NC is excluded."); return; }
  for(unsigned i=0;i<PARAM_COUNT;++i) { captions[i].Hide(); params[i].Hide(); }
  for(UiButton* b : {&origin_btn,&load_btn,&verify_btn,&file_btn}) b->Hide(); status.Hide();
  Application::GetInstance().GetLeftButton().Hide(); Application::GetInstance().GetRightButton().Hide();
  files.SetCount(file_count); files.Show(5000); file_menu=true;
  ProbeScr::GetInstance().DisableScreenChange();
}
void PcbMapScr::CloseFileMenu()
{
  if(file_menu) { files.Hide(); file_menu=false; ProbeScr::GetInstance().EnableScreenChange(); }
}
void PcbMapScr::StartFile(unsigned index)
{
  CloseFileMenu(); Show(); if(index>=file_count) return;
  processor=new(std::nothrow) PcbFileProcessor(map,values[CLEARANCE]);
  if(!processor) { Message("PCB FILE","Not enough free memory."); return; }
  if(!processor->Open(file_names[index]))
  { char error[128]; std::snprintf(error,sizeof(error),"%s",processor->Error()); delete processor; processor=nullptr; Message("PCB FILE",error); return; }
  phase=Processing; ProbeScr::GetInstance().DisableScreenChange(); Refresh();
}
Result PcbMapScr::SelectFile(PcbMapScr* self,void* index)
{ if(self) self->StartFile(uintptr_t(index)); return Result::RESULT_OK; }
Result PcbMapScr::CancelFile(PcbMapScr* self,void*)
{ if(self) { self->CloseFileMenu(); self->Show(); } return Result::RESULT_OK; }
Result PcbMapScr::ProcessCallback(const void* ptr)
{
  auto& app=Application::GetInstance(); auto& grbl=GrblComm::GetInstance();
  if(ptr==&app.GetLeftButton())
  {
    if(phase!=Idle) Stop();
    else { confirm=1; Message("SCAN PCB","Clip connected to copper, spindle OFF.\nStart with Z above the PCB.\nBoard and tool must remain fixed.\nProceed with automatic grid probing?"); }
    return Result::RESULT_OK;
  }
  if(ptr==&app.GetRightButton()) { Stop(); return Result::ERR_UNHANDLED_REQUEST; }
  if(phase!=Idle) return Result::RESULT_OK;
  if(ptr==&app.GetMsgBox())
  {
    unsigned action=confirm; confirm=0;
    ProbeScr::GetInstance().EnableScreenChange();
    if(action&&app.GetMsgBox().GetResult()==Result::RESULT_OK) Begin(action==2);
    return Result::RESULT_OK;
  }
  if(ptr==&app.GetChangeValueBox())
  {
    ProbeScr::GetInstance().EnableScreenChange();
    if(app.GetChangeValueBox().GetResult()&&edited<PARAM_COUNT)
    {
      values[edited]=app.GetChangeValueBox().GetValue();
      if(edited<=ROWS) { map.Invalidate(); PcbInvalidateSession(); }
      Refresh();
    }
    return Result::RESULT_OK;
  }
  for(unsigned i=0;i<PARAM_COUNT;++i) if(ptr==&params[i])
  {
    edited=i; int32_t min=1000,max=2000000; unsigned precision=3;
    if(i==COLS||i==ROWS) { min=2; max=Pcb::HeightMap::MAX_AXIS; precision=0; }
    else if(i==CLEARANCE) { min=500; max=20000; }
    else if(i==TRAVEL) { min=100; max=20000; }
    else if(i==FEED) { min=5; max=300; precision=0; }
    app.GetChangeValueBox().Setup(LABELS[i],i==FEED ? "mm/min":(precision ? "mm":"points"),values[i],min,max,precision,1);
    app.GetChangeValueBox().Show(10000);
    ProbeScr::GetInstance().DisableScreenChange();
    return Result::RESULT_OK;
  }
  if(ptr==&origin_btn)
  {
    if(!grbl.IsMetric()||grbl.GetState()!=GrblComm::IDLE) { Message("PCB ORIGIN","Need Idle and metric reports."); return Result::RESULT_OK; }
    origin_x=grbl.GetAxisPosition(0); origin_y=grbl.GetAxisPosition(1); origin_set=true;
    map.Invalidate(); PcbInvalidateSession(); char x[24],y[24]; Millimetres(x,sizeof(x),origin_x); Millimetres(y,sizeof(y),origin_y);
    std::snprintf(status_text,sizeof(status_text),"Grid starts at X%s Y%s\nWidth/length extend toward +X/+Y.",x,y); Refresh();
  }
  else if(ptr==&load_btn)
  {
    PcbInvalidateSession(); map.Invalidate();
    AppTask::GetCurrent()->StopTimer(); bool ok=MountCard()&&PcbLoadMap(map); AppTask::GetCurrent()->StartTimer();
    if(ok)
    {
      values[WIDTH]=map.span_x; values[HEIGHT]=map.span_y; values[COLS]=map.nx; values[ROWS]=map.ny;
      origin_x=map.x0; origin_y=map.y0; origin_set=true;
      std::snprintf(status_text,sizeof(status_text),"Loaded %ux%u map; NOT verified.\nUse same fixed PCB/tool/G54.\nVerify reference before preparing job.",map.nx,map.ny); Refresh();
    }
    else Message("PCB MAP","Map missing, incomplete or corrupted.\nA complete scan creates PCB_MAP.CSV.");
  }
  else if(ptr==&verify_btn)
  { confirm=2; Message("VERIFY MAP","Same fixed PCB, tool and G54?\nReference will be probed twice.\nIf setup changed, run a full scan."); }
  else if(ptr==&file_btn) OpenFiles();
  return Result::RESULT_OK;
}
