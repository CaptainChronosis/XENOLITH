"""Exercise the actual foreground MIDI pump and audio-owned queues with mocked ports."""
from datetime import datetime
from pathlib import Path
import hashlib,json,re,subprocess
def function(code,name):
    found=re.search(r'^static\s+[^;\n]+\b'+name+r'\([^;]*?\)\s*\{',code,re.M)
    if not found:raise RuntimeError('Function not found: '+name)
    start=found.start();at=code.index('{',found.start());depth=1;end=at+1
    while depth:
        depth+=(code[end]=='{')-(code[end]=='}');end+=1
    body=code[start:end]
    return body[:body.index('{')].strip()+';',body


PROJECT=Path(__file__).resolve().parents[2]
SOURCE=PROJECT/'app'

def main():
    out=PROJECT/'sessions'/datetime.now().strftime('%Y-%m-%d/midi_app_host_%H%M%S_%f')
    out.mkdir(parents=True)
    app=(SOURCE/'app.c').read_text(encoding='utf-8')
    names='midi_note_pitch midi_identity_pitch live_note_gate seq_send seq_simple seq_audio_commands midi_audio_realtime physical_recorded_note midi_record_releases midi_queue_realtime midi_input'.split()
    pieces=[function(app,n) for n in names]
    command=re.search(r'typedef struct \{\s*uint32_t type,index,a,b;.*?\} SeqCommand;',app,re.S).group()
    enum=re.search(r'enum \{ SQ_CONFIG.*?\};',app).group()
    text=Path(__file__).with_name('test_midi_app_host.c').read_text(encoding='utf-8')
    text=text.replace('/* COMMAND */',enum+'\n'+command)
    text=text.replace('/* FUNCTIONS */','\n'.join(x[0] for x in pieces)+'\n'+'\n'.join(x[1] for x in pieces))
    code=out/'actual_midi_app.c';code.write_text(text,encoding='utf-8')
    modules=[SOURCE/('xenolith_'+n+'.c') for n in ('midi','sequence','paraphony','keyboard')]
    cmd=['gcc','-std=c11','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror','-Wno-unused-function',str(code),*[str(p) for p in modules],'-I'+str(SOURCE),'-I'+str(PROJECT/'vendor/fm1-base/include'),'-lm','-o',str(out/'test.exe')]
    c=subprocess.run(cmd,capture_output=True,text=True,timeout=30);(out/'compile.log').write_text(c.stdout+c.stderr)
    if c.returncode:raise RuntimeError(c.stderr)
    r=subprocess.run([str(out/'test.exe')],capture_output=True,text=True,timeout=30);(out/'run.log').write_text(r.stdout+r.stderr)
    if r.returncode:raise RuntimeError(r.stdout+r.stderr)
    result=json.loads(r.stdout);result['scope']='Actual app MIDI pump and SPSC command consumers, actual MIDI/sequence/voice allocator modules, mocked ports and envelope-event publication; no hardware timing claim.'
    result['sha256']={str(p.relative_to(PROJECT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [SOURCE/'app.c',*modules,Path(__file__),Path(__file__).with_name('test_midi_app_host.c')]}
    (out/'RESULTS.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result|{'output':str(out)},indent=2))
if __name__=='__main__':main()
