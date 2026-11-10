"""Compile actual device/host button implementation bodies against narrow mocks.

No firmware build, device access, USB/MIDI changes or remote repository actions.
"""
from pathlib import Path
import hashlib
import json
import os
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]

def function(text,name):
    start=re.search(r'uint32_t\s+'+re.escape(name)+r'\(void\)\s*\{',text).start()
    brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]+'\n'

def implementation(path):
    text=path.read_text(encoding='utf-8-sig')
    begin=text.index('/* XX29_BUTTON_SNAPSHOT_BEGIN */')
    end=text.index('/* XX29_BUTTON_SNAPSHOT_END */',begin)
    return function(text,'plat_buttons')+text[begin:end]

def main():
    paths=[ROOT/'device/plat_fm1.c',ROOT/'host/plat_host.c',ROOT/'include/plat.h',Path(__file__),Path(__file__).with_suffix('.c')]
    with tempfile.TemporaryDirectory(prefix='xx29_button_snapshot_') as temp:
        directory=Path(temp)
        (directory/'device_implementation.inc').write_text(implementation(paths[0]),encoding='utf-8')
        (directory/'host_implementation.inc').write_text(implementation(paths[1]),encoding='utf-8')
        executable=directory/('button_snapshot_test.exe' if os.name=='nt' else 'button_snapshot_test')
        command=[os.environ.get('CC','gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
                 '-I'+str(ROOT/'include'),'-I'+str(directory),str(paths[-1]),'-o',str(executable)]
        flags=subprocess.CREATE_NO_WINDOW if os.name=='nt' else 0
        build=subprocess.run(command,text=True,capture_output=True,timeout=30,creationflags=flags)
        if build.returncode:raise RuntimeError(build.stdout+build.stderr)
        run=subprocess.run([str(executable)],text=True,capture_output=True,timeout=10,creationflags=flags)
        if run.returncode:raise RuntimeError(run.stdout+run.stderr)
        result=json.loads(run.stdout)
        result.update(scope='Host mocks compile actual platform button functions. No target or physical claim.',
                      source_sha256={str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths})
        print(json.dumps(result,indent=2))

if __name__=='__main__':main()
