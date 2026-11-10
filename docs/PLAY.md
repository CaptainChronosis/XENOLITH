# XX29 / Xenolith - playing guide

Xenolith is a cell-based FM instrument with a shared signal path. Begin with HOME,
then open the individual sections as needed. The controls below describe the
current instrument; release and hardware verification live in their own receipts.

## Start, stop and navigation

The FM-1 keys play; release enters ADSR release. OCT-/OCT+ change octave.
The octave offset ranges from -3 to +3; tonic remains 20-800 Hz.
This release includes the firmware and updater, not a native emulator launcher.

**PLAY** starts/stops an enabled sequence and flashes green at quarter-note tempo.
With SEQ disabled it toggles a steady-green drone. REC stays lit while armed or recording.
When not recording, REC pulses on every page once smoothed LOAD stress rises
above onset. Pulses become quicker and irregular with stress, with visible OFF
intervals even during repeated deadline intervention. The small precursor colour
alone does not light REC. Recording keeps priority. Hold FX and press REC for
LOAD ENGINE; see the control guide
and current lamp behaviour.
**REC** arms/stops a take, enabling SEQ if needed. The default REPLACE mode waits
for the first fresh key, then records one timed phrase and continues playback.
It uses the visible ARP assignment or the parameter being edited. Stopped,
one-key-per-position entry remains available as STEP in SYSTEM detail.
**HOME+REC always panics**, stopping transport/REC and clearing owned sound/tail
state while preserving the recorded take. A fresh key or PLAY resumes playing.
Short HOME leaves the page without resetting values, held notes or tails.

Boot is stopped: 84 Hz / index 2.4 / ratio 1.2 / drift 0.20, eight cells, 44.1 kHz,
FRAC BITS 24 (native bypass), 25% wet. MOD/SEQ, filter and three custom contours are off, keys are TONIC/CHROMATIC,
colour 0 and growth PARALLEL/LINEAR. The battery/USB indicator is coarse, not a calibrated percentage.

## HOME and growth

| Knob | HOME |
|---|---|
|K1|TONIC 20-800 Hz|
|K2|FM INDEX 0-64; in LINEAR this is deviation relative to cell tonic|
|K3|MOD RATIO 0.001-32|
|K4|DRIFT 0-1; nominal +/-2% at default 0.20, with noise-curve overshoot|

Rotate SELECT through **HOME  ->  TONIC  ->  INDEX  ->  RATIO  ->  DRIFT** detail pages.
Within a detail, SELECT press or HOME returns to HOME. K1 edits response and
K2 sensitivity, from 0.25x to 4x. The remaining knobs are:

|Detail|K3|K4|
|---|---|---|
|TONIC|GLIDE 0-2000 ms|MONO / 2-8 paraphonic voices|
|INDEX|INDEX / DEPTH ST|VARIANT: NORMAL / RETURN - / RETURN +- / CASCADE|
|RATIO|RATIO|SNAP: OFF / 12-TET / RATIOS|
|DRIFT|INTENSITY|SPREAD 0-2x|

MONO, zero glide and SPREAD 1 preserve the established defaults. SPREAD changes
the tuning width separately from random DRIFT. Live paraphony shares the ADSR
and effects and requires GLOBAL TONIC/CHROMATIC, SEQ disabled, and PARALLEL or
independent SELF growth. Connected mixed arrays use mono. Other routes retain
the request and use mono. Request eight voices
with three cells and three are available; restoring eight cells restores the
eight-voice capacity. Controls and verification.

ALGORITHM selects 1-64 cells. Each cell retains a local modulator/carrier pair;
this is one instrument with a shared envelope, not 64 keyed voices. In
PARALLEL, counts 1-16 retain their preceding tuning. Counts 17-32 smoothly
widen the total field from about 39 cents at 16 cells to 150 cents at 32.
Above 32, a cubic curve opens the field to six octaves at 64. This gives the
middle counts more audible thickness before the broad upper-count spread.
CHAIN4, BRANCH, LOOP4 and SELF all keep compact additive tuning at every count:
small 0.46 Hz offsets remain, but count alone does not select the wide field.
Explicit SPECTRAL-keyboard frequencies still take precedence.
Exact tuning curve and comparison renders.
Within the same effective clock, existing cells keep their phase and drift
state while the added/removed suffix fades over about 5 ms. Tuning layout
settles over about 20 ms. A real clock or topology boundary still uses the
protected transition; this is not a whole-bank restart on every count notch.

**Hold HOME 650 ms** to open ARRAY. CURRENT is the new-patch default:

|K1 ADD TYPE|K2 MODULATION|K3 LINK|K4 FEEDBACK|
|---|---|---|---|
|PARALLEL / SERIES / BRANCH / LOOP / SELF|LINEAR / PHASE / EXP / PULSE FM / PHASE WARP|0-4, forward links|0-0.95, delayed returns|

K1 chooses the next addition; ALGORITHM adds/removes the newest cells. It does
not rewrite the existing graph. SELECT rotation chooses AUTO or a source cell;
SELECT press switches CURRENT/GLOBAL. AUTO SERIES extends the preceding cell,
BRANCH takes the oldest available parent in the latest component, LOOP adds a
delayed ancestral return, and SELF adds its own delayed return. Explicit
attachment also allows a SELF cell to receive a forward source. Re-adding a
removed cell uses the currently selected type/attachment. See the
mixed-array guide for the exact placement rules.

GLOBAL auditions a legacy template without erasing the CURRENT recipe. Short
SELECT returns to that recipe, including after editing GLOBAL K1. GLOBAL* marks
an edited template; **hold SELECT 650 ms in GLOBAL** to deliberately adopt it
into CURRENT. Its K1 is GROWTH:
PARALLEL retains independent pairs. CHAIN4 links carriers in groups of four;
BRANCH fans each parent's carrier into two descendants; LOOP4 adds a delayed
last-to-first return to each complete four-cell chain. BRANCH's zero-based
parent is floor((i-1)/2): 0 feeds 1/2, 1 feeds 3/4, 2 feeds 5/6. SELF returns
each cell's own previous carrier sample; it does not connect neighbouring cells.
All cells still feed the output sum. These are optional Xenolith structures,
not standard six-operator DX7 algorithms. Short HOME exits.

ARRAY K2 is the sole FM-type selector and applies to the entire graph in both
scopes. INDEX detail K4 remembers one VARIANT for each type. RETURN - reverses
delayed returns; RETURN +- alternates their polarity between receiver cells.
CASCADE reduces the audible upstream SERIES taps as LINK increases while
retaining their full modulation signals. NORMAL preserves the existing path.
These are optional and reserve their own conservative workload allowance.
Equations, examples and limits.

| Mode | What INDEX changes |
|---|---|
| LINEAR | Signed frequency deviation relative to cell tonic; it can pass through zero |
| PHASE | Carrier lookup angle, in radians |
| EXP | Exponential frequency modulation, INDEX in semitones; depth can raise the centre pitch |
| PULSE FM | Bipolar rectangular modulator switches linear frequency deviation instead of a sine |
| PHASE WARP | Modulator varies a two-slope, half-wave-symmetric phase-address warp; INDEX 0...64 gives depth 0...1 |

The sine lookup is the final waveform evaluator, not the entire oscillator.
EXP depth spans 0-120 semitones; its first nonzero detent is one semitone,
with progressively larger steps higher up. This is peak frequency excursion
in pitch units, not an additive pitch offset.
LINEAR integrates modulation into phase; PHASE adds it to the lookup angle.
For an isolated steady sine at sufficiently high sample rate, their phase
excursions are approximately INDEX/RATIO versus INDEX. Matching those depths
and the modulator phase can approach the same sound. Changing ratio, envelopes,
clock or feedback breaks that simple comparison. PULSE/WARP provide additional
spectral changes, not alias-free or vintage-circuit emulations.
Equations, primary reading and limits.

## Coupling treatment

From ARRAY, tap EDIT for COUPLING; EDIT returns to ARRAY and HOME returns home.
K1 selects OFF / BIAS / LAG / ALLPASS / COMMON BUS. K2 blends the treatment
with each cell's original local modulator. K4 offsets the treatment input.

| METHOD | K3 | Treatment |
|---|---|---|
| BIAS | DRIVE 1-16x | Bounded cubic transfer curve |
| LAG | TIME 0.02-100 ms | One-pole memory on the local modulator |
| ALLPASS | DELAY 0.25-8 samples | Frequency-dependent phase change; the value is low-frequency group delay |
| COMMON BUS | TIME 0.02-100 ms | Subtract a smoothed, normalized mean of the preceding tick's carriers |

Graph links and delayed returns retain their separate roles. OFF is the default;
DEPTH 0 restores the original path. Continuous controls settle over about 20 ms;
method changes use a protected transition. These are optional digital treatments,
not calibrated analogue circuit models. They reserve additional processing time.

## Modulation  -  LFO

Press LFO. Rotate SELECT through **A / B / RANDOM / MIX**. Press LFO again to
alternate that face's main/detail page. **SELECT press globally bypasses MOD**;
it does not select a page or bypass only the selected face. HOME exits.

|Face/page|K1|K2|K3|K4|
|---|---|---|---|---|
|A or B main|RISE s|FALL s|signed DEPTH|DESTINATION|
|A or B detail|MODE|INPUT|TRIGGER|CURVE|
|RANDOM main|RATE Hz|SPREAD|signed DEPTH|DESTINATION|
|RANDOM detail|CORRELATE|LEVELS|CLOCK|STEP / WALK output|
|RANDOM, hold LFO 650 ms|LAW|MEMORY|RENEW %|BIAS|
|MIX main|INPUT A|INPUT B|signed DEPTH|DESTINATION|
|MIX detail|GAIN A|GAIN B|OFFSET|MODE|

A/B modes are OFF, AD, GATE, CYCLE and SLEW. CYCLE runs rise/fall continuously
when MOD is on. AD uses the chosen trigger; GATE follows key/source gate;
SLEW follows a bounded input with separate rise/fall times. MODE OFF disables
that function/mixer. DESTINATION NONE removes that face's primary route.
Setting a real destination establishes its route and enables MOD; check the
visible ON/OFF before using SELECT. Base controls remain separate from offsets.

On RANDOM's held page, LAW selects CURRENT / BELL / EDGE / BINARY. CURRENT
with MEMORY OFF preserves the prior random path. MEMORY captures 2-32 events;
RENEW 0 repeats the stored phrase, while 100% replaces every visited event.
Changing length starts a fresh capture; law changes enter the phrase as slots
renew. BELL favours the centre, EDGE the ends, and BINARY selects two endpoints;
BIAS changes BINARY's upper-state probability. Correlation/slew can keep moving
after a raw phrase repeats. Short LFO returns from the held page.
Sources and mathematics.

CORE RATE, WET RATE and ECHO TIME take bounded octave offsets; CELLS takes an
additive offset. They prepare slow, coalesced changes outside the sample loop,
retaining the base knobs and waiting for admitted clock/count transitions.
After bypass, the base requests return as retiring work finishes. Held keys
retain their established priority over matching SEQ values; MOD offsets apply
to the selected base. Transaction details.

PITCH depth is semitones, RATIO/time depth is octaves, INDEX depth 1 equals 64
index units. BIT DEPTH uses signed bit offsets; other scalar depths are
normalized. A/B and RANDOM outputs are centered bipolar signals for both direct
routes and MIX inputs; negative depth reverses their direction. Internal GATE
and SLEW state remains 0...1. AMPLITUDE starts at base 1, so a positive direct
bipolar route is clipped above unity and does not give a complete 0...1 contour.

For a normal function-controlled amplitude contour: set A to CYCLE, route A
itself to NONE; set MIX input A=FUNC A, gain A=0.5, gain B=0, offset=-0.5, mode SUM.
Route MIX -> AMPLITUDE at depth +1. For raw contour A in 0...1, the result is
clamp(1+0.5x(2A-1)-0.5, 0, 1)=A, multiplied
by the traditional ADSR. With SEQ off, PLAY supplies the sustained source.

For an unequal changing contour, start A around 0.05 s rise/0.8 s fall. RANDOM can
route STEP or WALK -> FUNC A FALL at about +2 octaves, with CLOCK FUNCTION END and
moderate correlation. Each end samples a new bounded time offset. This is a
small generative patch; a wider free-routing editor is deferred.
More detail.

## Sequence and recording  -  SEQ

SEQ opens SYSTEM, TONIC NOTES and CV LANE 1...4, chosen by SELECT rotation. SELECT
press alternates two pages for each object. HOME leaves transport running.
The default phrase has sixteen beats at 120 BPM, with four positions per beat:
64 positions for notes and CV1, and sixteen positions for CV2-4. Its duration
is eight seconds. The display groups the fine positions in fours. Sequence
starts disabled, with all notes REST and lanes off.

|Object/page|K1|K2|K3|K4|
|---|---|---|---|---|
|SYSTEM main|ENABLE|BPM 20-300|BEATS 1-16|QUANT|
|SYSTEM detail|NEXT REC MODE|ARM NOTES|ARM LANES 1-4 mask|NOTE GATE ADSR / CONTINUOUS|
|TONIC NOTES main|EDIT STEP|REST / NOTE / TIE|MIDI NOTE|GATE 1-99%|
|TONIC NOTES detail|EDIT STEP|TRIGGER A|TRIGGER B|RANDOM NEXT|
|CV LANE main|TARGET|ENABLE|SLEW|VALUE in target units|
|LANE detail|EDIT STEP|VALUE in target units|STEP ON|FILL VALUES|

QUANT selects **1/4 BEAT / 1/2 BEAT / 1 BEAT**. It snaps both running recording
and existing note/CV1 playback to the nearest grid point. It never changes BPM
or phrase duration. Playback leaves stored positions intact: return to 1/4 BEAT
to hear the original placement again. When several attacks land together, the
latest original attack wins; trigger flags combine. The final grid point keeps
late events inside the phrase instead of moving them to its beginning.

At 120 BPM these grids are 125 / 250 / 500 milliseconds. Sixteen beats are four
bars of 4/4. CV2-4 advance once per beat, completing the same phrase; they are
not repeated four times. BEATS changes phrase length in whole beats. Legacy
patches with other stored clock divisions retain their timing and show SLOTS
instead. Patches using the former default 1/64 division receive the corrected
four-positions-per-beat timing. See repair and compatibility.

ADSR retriggers each NOTE and uses its GATE percentage. CONTINUOUS supplies a
drone while transport runs, including REST steps and CV-only sequences. It
bypasses ADSR with a short transition, retaining the amplifier and AMPLITUDE
modulation. Default is ADSR; the choice is saved with the patch.

The edit cursor is separate from the playhead. Lane targets include pitch,
index, EXP depth, ratio, drift, wet mix, core/wet rate, amplitude, ANS CELL, BITS,
FILTER HZ and FILTER RES; four lanes may not
own the same destination. Each lane has one SLEW control: OFF, readable
millisecond detents, then 1-60 s. It writes rise/fall together; an old asymmetric
pair shows ASYM and stays intact until edited. Editing VALUE enables that
step's lock. Retargeting preserves its normalized values and masks. These are
four control lanes alongside one note track, not four extra note instruments.
Trigger bits can act on rests/ties. Timing uses 44.1 kHz output samples, regardless
of core/wet clock changes. Ordinary TONIC/CHROMATIC live notes take priority
over a sequenced note.

To record a timed phrase:

1. Leave SYSTEM at 16 BEATS and choose BPM and QUANT. REPLACE is the default
   next recording mode; transport should be stopped.
2. Open ARP, choose TONIC/CHROMATIC and press REC there to arm only notes.
3. The first key starts the clock. Play across the phrase; timing determines
   placement, and held keys create ties. At its end recording stops and the
   completed phrase keeps playing. REC can finish capture early.
4. Change QUANT to tighten existing playback. PLAY stops; HOME only leaves the
   page. Individual notes remain editable on TONIC NOTES.
5. SAVE twice stores it with the sound. Recall never auto-starts it.

For stopped step entry, choose STEP in SYSTEM detail before REC. Each fresh
key then writes and advances one position, independently of QUANT. Four such
positions fill one beat. The edit cursor remains independent of playback.

For keyboard CV steps, select a scalar ARP assignment such as INDEX and press
REC there. A matching or unused lane is selected and only that lane is armed.
In live REPLACE/OVERDUB, the first key starts the timed phrase and later keys
write values at their timed positions. In STEP, each fresh key writes a
normalized value and advances the shared cursor.
Four occupied lanes with no matching target report NO FREE CV LANE. In ANS
CELLS mode, REC arms an ANS CELL lane: each fresh key records its position
across the 27-key span; playback selects the corresponding cell band using
the current width. ANS CELLS mode applies its RELATIVE/SPECTRAL layout;
with GLOBAL assignment the sequence selects the ordinarily tuned array.
This is one scalar band position, not a recording
of an arbitrary simultaneous mask. Stop REC before rerouting the CV lane.
STEP while running is refused; stop transport first.

For a direct stopped STEP take, REC can choose a matching/free lane when you
turn a supported parameter. The knob writes the current position without
advancing; a fresh assigned key writes and advances. You can also explicitly
select/arm lanes in SYSTEM detail. In REPLACE/OVERDUB, PLAY can start an armed
take without a key; arming during playback waits for the next loop. All lanes
record one complete phrase; CV2-4 project gestures onto their sixteen beats.
Stopped CV2-4 STEP entry still needs only sixteen fresh keys. Older playback
grids retain their original short-lane recording cycle. REPLACE replaces armed
material; OVERDUB retains untouched steps. Capture stores base gestures, not already-modulated
values. Running capture locks incompatible editor operations. Panic preserves
all writes already made. 

## Keyboard assignment  -  ARP

ARP opens assignment; SELECT press changes its page. HOME or ARP exits.

|Page|K1|K2|K3|K4|
|---|---|---|---|---|
|ASSIGN|DESTINATION (GLOBAL only)|GLOBAL / ANS CELLS|LATCH|KEY GATE|
|RANGE, GLOBAL|LOW|HIGH| - |RESPONSE|
|RANGE, ANS CELLS|LOW SPEC|HIGH SPEC|BAND WIDTH|LAYOUT|

Scalar destinations are TONIC, INDEX, RATIO, CORE RATE, WET RATE, DRIFT, WET
MIX, AMPLITUDE, BIT DEPTH, EXP DEPTH, FILTER HZ and FILTER RES. FILTER HZ defaults
to logarithmic 20-20,000 Hz, FILTER RES to linear 0-1. TONIC/CHROMATIC retains ordinary note/octave behavior;
LINEAR/LOG map the 27 keys across low/high. LOG requires low>0. LATCH retains
the assignment after release, but yields to an active matching CV lane; a held
physical key still overrides that lane. HOME numbers and bars show the effective
engine values. KEY GATE determines whether key edges operate
the main amplitude ADSR. Gate off is useful with a PLAY drone. Clock assignment stays bounded by
the effective workload policy.

ANS CELLS unions all held-key bands across the current 1-64-cell bank. All
27 keys together cover the complete bank, including at 64 cells. WIDTH 1-16
sets a minimum band width, clamped to the count. RELATIVE retains existing tuning; SPECTRAL prepares
log-spaced absolute targets between LOW/HIGH 20-8000 Hz. Keys share the master
envelope. This is optional frequency-band selection, not an ANS reconstruction
or per-cell envelope editor. 

## Envelope, colour, clocks and analysis

There are four envelopes. ENV opens ATTACK / DECAY / SUSTAIN / RELEASE, with SELECT rotation choosing
AMP / FILTER / INDEX / TONIC. AMP retains 10 ms /250 ms /80% /400 ms. The three
custom contours start disabled with zero depth, so boot sound is unchanged.
Hold ENV 650 ms for DEST / DEPTH / TRIGGER / ENABLE; short ENV returns to ADSR,
then HOME. AMP's route is fixed/read-only. Custom destinations include cutoff,
resonance, pitch, index, ratio, drift, amplitude, wet mix and bits. KEY, SEQ NOTE
or BOTH triggers retrigger from the current level; release follows the matching
gate, and panic clears all levels without erasing settings. KEY includes the
manual PLAY drone, which holds sustain until PLAY stops; keyboard release
alone does not release that latched gate. These are control
contours, not three additional audio voices. 

From plain HOME, SELECT press cycles VCA  ->  FOLDER  ->  HOME; within a HOME detail
it first returns to HOME. SELECT rotation on either VCA/FOLDER
face selects the boxed VCA curve: CUBIC / SPACE3/2 / SOFTSIGN / TRIFOLD.

|Face|K1|K2|K3|K4|
|---|---|---|---|---|
|VCA|DRIVE|BIAS|COLOUR|DEGEN|
|FOLDER|FOLD|BIAS|STAGES: 1 / 2 / 4 / 6|MIX|

COLOUR blends the VCA transfer curve; zero bypasses that branch and shows DRY. DEGEN applies
static negative feedback around the selected curve, changing its gain, headroom
and curvature. Start with COLOUR above zero and some DRIVE, then sweep DEGEN.
This follows the feedback relation in Radiotron; it is not a calibrated valve
model or simulated power-supply sag. MASTER remains a clean output control.

The independent diode folder follows the VCA. FOLD increases its input drive;
BIAS offsets the waveform against the folding thresholds. STAGES changes how
many diode folding sections act together; MIX blends the result. FOLD zero with
zero BIAS bypasses it exactly. FOLD is a modulation/envelope destination even
with COLOUR zero. TRIFOLD in the curve selector is the older static triangular
transfer, distinct from this diode cascade. The new folder uses antiderivative
antialiasing; aggressive folding can still produce aliasing.

EDIT cycles DIGITAL / FILTER / HOME. FILTER has CUTOFF Hz / MODE / TOPOLOGY /
RESONANCE. SELECT rotation chooses OFF / PRE DIGITAL / POST DIGITAL / POST FX /
PRE FX. SELECT press cycles the cutoff response: FINE (96 detents/octave),
SWEEP (24, the default), FAST (12), and LINEAR (100 Hz/detent).
PRE acts before quantization and reconstruction at the admitted internal clock;
POST DIGITAL is mono 44.1 kHz before the amplifier; PRE FX is mono 44.1 kHz
after VCA colour and before the wet/dry split. POST FX is stereo 44.1 kHz. Insertion changes briefly
fade output; parameter edits retain filter history. Cutoff adds ENV/MOD octaves
after the absolute SEQ/KEY target; resonance adds normalized offsets. Its actual
cutoff display follows those controls, capped at45% of the active filter clock.
At the PRE DIGITAL ceiling, RATE LIMIT appears. Turning K1 then starts from
that ceiling, so a downward turn responds immediately instead of spending
detents above the audible range. Clock changes alone retain the stored cutoff.
MODE offers LP/BP/HP/NOTCH/PHASE/TWIN T. TOPOLOGY chooses 2POLE/4POLE/BRIDGE.
All six responses share the same approximate loaded-triode/parallel-T feedback
cell, grounded in Radiotron Fig. 15.15. LP/BP/HP/NOTCH/PHASE use added buffered
state taps; those mode switches are our extension, not a printed book circuit.
The curvature is inside feedback, so response depends mildly on level. PHASE is
all-pass only in the small-signal limit before output coupling; BRIDGE can create
phase-cancellation notches. This is a reduced model, not a calibrated 6SL7 replica.
TWIN T returns the selective plate response. K1 becomes CENTRE Hz; its network
labels are ONE / TWO / BRIDGE. Start around 200 Hz and resonance .7 with the
84 Hz default voice; 2 kHz emphasizes a higher band. Resonance 0 removes TWIN's
selective feedback. Entering/leaving TWIN retains the protected transition.

Filter knob recording locks the current step without advancing; keyboard CV
recording advances once per fresh key. The filter starts OFF.

Hold EDIT 650 ms opens **CIRCUIT**: CATHODE R / HEADROOM / COUPLING / T BALANCE.
CATHODE R changes the loaded stage's degeneration (470-10,000 ohms, default
2,200). HEADROOM changes the input scale at which curvature becomes pronounced
(0.0625-1, default .25), with small-signal gain compensation. COUPLING controls
the output high-pass pole (.5-100 Hz, default about3.18). T BALANCE moves linked
passive ratios (.5-1, default1); lowering it fills the feedback notch. These
settings affect the common cell beneath every response.

DIGITAL has MAX KHZ / FRAC BITS / QUANT / RECON. SELECT press cycles the MAX
response: FINE (384 detents/octave), SWEEP (96, default), FAST (48), or LINEAR
(250 Hz/detent). These apply from 1-44.1 kHz; the upper choices remain exact
88.2/176.4 kHz stages, subject to workload admission. The response is a session
preference and resets to SWEEP at boot; the requested rate is saved in the patch.
FRAC BITS starts at 24 and turns down to 1.
**24 is native bypass**, the former OFF position. Values 2-23 set a fractional
grid on the summed signal, not a strict N-bit DAC with N-bit full-scale levels.
1 is a polarity quantizer with levels +/-0.5; exact zero stays zero. TRUNC and
ROUND share that polarity rule; ERROR FB provides bounded first-order density.
Older saved fractional-Q24 configurations retain their original grid.
MOD BIT DEPTH, SEQ BITS and GLOBAL key BIT DEPTH can change precision live.
Quantizers are TRUNC /ROUND /ERROR FB; lower-rate reconstruction ZOH /LINEAR.
Upper stages use FIR.
The admitted clock depends on cell count and active work. A lower requested
ceiling remains lower; the request survives temporary workload caps and returns
after retiring work is acknowledged. Costly configurations wait for a safe
rate before activation. Output stays 44.1 kHz. Current admission and verification
scope belong in the render notes, not a fixed promise
that every combination runs at the maximum stage.

The 2,048-value sine table and the folder tables reside in shared fast RAM.
Rendering kernels use internal RAM code where verified. This does not provide
another processor core for audio: CPU0 still owns the synthesis path.

Basic oversampling allows 4x at 1-8 cells, 2x at 9-16, and 1x above 16. Extra
workload policies can lower the nominal admitted rate. 

### LOAD ENGINE

Hold FX and press REC. SELECT rotation sets ONSET; the four knobs are SPAN,
FILTER, REVERB and VCA BIAS. SELECT press opens timing: ONSET / SPAN / RECOVER /
CURVE. Hold REC while inside LOAD to reach the core controls; SELECT press then
alternates table erosion and cell/link/clock limits. HOME exits without recording.

New defaults are ONSET 60%, SPAN 40 percentage points, RECOVER 250ms and VCA BIAS
-15%. ONSET covers0-100%; SPAN independently covers5-50 percentage points.
A very small colour precursor rises
through the ten percentage points immediately below ONSET; below that it returns
to bypass. This precursor never reduces the core clock or lookup precision.
The late CLOCK FLOOR defaults to37.5% and covers25-100%. Existing patches
retain their saved LOAD settings.

The filter softens cutoff/resonance through its existing circuit; the tank gains
damping and gradually reduces two input diffusers. VCA BIAS also controls a small
signal-driven supply reservoir: sustained demand reduces available swing, then
recovers with an approximately80ms time constant. Zero VCA BIAS disables this
colour after recovery. The independent folder retains its drive, stages and
antialiasing. Core calculation duty begins changing only at65% of the configured
stress range; lookup interpolation loss begins at80%. Late stress includes
measured deadline headroom so relief can precede emergency sample holds.
Requested MAX is retained,
including on the 2x/4x paths. Sequence/modulation time stays at 44.1kHz. This is
CPU-driven sound shaping with a simplified averaged VCA reservoir, not a complete
valve supply or rectifier simulation. Independent deadline
protection remains active. REC displays stress when it is not showing recording.

FX cycles ECHO MS / DAMP HZ / TAIL S / MIX  ->  WET KHZ / STRUCTURE / DIFFUSION / WIDTH  ->  HOME.
STRUCTURE selects TANK / CLOUD4 / CLOUD8 / RING / PAIRS. SELECT within FX toggles
ZOH/LINEAR reconstruction without taking a knob. CLOUD uses early taps from
the existing tank; RING and PAIRS change the feedback connections. All use the
same delay memory. Their 50 ms transitions retain the running tail.
DIFFUSION adjusts the existing input allpasses, 0-0.85 with default
0.65. WIDTH moves from mono at 0% to the existing stereo projection at 100%.
Hold FX 650 ms for **INPUT / TANK A / TANK B / SCALE**. INPUT is the existing
input diffusion control. TANK A/B add short normalized allpasses inside the two
groups of four feedback legs; both default to zero/bypass. SCALE selects quarter,
half or full short-delay geometry. It retires those small histories through
bypass, preserving the long reverb tank. These controls change tail dispersion,
not a literal room size or physical spring model.

ECHO covers 2 ms up to the available memory:
1 s at 44.1 kHz, 2 s at 22.05 kHz, up to 32 s at the lowest wet clock. A larger
stored request survives a clock cap. Sweeps move the read head, bending pitch
and comb resonances; those effects alone do not establish aliasing.
DAMP requests 20-16,000 Hz, capped at 45% of the current wet clock. The higher
request returns when the clock rises. Old patches retain their previous damping
law until DAMP is edited. TAIL is nominal low-frequency feedback-network T60;
the input allpasses and moving echo can make the complete decay longer.
Wet rates span 1.378125-44.1 kHz, independently of the core; lower rates add
sampling colour and lengthen fixed tank paths. Compatible core/wet rate changes
retime over 50 ms without resetting state; structural changes retain protection.
Hold GLO 650 ms opens **LOOKUP**: TABLE / INTERP / HARMONIC / TARGET.
TABLE selects4-2,048 points in powers of two; INTERP is ZOH (zero-order hold)
or LINEAR. HARMONIC blends toward the third harmonic; TARGET selects MOD / CAR /
BOTH. Defaults2,048 /LINEAR /0% /BOTH retain the original waveform path. This
changes waveform evaluation before FM; FRAC BITS still quantizes the summed
signal later. PULSE FM's rectangular modulator does not use the sine table.

GLO shows measured spectrum/phase. GLO -> EDIT edits SPAN / TAP / FFT FPS / FFT
SIZE. PRESETS on GLO selects L/R/MID. SCL toggles spectrum-driven note LEDs.
CPU1 handles regular 1K analysis only; it supplies no extra audio-engine budget.

## MIDI connections

USB MIDI from a computer and the TRS MIDI input receive notes on all channels. Note On plays
TONIC through the shared ADSR; Note Off releases the corresponding note. A
Note On with velocity zero is also a release. All other velocities use the
normal fixed level. The tonic range remains 20-800 Hz, so notes outside it clamp
to the nearest limit. Where the current array permits paraphony, external notes
use the requested 1-8 voice allocation.

ARP parameter assignments apply to the FM-1 panel keyboard only. MIDI notes
always enter the tonic path and can be recorded into the tonic sequence as
MIDI note numbers. They do not enter keyboard-assigned CV lanes.

For external timing, enable SEQ, then send MIDI Start or Continue followed by
24-PPQN MIDI Clock. The first clock pulse begins playback. Clock pulses alone
do not start a stopped sequence. Start begins at the phrase start; Continue
resumes the saved transport position. The BPM area shows EXT while externally
running. MIDI Stop or loss of clock stops the sequence but retains held live
notes. Pressing local PLAY returns transport to the internal BPM.

CC120 and CC123 release notes belonging to their originating port and channel.
Other CC messages, pitch bend, sustain and expressive velocity are not mapped.
There is no MIDI output or thru, and the TRS jack is input only. Bluetooth and
direct USB-keyboard hosting are disabled. USB reconnection clears USB-owned
notes; if a disconnected controller leaves a held note, HOME+REC is the panic.

USB also carries installation, backup, diagnostics and the remote-panel
protocol. These remain separate from performance MIDI. Candidate-specific
hardware checks are recorded with the release evidence.

## Save and reopen

PRESETS selects slot 01-16 without recall. PLAY on the patch page loads an
occupied slot; SAVE, choose slot, SAVE confirms. HOME cancels an unconfirmed
save. Saving uses acknowledged quiet foreground ownership and writes one atomic
slot; no flash/UI work occurs in the callback. V25 uses a 1,712-byte patch in a
1,740-byte V21 atomic slot object, loaded on demand. It stores the mixed graph,
five remembered variant choices, the optional coupling treatment,
HOME response/paraphony settings, sound, MOD, sequence including NOTE GATE,
key assignment, amplifier shape, topology, filter, all four envelope settings,
FX structure/damping/dispersion, random-memory controls, circuit settings and
oscillator-table settings, dedicated folder, VCA DEGEN and LOAD ENGINE controls.
Notes and CV1 retain all 64 positions; CV2-4 retain 16. It omits held keys,
live transport, oscillator phase, the captured random phrase and tank audio. Recall starts transport/REC stopped.
Earlier formats migrate in RAM with their strict old limits; recall never
automatically writes the new format. [Schema source](../app/xenolith_patches.h).

For installation, use [Update.bat](../Update.bat) and [INSTALL.md](../INSTALL.md).
Preserve valued patches before changing firmware; third-party updates may erase
saved banks. Package and device verification are recorded in
[evidence/BUILD.json](../evidence/BUILD.json).
