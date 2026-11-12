#include "PcbHeightMap.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <climits>

namespace Pcb
{
namespace
{
constexpr double PI = 3.14159265358979323846;
constexpr double EPS = 1e-8;
double Clamp(double v, double lo, double hi) { return std::max(lo, std::min(hi, v)); }
bool Equal(double a, double b) { return std::fabs(a - b) < EPS; }
bool Fits(int n, size_t size) { return n >= 0 && size_t(n) < size; }
bool Fixed4(double value, char* out, size_t size)
{
  if(!std::isfinite(value) || std::fabs(value)>2000001) return false;
  int64_t rounded=std::llround(value*10000.0);
  uint64_t magnitude=rounded<0 ? uint64_t(-rounded):uint64_t(rounded);
  return Fits(std::snprintf(out,size,"%s%lu.%04lu",rounded<0 ? "-":"",
    (unsigned long)(magnitude/10000),(unsigned long)(magnitude%10000)),size);
}
void HashWord(uint32_t& hash, uint32_t word)
{
  for(unsigned i = 0; i < 4; ++i) { hash ^= (word >> (i * 8)) & 255u; hash *= 16777619u; }
}
double PositiveAngle(double a) { while(a < 0) a += 2 * PI; while(a >= 2 * PI) a -= 2 * PI; return a; }
bool OnArc(double candidate, double start, double sweep)
{
  return sweep > 0 ? PositiveAngle(candidate - start) <= sweep + EPS :
                     PositiveAngle(start - candidate) <= -sweep + EPS;
}
bool ReadInts(const char* text, const char* tag, int64_t* values, unsigned n, bool hex = false)
{
  size_t len = std::strlen(tag);
  if(std::strncmp(text, tag, len)) return false;
  text += len;
  for(unsigned i = 0; i < n; ++i)
  {
    char* end = nullptr;
    if(!*text || std::isspace(static_cast<unsigned char>(*text))) return false;
    values[i] = std::strtoll(text, &end, hex ? 16 : 10);
    if(end == text || values[i] < INT32_MIN || values[i] > UINT32_MAX) return false;
    text = end;
    if(i + 1 < n) { if(*text++ != ',') return false; }
  }
  while(*text == '\r' || *text == '\n') ++text;
  return *text == 0;
}
}

bool Context::Matches(const Context& b, bool check_epoch) const
{
  return offset[0] == b.offset[0] && offset[1] == b.offset[1] &&
         offset[2] == b.offset[2] && tool == b.tool && tlo == b.tlo &&
         (!check_epoch || epoch == b.epoch);
}

bool HeightMap::Configure(int32_t x, int32_t y, int32_t width, int32_t height,
                          unsigned cols, unsigned rows, const Context& ctx)
{
  Invalidate();
  if(cols < 2 || rows < 2 || cols > MAX_AXIS || rows > MAX_AXIS ||
     width < 100 || height < 100 || width > 2000000 || height > 2000000 ||
     std::abs(int64_t(x)) > 2000000 || std::abs(int64_t(y)) > 2000000 ||
     int64_t(x) + width > 2000000 || int64_t(y) + height > 2000000) return false;
  x0 = x; y0 = y; span_x = width; span_y = height; nx = cols; ny = rows; context = ctx;
  return true;
}
void HeightMap::Invalidate() { valid = false; stored = 0; std::memset(received, 0, sizeof(received)); }
int32_t HeightMap::X(unsigned col) const { return x0 + int64_t(span_x) * col / (nx - 1); }
int32_t HeightMap::Y(unsigned row) const { return y0 + int64_t(span_y) * row / (ny - 1); }
int32_t HeightMap::Height(unsigned col, unsigned row) const { return z[row * nx + col]; }
bool HeightMap::Set(unsigned col, unsigned row, int32_t value)
{
  if(col >= nx || row >= ny || nx < 2 || ny < 2 || std::abs(int64_t(value)) > 2000000) return false;
  unsigned index = row * nx + col;
  if(received[index / 8] & (1u << (index % 8))) return false;
  received[index / 8] |= 1u << (index % 8); z[index] = value; ++stored; valid = false;
  return true;
}
bool HeightMap::Complete() { valid = nx >= 2 && ny >= 2 && stored == Count(); return valid; }
bool HeightMap::Contains(double x, double y) const
{
  return Valid() && std::isfinite(x) && std::isfinite(y) &&
    x >= x0 / 1000.0 - EPS && x <= (x0 + span_x) / 1000.0 + EPS &&
    y >= y0 / 1000.0 - EPS && y <= (y0 + span_y) / 1000.0 + EPS;
}
bool HeightMap::Correction(double x, double y, double& result) const
{
  if(!Contains(x, y)) return false;
  // Use actual rounded micrometre probe coordinates, not ideal grid spacing.
  double gx = (x * 1000.0 - x0) * (nx - 1) / span_x;
  double gy = (y * 1000.0 - y0) * (ny - 1) / span_y;
  unsigned ix = std::min(unsigned(std::max(0.0, std::floor(gx))), nx - 2);
  unsigned iy = std::min(unsigned(std::max(0.0, std::floor(gy))), ny - 2);
  if(ix + 1 < nx - 1 && x * 1000.0 > X(ix + 1)) ++ix;
  if(iy + 1 < ny - 1 && y * 1000.0 > Y(iy + 1)) ++iy;
  if(ix > 0 && x * 1000.0 < X(ix)) --ix;
  if(iy > 0 && y * 1000.0 < Y(iy)) --iy;
  double u = Clamp((x * 1000.0 - X(ix)) / (X(ix+1) - X(ix)), 0, 1);
  double v = Clamp((y * 1000.0 - Y(iy)) / (Y(iy+1) - Y(iy)), 0, 1);
  result = ((1-u)*(1-v)*Height(ix,iy) + u*(1-v)*Height(ix+1,iy) +
             (1-u)*v*Height(ix,iy+1) + u*v*Height(ix+1,iy+1)) / 1000.0;
  return std::isfinite(result);
}
double HeightMap::MinZ() const { int32_t v = z[0]; for(unsigned i=1;i<Count();++i) v=std::min(v,z[i]); return v/1000.0; }
double HeightMap::MaxZ() const { int32_t v = z[0]; for(unsigned i=1;i<Count();++i) v=std::max(v,z[i]); return v/1000.0; }
double HeightMap::MaxTwist() const
{
  double result = 0;
  for(unsigned y=0;y+1<ny;++y) for(unsigned x=0;x+1<nx;++x)
  {
    double twist = double(Height(x,y))-Height(x+1,y)-Height(x,y+1)+Height(x+1,y+1);
    result=std::max(result,std::fabs(twist)*1000.0/((X(x+1)-X(x))*double(Y(y+1)-Y(y))));
  }
  return result;
}
uint32_t HeightMap::Hash() const
{
  uint32_t h=2166136261u;
  for(int32_t v : {int32_t(nx),int32_t(ny),x0,y0,span_x,span_y}) HashWord(h,uint32_t(v));
  for(int32_t v : context.offset) HashWord(h,uint32_t(v));
  HashWord(h,context.tool); HashWord(h,uint32_t(context.tlo));
  for(unsigned i=0;i<Count();++i) HashWord(h,uint32_t(z[i]));
  return h ? h:1u;
}
bool HeightMap::WriteRecord(unsigned row, char* out, size_t size) const
{
  if(!Valid() || !out || !size) return false;
  int n=-1;
  if(row==0) n=std::snprintf(out,size,"PCBHM1\n");
  else if(row==1) n=std::snprintf(out,size,"G,%u,%u,%ld,%ld,%ld,%ld\n",nx,ny,long(x0),long(y0),long(span_x),long(span_y));
  else if(row==2) n=std::snprintf(out,size,"C,%ld,%ld,%ld,%lu,%ld\n",long(context.offset[0]),long(context.offset[1]),long(context.offset[2]),(unsigned long)context.tool,long(context.tlo));
  else if(row<Count()+3) n=std::snprintf(out,size,"H,%u,%ld\n",row-3,long(z[row-3]));
  else if(row==Count()+3) n=std::snprintf(out,size,"END,%08lX\n",(unsigned long)Hash());
  return Fits(n,size);
}
bool HeightMap::ReadRecord(unsigned row, const char* text)
{
  int64_t v[6]={};
  if(row==0) { Invalidate(); nx=ny=0; return !std::strcmp(text,"PCBHM1\n") || !std::strcmp(text,"PCBHM1\r\n"); }
  if(row==1)
  {
    if(!ReadInts(text,"G,",v,6) || v[0]<2 || v[1]<2 || v[0]>MAX_AXIS || v[1]>MAX_AXIS ||
       std::abs(v[2])>2000000 || std::abs(v[3])>2000000 || v[4]>2000000 || v[5]>2000000) return false;
    Context c; return Configure(v[2],v[3],v[4],v[5],v[0],v[1],c);
  }
  if(row==2)
  {
    if(!ReadInts(text,"C,",v,5)) return false;
    for(unsigned i=0;i<3;++i) { if(std::abs(v[i])>2000000) return false; context.offset[i]=v[i]; }
    if(v[3]<0 || v[3]>999999 || std::abs(v[4])>2000000) return false;
    context.tool=v[3]; context.tlo=v[4]; return true;
  }
  if(row<Count()+3)
  {
    if(!nx || !ReadInts(text,"H,",v,2) || v[0]!=int64_t(row-3) || std::abs(v[1])>2000000) return false;
    return Set((row-3)%nx,(row-3)/nx,v[1]);
  }
  if(row==Count()+3 && ReadInts(text,"END,",v,1,true) && uint32_t(v[0])==Hash()) return Complete();
  return false;
}

Compensator::Compensator(const HeightMap& input, Position initial) : map(input), current(initial) {}
bool Compensator::Fail(const char* message) { error=message; return false; }
bool Compensator::PrepareLinear(Position from, Position to)
{
  if(!map.Contains(from.x,from.y) || !map.Contains(to.x,to.y)) return Fail("XY outside measured map");
  line_start=from; line_end=to; t=0;
  double dx=to.x-from.x,dy=to.y-from.y;
  double count=std::max(1.0,std::ceil(std::hypot(dx,dy))); // <=1 mm XY segments
  count=std::max(count,std::ceil(std::sqrt(map.MaxTwist()*std::fabs(dx*dy)/(4*Z_TOLERANCE))));
  if(count>200000 || !std::isfinite(count)) return Fail("Move has too many segments");
  dt=1/count; linear_pending=true; return true;
}
bool Compensator::PrepareArc(Position from, Position to, bool cw,
                            bool has_i,double i,bool has_j,double j,bool has_r,double r)
{
  if(has_r && (has_i||has_j)) return Fail("Arc mixes R with I/J");
  if(!has_r && !has_i && !has_j) return Fail("Arc has no centre");
  double dx=to.x-from.x,dy=to.y-from.y;
  if(has_r)
  {
    double chord=std::hypot(dx,dy), rad=std::fabs(r);
    if(chord<EPS || rad<chord/2-EPS) return Fail("Invalid R arc");
    double h=std::sqrt(std::max(0.0,rad*rad-chord*chord/4));
    double side=(cw ? -1.0:1.0)*(r<0 ? -1.0:1.0);
    cx=(from.x+to.x)/2-side*dy/chord*h;
    cy=(from.y+to.y)/2+side*dx/chord*h;
  }
  else { cx=from.x+i; cy=from.y+j; }
  radius=std::hypot(from.x-cx,from.y-cy);
  if(radius<0.0001 || std::fabs(std::hypot(to.x-cx,to.y-cy)-radius)>std::max(0.002,radius*0.0001)) return Fail("Arc radii do not match");
  angle=std::atan2(from.y-cy,from.x-cx);
  double end=std::atan2(to.y-cy,to.x-cx);
  sweep=cw ? -PositiveAngle(angle-end):PositiveAngle(end-angle);
  if(std::fabs(sweep)<EPS) sweep=cw ? -2*PI:2*PI;
  for(unsigned k=0;k<4;++k)
    if(OnArc(k*PI/2,angle,sweep) && !map.Contains(cx+radius*std::cos(k*PI/2),cy+radius*std::sin(k*PI/2))) return Fail("Arc leaves measured map");
  if(!map.Contains(from.x,from.y)||!map.Contains(to.x,to.y)) return Fail("Arc outside measured map");
  double max_angle=std::min(PI/8,2*std::acos(Clamp(1-ARC_TOLERANCE/radius,-1,1)));
  arc_steps=unsigned(std::ceil(std::fabs(sweep)/max_angle));
  if(!arc_steps || arc_steps>200000) return Fail("Arc has too many segments");
  arc_step=0; block_start=from; block_end=to; return true;
}

bool Compensator::BeginLine(const char* input)
{
  if(error) return false;
  if(linear_pending || arc_step<arc_steps || prefix_ready) return Fail("Previous block not consumed");
  char line[192]={}; unsigned n=0; bool comment=false;
  for(const unsigned char* p=(const unsigned char*)input;*p;++p)
  {
    if(*p=='(') { if(comment) return Fail("Nested comment"); comment=true; continue; }
    if(*p==')') { if(!comment) return Fail("Unmatched comment"); comment=false; continue; }
    if(comment) continue;
    if(*p==';') break;
    if(*p=='\r'||*p=='\n') break;
    if(std::isspace(*p)) continue;
    if(n+1>=sizeof(line)) return Fail("Source block too long");
    line[n++]=std::toupper(*p);
  }
  if(comment) return Fail("Unclosed comment");
  if(!n || !std::strcmp(line,"%")) return true;
  if(ended) return Fail("Code follows program end");
  Position target=current;
  bool axes=false,have[26]={},mi=false,mj=false,mr=false,dwell=false,has_end=false,immediate_output=false;
  bool motion_seen=false; double ai=0,aj=0,ar=0,parameter=0;
  int block_motion=motion; prefix[0]=0;
  unsigned used=0;
  const char* p=line;
  while(*p)
  {
    char letter=*p++;
    if(letter<'A'||letter>'Z') return Fail("Unsupported G-code syntax");
    // No exponent syntax: E is a separate, unsupported G-code word.
    const char* number=p; if(*p=='+'||*p=='-') ++p;
    bool digit=false,point=false;
    while(std::isdigit((unsigned char)*p)||(*p=='.'&&!point)) { if(*p=='.') point=true; else digit=true; ++p; }
    if(!digit || p-number>=40) return Fail("Invalid numeric word");
    char value_text[40]={}; std::memcpy(value_text,number,p-number);
    const char* q=value_text; double sign=1,v=0,fraction=0.1; bool fractional=false;
    if(*q=='-'||*q=='+') { if(*q=='-') sign=-1; ++q; }
    for(;*q;++q)
    {
      if(*q=='.') { fractional=true; continue; }
      if(fractional) { v+=(*q-'0')*fraction; fraction*=0.1; }
      else v=v*10+(*q-'0');
    }
    v*=sign;
    if(!std::isfinite(v) || std::fabs(v)>10000000) return Fail("Invalid numeric range");
    if(letter!='G'&&letter!='M'&&letter!='N')
    { if(have[letter-'A']) return Fail("Duplicate word"); have[letter-'A']=true; }
    bool keep=true;
    switch(letter)
    {
      case 'N': keep=false; if(v<0||v!=std::floor(v)) return Fail("Invalid line number"); break;
      case 'X': target.x=v; axes=true; keep=false; break;
      case 'Y': target.y=v; axes=true; keep=false; break;
      case 'Z': target.z=v; axes=true; keep=false; break;
      case 'I': ai=v; mi=true; keep=false; break;
      case 'J': aj=v; mj=true; keep=false; break;
      case 'R': ar=v; mr=true; keep=false; break;
      case 'F': if(v<0) return Fail("Feed cannot be negative"); feed=v; break;
      case 'S': if(v<0) return Fail("Negative spindle speed"); break;
      case 'T': return Fail("Tool selection requires new map");
      case 'P': if(v<0) return Fail("Negative P value"); parameter=v; break;
      case 'G':
        if(Equal(v,0)||Equal(v,1)||Equal(v,2)||Equal(v,3))
        { if(motion_seen) return Fail("Conflicting motion codes"); motion_seen=true; block_motion=int(v); keep=false; }
        else if(Equal(v,21)) metric=true;
        else if(Equal(v,90)) absolute=true;
        else if(Equal(v,91.1)||Equal(v,17)||Equal(v,94)||Equal(v,40)||Equal(v,54)) {}
        else if(Equal(v,49)) { if(map.context.tlo) return Fail("G49 would change mapped Z"); }
        else if(Equal(v,80)) { block_motion=-1; keep=false; }
        else if(Equal(v,4)) dwell=true;
        else return Fail("Unsupported G mode; use G21/G90/G17/G94/G54");
        break;
      case 'M':
        if(v!=std::floor(v)) return Fail("Invalid M code");
        if(v==0||v==1||v==2||v==30) { has_end=true; if(v==2||v==30) ended=true; }
        else if(v==64||v==65) immediate_output=true;
        else if(v!=3&&v!=4&&v!=5&&v!=7&&v!=8&&v!=9) return Fail("Unsupported M code or tool change");
        break;
      default: return Fail("Unsupported word");
    }
    if(keep)
    {
      int k=std::snprintf(prefix+used,sizeof(prefix)-used,"%c%s",letter,value_text);
      if(!Fits(k,sizeof(prefix)-used)) return Fail("Control block too long");
      used+=k;
    }
  }
  if(have['P'-'A'] && !dwell && !immediate_output) return Fail("P supported only for G4 or M64/M65");
  if(immediate_output && (!have['P'-'A'] || axes || mi || mj || mr || dwell)) return Fail("M64/M65 requires P and no motion");
  if(immediate_output && (parameter!=std::floor(parameter)||parameter>255)) return Fail("Invalid digital output number");
  if(dwell && (axes||mi||mj||mr||motion_seen)) return Fail("Dwell mixed with motion");
  if(has_end && (axes||mi||mj||mr)) return Fail("Pause/end mixed with motion");
  if(axes || mi || mj || mr)
  {
    if(!metric||!absolute) return Fail("File must select G21 and G90 before motion");
    if(block_motion<0) return Fail("Motion mode missing");
    if(block_motion && feed<=0) return Fail("Cutting move has no feed");
    output_motion=block_motion==0 ? 0:1;
    if(block_motion<2)
    { if(mi||mj||mr) return Fail("Arc centre on a linear move"); if(!PrepareLinear(current,target)) return false; }
    else if(!PrepareArc(current,target,block_motion==2,mi,ai,mj,aj,mr,ar)) return false;
  }
  motion=block_motion; prefix_ready=prefix[0]!=0; return true;
}

int Compensator::Next(char* out,size_t size)
{
  if(error) return -1;
  if(prefix_ready)
  { prefix_ready=false; if(!Fits(std::snprintf(out,size,"%s",prefix),size)) { Fail("Output buffer too small"); return -1; } return 1; }
  if(!linear_pending && arc_step<arc_steps)
  {
    ++arc_step; double u=double(arc_step)/arc_steps;
    Position target=block_end;
    if(arc_step<arc_steps) { target.x=cx+radius*std::cos(angle+sweep*u); target.y=cy+radius*std::sin(angle+sweep*u); target.z=block_start.z+(block_end.z-block_start.z)*u; }
    if(!PrepareLinear(current,target)) return -1;
  }
  if(!linear_pending) return 0;
  double next=std::min(1.0,t+dt);
  double dx=line_end.x-line_start.x,dy=line_end.y-line_start.y;
  // Break every mesh boundary: each output chord lies inside one bilinear cell.
  if(std::fabs(dx)>EPS) for(unsigned i=1;i+1<map.nx;++i)
  { double at=(map.X(i)/1000.0-line_start.x)/dx; if(at>t+EPS&&at<next) next=at; }
  if(std::fabs(dy)>EPS) for(unsigned i=1;i+1<map.ny;++i)
  { double at=(map.Y(i)/1000.0-line_start.y)/dy; if(at>t+EPS&&at<next) next=at; }
  Position point{line_start.x+dx*next,line_start.y+dy*next,line_start.z+(line_end.z-line_start.z)*next};
  double surface=0;
  if(!map.Correction(point.x,point.y,surface)) { Fail("Compensation leaves map"); return -1; }
  double z=point.z+surface;
  char sx[24],sy[24],sz[24];
  if(!Fixed4(point.x,sx,sizeof(sx))||!Fixed4(point.y,sy,sizeof(sy))||!Fixed4(z,sz,sizeof(sz)))
  { Fail("Output coordinate out of range"); return -1; }
  int k=std::snprintf(out,size,"G%dX%sY%sZ%s",output_motion,sx,sy,sz);
  if(!Fits(k,size)||k>72) { Fail("Output line too long"); return -1; }
  current=point; t=next;
  if(t>=1-EPS) { current=line_end; linear_pending=false; }
  return 1;
}
}
