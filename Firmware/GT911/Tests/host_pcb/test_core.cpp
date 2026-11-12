#include "PcbHeightMap.h"
#include "PcbProbeSequence.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>
using namespace Pcb;

static HeightMap MakeMap(bool saddle=false,int32_t width=20000,int32_t height=10000)
{
  HeightMap m; Context c; c.offset[0]=-100000; c.offset[1]=-50000; c.offset[2]=-30000; c.epoch=3;
  assert(m.Configure(0,0,width,height,5,3,c));
  for(unsigned y=0;y<m.ny;++y) for(unsigned x=0;x<m.nx;++x)
  {
    double px=m.X(x)/1000.0,py=m.Y(y)/1000.0;
    int32_t z=std::llround((0.01*px+0.02*py+(saddle ? 0.01*px*py:0))*1000);
    assert(m.Set(x,y,z));
  }
  assert(m.Complete()); return m;
}
static void Consume(Compensator& engine,const char* line,std::vector<std::string>& out)
{
  if(!engine.BeginLine(line)) { std::fprintf(stderr,"Rejected test block [%s]: %s\n",line,engine.Error()); assert(false); }
  char text[80]; int n; unsigned count=0;
  while((n=engine.Next(text,sizeof(text)))>0) { out.emplace_back(text); assert(std::strlen(text)<=72); assert(++count<100000); }
  assert(n==0);
}
static bool Refuses(const HeightMap& map,const char* line)
{
  Compensator c(map,{10,5,2}); std::vector<std::string> text;
  Consume(c,"G21 G90 G17 G94 G54 F100",text);
  if(!c.BeginLine(line)) return true;
  char out[80]; int n; do { n=c.Next(out,sizeof(out)); } while(n>0);
  return n<0;
}
static void TestMap()
{
  HeightMap m=MakeMap(); double z;
  assert(m.Correction(10,5,z)&&std::fabs(z-.2)<1e-12);
  assert(m.Correction(20,10,z)&&std::fabs(z-.4)<1e-12);
  assert(!m.Correction(20.001,0,z));
  assert(!m.Correction(NAN,0,z));
  assert(!m.Set(0,0,0));
  HeightMap loaded; char record[96];
  for(unsigned i=0;i<m.RecordCount();++i) { assert(m.WriteRecord(i,record,sizeof(record))); assert(loaded.ReadRecord(i,record)); }
  assert(loaded.Valid()&&loaded.Hash()==m.Hash());
  Context fresh=m.context; fresh.epoch++;
  assert(loaded.context.Matches(fresh,false)&&!loaded.context.Matches(fresh));
  loaded.Invalidate(); assert(!loaded.Valid());
  HeightMap corrupt;
  for(unsigned i=0;i<m.RecordCount();++i)
  {
    assert(m.WriteRecord(i,record,sizeof(record)));
    if(i==3) std::strcpy(record,"H,0,99\n");
    if(i+1<m.RecordCount()) assert(corrupt.ReadRecord(i,record));
    else assert(!corrupt.ReadRecord(i,record));
  }
  assert(!corrupt.Valid());
  HeightMap bad; assert(!bad.Configure(INT32_MAX,0,1000,1000,3,3,{}));
  assert(!bad.Configure(0,0,1000,1000,22,3,{}));
  assert(!bad.ReadRecord(1,"G,3,3,4294967295,0,1000,1000\n"));
  assert(!bad.ReadRecord(1,"G,3,3,0,0,100000000000000000000,1000\n"));
  std::puts("PASS: grid limits, bilinear interpolation, checksum records, corruption, context");
}
static void TestLines()
{
  HeightMap m=MakeMap(true); Compensator engine(m,{0,0,1}); std::vector<std::string> lines;
  Consume(engine,"G21 G90 G17 G94 G54",lines);
  Consume(engine,"M64P10",lines);
  Consume(engine,"N1 G1 X20 Y10 Z-0.05 F100 (engrave)",lines);
  double px=0,py=0,pz=1;
  unsigned moves=0;
  for(const auto& text:lines)
  {
    double x,y,z;
    if(std::sscanf(text.c_str(),"G1X%lfY%lfZ%lf",&x,&y,&z)==3)
    {
      double surface;
      assert(m.Correction(x,y,surface));
      double nominal=1+(-.05-1)*(x/20);
      assert(std::fabs(z-(nominal+surface))<0.000051);
      // The emitted straight chord must stay within the compensation tolerance.
      for(unsigned j=1;j<10;++j)
      {
        double t=j/10.0,qx=px+(x-px)*t,qy=py+(y-py)*t;
        assert(m.Correction(qx,qy,surface));
        double qnom=1+(-.05-1)*(qx/20);
        assert(std::fabs((pz+(z-pz)*t)-(qnom+surface))<=0.00111);
      }
      px=x;py=y;pz=z;++moves;
    }
  }
  assert(moves>20&&std::fabs(px-20)<1e-8&&std::fabs(py-10)<1e-8);
  assert(std::fabs(engine.Current().z+.05)<1e-8);
  for(const char* bad : {"G91X1", "G20", "G18", "G53G0Z0", "G92X0", "G10L20P1Z0",
      "G81X1Y1Z-1R1", "G43.1Z1", "T1M6", "M6", "M70", "M72", "G1X1000", "G1X1X2",
      "G1X[1+2]", "$H", "/G0X1", "G1X1e999", "G1Xnan", "G1X1(unfinished", "G2X1Y1",
      "G1I1", "M30X1", "G4P1X2", "G1X1P2", "M64P1.5", "M64P10X1"}) assert(Refuses(m,bad));
  Compensator no_units(m,{0,0,1}); assert(!no_units.BeginLine("G1X1F100"));
  Compensator no_feed(m,{0,0,1}); std::vector<std::string> tmp;
  Consume(no_feed,"G21G90",tmp); assert(!no_feed.BeginLine("G1X1"));
  Consume(engine,"M5",lines); Consume(engine,"M30",lines); assert(!engine.BeginLine("G0X0"));
  std::puts("PASS: subdivided Z compensation, mesh boundaries, modal feed, fail-closed G-code");
}
static void TestArcs()
{
  HeightMap map=MakeMap(false,20000,20000);
  for(const char* arc : {"G2X10Y10I5J0F100","G3X10Y10R5F100","G2X10Y10R-5F100", "G2I5J0F100"})
  {
    // All these arcs use a circle centred at (10,5) or a radius-five variant.
    Compensator engine(map,{5,5,-.05}); std::vector<std::string> output;
    Consume(engine,"G21G90G17G94",output);
    Consume(engine,arc,output);
    assert(output.size()>30);
    for(const auto& line:output)
    {
      double x,y,z;
      if(std::sscanf(line.c_str(),"G1X%lfY%lfZ%lf",&x,&y,&z)==3)
      { double surface; assert(map.Correction(x,y,surface)); assert(std::fabs(z-(-.05+surface))<0.000051); }
    }
  }
  assert(Refuses(map,"G2X15Y5R2F100"));
  assert(Refuses(map,"G2X10Y9I5J0F100"));
  assert(Refuses(map,"G2I50J0F100"));
  assert(Refuses(map,"G2I1R1F100"));
  std::puts("PASS: I/J full circles, R short/long arcs, arc bounds and compensation");
}
static void TestSequence()
{
  HeightMap m; Context c; c.epoch=4;
  assert(m.Configure(0,0,20000,10000,5,3,c));
  ProbeSettings settings; ProbeSequence scan;
  assert(scan.Begin(m,c,1000,settings,false));
  int32_t x=0,y=0,z=1000; unsigned touches=0,moves=0;
  while(!scan.Done())
  {
    ProbeCommand cmd; assert(scan.Next(cmd));
    ProbeObservation o; o.context=c; o.acknowledged=o.fresh_status=o.idle=true;
    if(cmd.kind==ProbeCommand::Travel) { x=cmd.x; y=cmd.y; }
    else if(cmd.kind==ProbeCommand::Raise) z=cmd.z;
    else
    {
      o.probe_report=o.probe_success=o.probe_pin=true;
      o.probe[0]=x;o.probe[1]=y;o.probe[2]=x/100+y/50;
      z=o.probe[2];++touches;
    }
    assert(scan.Accept(o)); assert(++moves<1000);
  }
  assert(m.Valid()&&touches==30&&x==0&&y==0&&z>=1000);
  HeightMap saved=m;
  for(unsigned fault=0;fault<6;++fault)
  {
    HeightMap test=m; ProbeSequence probe; assert(probe.Begin(test,c,1000,settings,false));
    ProbeCommand command; ProbeObservation o; o.context=c; o.acknowledged=o.fresh_status=o.idle=true;
    for(unsigned n=0;n<3;++n)
    {
      assert(probe.Next(command));
      if(n<2) { assert(probe.Accept(o)); continue; }
      o.probe_report=o.probe_success=true;
      if(fault==0) o.probe_report=false;
      if(fault==1) o.probe_success=false;
      if(fault==2) ++o.context.epoch;
      if(fault==3) o.fresh_status=false;
      if(fault==4) o.probe[0]=1000;
      if(fault==5) { probe.Abort("operator stop"); break; }
      assert(!probe.Accept(o));
    }
    assert(probe.Failed()&&!test.Valid()&&!probe.Next(command));
  }
  ProbeSequence verify; Context next=c; next.epoch++;
  assert(verify.Begin(saved,next,1000,settings,true));
  unsigned touches_verified=0;
  while(!verify.Done())
  {
    ProbeCommand cmd; assert(verify.Next(cmd));
    ProbeObservation o; o.context=next; o.acknowledged=o.fresh_status=o.idle=true;
    if(cmd.kind==ProbeCommand::Touch) { o.probe_report=o.probe_success=o.probe_pin=true; ++touches_verified; }
    assert(verify.Accept(o));
  }
  assert(touches_verified==2&&saved.Valid()&&saved.context.epoch==next.epoch);
  std::puts("PASS: two-touch serpentine scan, retract/park, stale/missing/failed probes, cancel, verify epoch");
}
int main() { TestMap(); TestLines(); TestArcs(); TestSequence(); }
