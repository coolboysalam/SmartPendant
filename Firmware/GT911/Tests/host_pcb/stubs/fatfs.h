#pragma once
#include <map>
#include <string>
#include <cstring>
#include <algorithm>
#include <cstdint>
using UINT=unsigned;
enum FRESULT { FR_OK,FR_DISK_ERR,FR_NO_FILE,FR_EXIST };
enum { FA_READ=1,FA_WRITE=2,FA_CREATE_ALWAYS=4,AM_DIR=16 };
struct FIL { std::string name; size_t pos=0; bool open=false; };
struct FILINFO { unsigned fattrib=0; char fname[256]={}; };
namespace FakeFs
{
  inline std::map<std::string,std::string> files;
  inline int write_calls=0,fail_write=-1,rename_calls=0,fail_rename=-1;
  inline int read_limit=-1;
  inline bool fail_sync=false;
  inline void ResetFaults() { write_calls=rename_calls=0; fail_write=fail_rename=read_limit=-1; fail_sync=false; }
}
inline FRESULT f_open(FIL* f,const char* name,unsigned flags)
{
  if(flags&FA_CREATE_ALWAYS) FakeFs::files[name].clear();
  if(!FakeFs::files.count(name)) return FR_NO_FILE;
  f->name=name; f->pos=0; f->open=true; return FR_OK;
}
inline FRESULT f_close(FIL* f) { f->open=false; return FR_OK; }
inline FRESULT f_sync(FIL*) { return FakeFs::fail_sync ? FR_DISK_ERR:FR_OK; }
inline bool f_eof(FIL* f) { return f->pos>=FakeFs::files[f->name].size(); }
inline FRESULT f_lseek(FIL* f,unsigned pos) { f->pos=pos; return f->open ? FR_OK:FR_DISK_ERR; }
inline char* f_gets(char* out,int size,FIL* f)
{
  if(!f->open||f_eof(f)||(FakeFs::read_limit>=0&&f->pos>=size_t(FakeFs::read_limit))) return nullptr;
  auto& data=FakeFs::files[f->name]; unsigned n=0;
  while(n<unsigned(size-1)&&f->pos<data.size())
  {
    out[n]=data[f->pos++]; if(out[n++]=='\n') break;
  }
  out[n]=0; return out;
}
inline FRESULT f_write(FIL* f,const void* src,unsigned size,UINT* written)
{
  *written=0;
  if(!f->open||++FakeFs::write_calls==FakeFs::fail_write) return FR_DISK_ERR;
  auto& data=FakeFs::files[f->name];
  if(data.size()<f->pos+size) data.resize(f->pos+size);
  std::memcpy(&data[f->pos],src,size); f->pos+=size; *written=size; return FR_OK;
}
inline FRESULT f_stat(const char* name,FILINFO*) { return FakeFs::files.count(name) ? FR_OK:FR_NO_FILE; }
inline FRESULT f_unlink(const char* name) { return FakeFs::files.erase(name) ? FR_OK:FR_NO_FILE; }
inline FRESULT f_rename(const char* from,const char* to)
{
  if(++FakeFs::rename_calls==FakeFs::fail_rename) return FR_DISK_ERR;
  if(!FakeFs::files.count(from)) return FR_NO_FILE;
  if(FakeFs::files.count(to)) return FR_EXIST;
  FakeFs::files[to]=FakeFs::files[from]; FakeFs::files.erase(from); return FR_OK;
}
