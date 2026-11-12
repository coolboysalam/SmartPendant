#include "GrblComm.h"
#include "PcbRuntime.h"
#include "PcbFiles.h"
#include "sender.h"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
struct Wire : IUart
{
  std::vector<std::string> output;
  Result Init() override { return Result::RESULT_OK; }
  Result SetBaudRate(uint32_t) override { return Result::RESULT_OK; }
  Result Read(uint8_t&) override { return Result::ERR_UART_EMPTY; }
  Result Write(uint8_t* text,uint32_t size) override { output.emplace_back((char*)text,size); return Result::RESULT_OK; }
};
static Wire wire;
static void Feed(GrblComm& comm,const char* text)
{
  assert(std::strlen(text)<sizeof(comm.rx_buf));
  std::strcpy((char*)comm.rx_buf,text); comm.ParseData();
}
static GrblComm& Ready()
{
  auto& c=GrblComm::GetInstance(); c.uart=&wire;
  c.messages.clear(); c.grbl_state=GrblComm::IDLE;c.grbl_status=GrblComm::Status_OK;
  c.grbl_mpgMode=true;c.number_of_axis=3;c.mode_of_operation=GrblComm::MODE_OF_OPERATION_MILL;
  c.measurement_system=GrblComm::MEASUREMENT_SYSTEM_METRIC;c.request_settings=false;
  c.respond_pending=false;
  Feed(c,"$10=48"); Feed(c,"$20=1"); Feed(c,"$22=1");
  Feed(c,"[GC:G0 G54 G17 G21 G90 G94 M5 M9 T2 F0 S0]");
  Feed(c,"[TLO:0.000]");
  Feed(c,"<Idle|MPos:-90.000,-40.000,-27.000|WCO:-100.000,-50.000,-30.000|H:1|T:2|MPG:1>");
  return c;
}
static Pcb::HeightMap MakeMap()
{
  Pcb::Context context; assert(PcbReadContext(context));
  Pcb::HeightMap map; assert(map.Configure(0,0,20000,10000,5,3,context));
  for(unsigned y=0;y<3;++y) for(unsigned x=0;x<5;++x) assert(map.Set(x,y,x*50+y*100));
  assert(map.Complete()); return map;
}
static void ContextTests()
{
  auto& c=Ready(); int32_t work[3],offset[3],tlo; uint32_t tool,epoch;
  assert(c.GetPcbContext(work,offset,tool,tlo,epoch)&&c.PcbMachineReady());
  assert(work[0]==10000&&work[1]==10000&&work[2]==3000&&offset[2]==-30000&&tool==2&&tlo==0);
  Feed(c,"[TLO:0.125]"); assert(c.GetPcbContext(work,offset,tool,tlo,epoch)&&tlo==125);
  Feed(c,"[TLO:0.000,0.000,0.250]"); assert(c.GetPcbContext(work,offset,tool,tlo,epoch)&&tlo==250);
  Feed(c,"<Idle|MPos:10.0,nan,3|WCO:-100,-50,-30>");
  Pcb::Context invalid; assert(!PcbReadContext(invalid));
  Ready();
  for(const char* bad:{"<Idle|WCO:1,2>","<Idle|MPos:1,2,3oops>","<Idle|MPos:10000000000000,2,3>","<Idle|MPos:2000.1,2,3>"})
  { Ready(); Feed(c,bad); Pcb::Context ctx; assert(!PcbReadContext(ctx)); }
  Ready(); Feed(c,"[GC:G0 G55 G17 G21 G90 G94]"); Pcb::Context ctx; assert(!PcbReadContext(ctx));
  Ready(); Feed(c,"[GC:G0 G54.1 G17 G21 G90 G94]"); assert(!PcbReadContext(ctx));
  Ready(); Feed(c,"$20=0"); assert(!c.PcbMachineReady());
  Ready(); Feed(c,"<Idle|H:0>"); assert(!c.PcbMachineReady());
  Ready(); Pcb::HeightMap map=MakeMap(); PcbBindMap(&map); assert(PcbJobAllowed(map.Hash()));
  Feed(c,"<Idle|WCO:-100,-50,-30.001>"); assert(!PcbJobAllowed(map.Hash()));
  Ready(); assert(!PcbJobAllowed(map.Hash())); // restoring values alone cannot re-verify a map
  PcbBindMap(&map); assert(PcbJobAllowed(map.Hash()));
  c.Stop(); assert(!PcbJobAllowed(map.Hash()));
  Ready(); map=MakeMap(); PcbBindMap(&map);
  Feed(c,"<Home|MPos:-90,-40,-27>"); assert(!PcbJobAllowed(map.Hash()));
  Ready(); map=MakeMap(); PcbBindMap(&map);
  Feed(c,"GrblHAL 1.1f"); assert(!PcbJobAllowed(map.Hash()));
  Ready(); map=MakeMap(); PcbBindMap(&map);
  RtosTick::now=1000; c.status_rx_timestamp=0; c.TimerExpired(0); assert(!PcbJobAllowed(map.Hash()));
  PcbInvalidateSession();
  uint32_t hash;
  assert(!PcbReadJobHeader("G0X0",hash));
  assert(PcbReadJobHeader(";PCBHM1 1234ABCD\n",hash)&&hash==0x1234ABCD);
  for(const char* text:{";PCBHM1",";PCBHM1 ",";PCBHM1 123Q",";PCBHM1 1234ABCDX"})
  { assert(PcbReadJobHeader(text,hash)&&hash==0); }
  std::puts("PASS: real status/modal/TLO parser, stale WCO, homing/reset/stop/link epochs, job signatures");
}
static void FileTests()
{
  Ready(); FakeFs::files.clear();FakeFs::ResetFaults();
  Pcb::HeightMap map=MakeMap();PcbBindMap(&map);
  assert(PcbSaveMap(map)); std::string original_map=FakeFs::files["PCB_MAP.CSV"];
  Pcb::HeightMap loaded;assert(PcbLoadMap(loaded)&&loaded.Hash()==map.Hash());
  assert(!PcbJobAllowed(loaded.Hash()));
  // A successful reference verification binds the runtime epoch.
  loaded.context.epoch=map.context.epoch;PcbBindMap(&loaded);assert(PcbJobAllowed(map.Hash()));
  FakeFs::fail_write=FakeFs::write_calls+2;assert(!PcbSaveMap(map));
  assert(FakeFs::files["PCB_MAP.CSV"]==original_map);FakeFs::ResetFaults();
  FakeFs::files["PCB_MAP.CSV"]="PCBHM1\nG,2,2,0,0,1000,1000\n";
  assert(!PcbLoadMap(loaded)&&!loaded.Valid());FakeFs::files["PCB_MAP.CSV"]=original_map;
  assert(PcbLoadMap(loaded));loaded.context.epoch=map.context.epoch;PcbBindMap(&loaded);
  const std::string input="G21G90G17G94G54\nG0X5Y5Z1\nM3S12000\nG1Z-0.05F100\nG2I5J0\nG0Z2\nM5\nM30\n";
  FakeFs::files["INPUT.NC"]=input;
  {
    PcbFileProcessor processor(loaded,1000);assert(processor.Open("INPUT.NC"));unsigned calls=0;
    while(!processor.Done()) { assert(processor.Tick(1));assert(++calls<10000); }
    assert(calls>100&&processor.OutputLines()>100);
    assert(FakeFs::files["INPUT.NC"]==input);
    assert(!FakeFs::files.count("PCB_TMP.NC"));
  }
  const std::string good=FakeFs::files["PCB_LEV.NC"];
  if(const char* directory=std::getenv("PCB_VALIDATOR_OUTPUT"))
  {
    std::ofstream(std::string(directory)+"/prepared-pcb.nc")<<good;
    std::ofstream(std::string(directory)+"/source-pcb.nc")<<input;
  }
  assert(good.find(";PCBHM1 ")==0&&good.find("G1X")!=std::string::npos);
  {
    PcbFileProcessor duplicate(loaded,1000);assert(!duplicate.Open("PCB_LEV.NC"));
  }
  for(unsigned fault=0;fault<6;++fault)
  {
    Ready();loaded.context.epoch=GrblComm::GetInstance().pcb_epoch;PcbBindMap(&loaded);FakeFs::ResetFaults();
    FakeFs::files["INPUT.NC"]=input;
    if(fault==0) FakeFs::files["INPUT.NC"]="G21G90\nM6\n";
    if(fault==1) FakeFs::files["INPUT.NC"]="G21G90\nG0X1000\n";
    if(fault==2) FakeFs::read_limit=30;
    if(fault==3) FakeFs::fail_write=3;
    if(fault==4) FakeFs::fail_sync=true;
    if(fault==5) FakeFs::fail_rename=2;
    PcbFileProcessor processor(loaded,1000);
    bool ok=processor.Open("INPUT.NC"); unsigned calls=0;
    while(ok&&!processor.Done()) { ok=processor.Tick(3);assert(++calls<10000); }
    assert(!ok&&!processor.Done());assert(FakeFs::files["PCB_LEV.NC"]==good);
    assert(!FakeFs::files.count("PCB_TMP.NC"));
  }
  Ready();loaded.context.epoch=GrblComm::GetInstance().pcb_epoch;PcbBindMap(&loaded);FakeFs::ResetFaults();FakeFs::files["INPUT.NC"]=input;
  {
    PcbFileProcessor processor(loaded,1000);assert(processor.Open("INPUT.NC"));assert(processor.Tick(1));
    GrblComm::GetInstance().grbl_tool++;assert(!processor.Tick());
    assert(FakeFs::files["PCB_LEV.NC"]==good&&!FakeFs::files.count("PCB_TMP.NC"));
  }
  std::puts("PASS: real file processor, bounded work, unchanged input, missing/corrupt maps, read/write/sync/rename faults, stale contexts");
}
static void SenderTests()
{
  auto& comm=Ready(); Pcb::HeightMap map=MakeMap();PcbBindMap(&map);
  ProgramSender sender(comm);sender.pcb_job=true;sender.pcb_hash=map.Hash();
  comm.grbl_tool++;
  size_t before=wire.output.size();sender.TimerExpired(0);
  assert(!sender.run&&sender.finished&&sender.msg_box.shown>0&&wire.output.size()==before);
  std::puts("PASS: real ProgramSender timer stops protected jobs on a changed tool");
}
int main() { ContextTests(); FileTests(); SenderTests(); }
