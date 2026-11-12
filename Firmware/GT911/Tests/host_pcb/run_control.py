#!/usr/bin/env python3
"""Real communication, file preparation and sender timer with HAL/SD doubles."""
from pathlib import Path
import os, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='pcb-control-') as tmp:
    stage=Path(tmp)
    for p in (ROOT/'Tests/host/stubs').glob('*.h'): shutil.copy(p,stage)
    shutil.copy(ROOT/'Tests/host_pcb/stubs/fatfs.h',stage)
    shutil.copy(ROOT/'Tests/host/sender.h',stage)
    for path in ['Application/GrblComm.cpp','Application/GrblComm.h',
                 'Application/FramedUart.cpp','Application/FramedUart.h',
                 'DevCore/Framework/Result.h','DevCore/Interfaces/IUart.h',
                 'DevCore/Math/Decimal32.h']:
        s=(ROOT/path).read_text()
        if path.endswith('GrblComm.h'): s=s.replace('private:','public:')
        (stage/Path(path).name).write_text(s)
    for name in ['PcbGrbl.cpp','PcbHeightMap.cpp','PcbHeightMap.h',
                 'PcbRuntime.cpp','PcbRuntime.h','PcbFiles.cpp','PcbFiles.h']:
        shutil.copy(ROOT/'Application'/name,stage)
    sender=(ROOT/'Application/ProgramSender.cpp').read_text()
    timer=sender[sender.index('Result ProgramSender::TimerExpired('):sender.index('Result ProgramSender::ProcessSpeedFeed(')]
    (stage/'sender_timer.cpp').write_text('#include "sender.h"\n#include "PcbRuntime.h"\n'+timer)
    sources=['GrblComm.cpp','FramedUart.cpp','PcbGrbl.cpp','PcbHeightMap.cpp','PcbRuntime.cpp','PcbFiles.cpp','sender_timer.cpp']
    binary=stage/'test-control'
    command=[os.environ.get('CXX','c++'),'-std=c++17','-O1','-g','-Wno-format','-Wno-register',
             '-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-I',str(stage)]
    subprocess.run(command+[str(stage/p) for p in sources]+[str(ROOT/'Tests/host_pcb/test_control.cpp'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True,env=dict(os.environ,ASAN_OPTIONS=os.environ.get('ASAN_OPTIONS','detect_leaks=1'),UBSAN_OPTIONS='halt_on_error=1'))
