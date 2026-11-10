"""Pure MIDI parser/transport/recorder tests. No MIDI ports or audio opened."""
from pathlib import Path
import hashlib,json,os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[2]
APP=ROOT/'app'
SUITES={
    'midi_input':['xenolith_midi.c','xenolith_paraphony.c'],
    'sequence_midi_clock':['xenolith_sequence.c'],
    'sequence':['xenolith_sequence.c','xenolith_envelope.c'],
}
def main():
    results={}
    with tempfile.TemporaryDirectory(prefix='xenolith-midi-') as directory:
        for name,units in SUITES.items():
            test=Path(__file__).with_name('test_'+name+'.c')
            exe=Path(directory)/(name+('.exe' if os.name=='nt' else ''))
            command=[os.environ.get('CC','gcc'),'-std=c11','-O2','-ffp-contract=off','-Wall','-Wextra','-Werror',
                str(test),*[str(APP/f) for f in units],'-I'+str(APP),'-I'+str(ROOT/'vendor/fm1-base/include'),'-o',str(exe)]
            subprocess.run(command,check=True,capture_output=True,text=True,timeout=30)
            run=subprocess.run([str(exe)],check=True,capture_output=True,text=True,timeout=60)
            try:results[name]=json.loads(run.stdout)
            except json.JSONDecodeError:results[name]={'passed':True,'output':run.stdout.strip()}
    print(json.dumps({'passed':all(v.get('passed',True) for v in results.values()),'suites':results,
        'scope':'Host parser, ownership, control timing and recording only; no physical MIDI/audio claim.'},indent=2))
if __name__=='__main__':main()
