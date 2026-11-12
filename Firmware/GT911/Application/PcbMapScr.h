#ifndef OMID_PCB_MAP_SCREEN_H
#define OMID_PCB_MAP_SCREEN_H
#include "DevCore.h"
#include "IScreen.h"
#include "DataWindow.h"
#include "Menu.h"
#include "PcbFiles.h"
#include "PcbProbeSequence.h"

class PcbMapScr : public IScreen
{
public:
  static PcbMapScr* GetInstance();
  Result Setup(int32_t y,int32_t height) override;
  Result Show() override;
  Result Hide() override;
  Result TimerExpired(uint32_t interval) override;
  Result ProcessCallback(const void* ptr) override;
  void Stop();
private:
  PcbMapScr() = default;
  enum Phase { Idle,QueryModal,QueryParameters,Scanning,Processing } phase=Idle;
  enum { WIDTH,HEIGHT,COLS,ROWS,CLEARANCE,TRAVEL,FEED,PARAM_COUNT };
  int32_t values[PARAM_COUNT]={50000,40000,11,9,1000,2000,30};
  int32_t origin_x=0,origin_y=0;
  bool origin_set=false,verify=false,visible=false,file_menu=false;
  unsigned confirm=0,edited=0;
  uint32_t command_id=0,command_started=0,command_timeout=0;
  Pcb::HeightMap map;
  Pcb::ProbeSequence sequence;
  Pcb::ProbeCommand probe_command;
  PcbFileProcessor* processor=nullptr;
  DataWindow params[PARAM_COUNT];
  String captions[PARAM_COUNT];
  MultiLineString status;
  UiButton origin_btn,load_btn,verify_btn,file_btn;
  char status_text[160]={};
  char rendered_status[160]={};
  char dialog_text[256]={};
  Menu files;
  static constexpr unsigned MAX_FILES=16;
  Menu::MenuItem items[MAX_FILES];
  char file_names[MAX_FILES][64]={};
  unsigned file_count=0;
  void Refresh();
  void Message(const char* title,const char* text);
  void Begin(bool verify_only);
  void Fail(const char* reason,bool cancel_motion=true);
  bool Submit(const Pcb::ProbeCommand& command);
  bool CommandComplete();
  void OpenFiles();
  void StartFile(unsigned index);
  void CloseFileMenu();
  static Result SelectFile(PcbMapScr* self,void* index);
  static Result CancelFile(PcbMapScr* self,void* ptr);
};
#endif
