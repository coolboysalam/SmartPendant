#include "GrblComm.h"
#include <cstring>
#include <cctype>

// Strictly validate complete XYZ fields before permitting PCB operations.
// Do not change the legacy DRO parser's behavior for other screens.
bool GrblComm::PcbValidAxes(const char* p)
{
  if(!p) return false;
  for(unsigned axis=0;axis<3;++axis)
  {
    if(*p=='+'||*p=='-') ++p;
    bool digit=false,point=false; unsigned count=0,whole=0;
    while(std::isdigit((unsigned char)*p)||(*p=='.'&&!point))
    {
      if(*p=='.') point=true;
      else
      {
        digit=true;
        if(!point) { whole=whole*10+(*p-'0'); if(whole>2000) return false; }
        else if(whole==2000&&*p!='0') return false;
      }
      ++p; if(++count>16) return false;
    }
    if(!digit) return false;
    if(axis<2) { if(*p++!=',') return false; }
  }
  return *p==0 || ((*p=='>'||*p==']')&&p[1]==0);
}

void GrblComm::PcbParseModal(const char* data)
{
  pcb_wcs=0;
  const char* closing=std::strrchr(data,']');
  if(!closing||closing[1]) { pcb_parser_valid=false; return; }
  for(const char* p=data;*p;++p)
  {
    if((p==data||p[-1]==' ')&&p[0]=='G'&&p[1]=='5'&&p[2]>='4'&&p[2]<='9'&&
       (p[3]==0||p[3]==' '||p[3]==']')) pcb_wcs=50+p[2]-'0';
  }
  pcb_parser_valid=pcb_wcs!=0;
}

Result GrblComm::RefreshPcbContext(uint32_t& id)
{
  mutex.Lock();
  pcb_parser_valid=false; pcb_tlo_valid=false; pcb_homed=false;
  // Use the normal polling path so status request timestamps stay coherent.
  status_request_command=CMD_STATUS_REPORT_ALL;
  mutex.Release();
  return SendCmd("$G\r",id);
}

bool GrblComm::GetPcbContext(int32_t* work,int32_t* offset,uint32_t& tool,
                            int32_t& tlo,uint32_t& epoch)
{
  mutex.Lock();
  bool valid=IsInControl() && grbl_status==Status_OK && !grbl_awaitWCO &&
    (grbl_state==IDLE||grbl_state==RUN||grbl_state==HOLD||grbl_state==DOOR) &&
    IsMetric() && number_of_axis==3 &&
    GetModeOfOperation()==MODE_OF_OPERATION_MILL && pcb_parser_valid &&
    pcb_wcs==54 && pcb_position_valid && pcb_wco_valid && pcb_tlo_valid &&
    axis_str[0][0]=='X' && axis_str[1][0]=='Y' && axis_str[2][0]=='Z';
  if(valid)
  {
    for(unsigned a=0;a<3;++a)
    {
      offset[a]=grbl_offset[a].ToFixedPoint(1000u);
      work[a]=(grbl_useWPos ? grbl_position[a]:grbl_position[a]-grbl_offset[a]).ToFixedPoint(1000u);
    }
    tool=grbl_tool; tlo=grbl_tool_length_offset[AXIS_Z].ToFixedPoint(1000u); epoch=pcb_epoch;
  }
  mutex.Release();
  return valid;
}

bool GrblComm::PcbMachineReady()
{
  mutex.Lock();
  bool ready=pcb_homed && pcb_soft_limits && IsHomingEnabled() &&
             (status_report_options & 48u)==48u; // $10: pin state + WCO
  mutex.Release();
  return ready;
}
