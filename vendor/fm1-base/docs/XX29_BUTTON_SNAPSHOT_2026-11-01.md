# Coherent foreground button snapshot — 1 November 2026

Local fork parent revision: `83e48458ab62b025a21bf537064e73ed2001d718`.
No remote repository, firmware package, USB/MIDI framing or physical device is
changed by this patch. The integrating application must deliberately opt in.

The observed trigger was a 120-ms HOME press missed during a rapid 64-cell/Q7
emulator scene. Audio continued and the old page remained selected. This is
evidence for a foreground input observation gap, not proof that the matrix ISR
failed or that this change fixes every source of foreground delay.

The BSP already latches `fm1_in.pressed` in its scan ISR. `plat_buttons()` exposes
only the held level, so a press and release entirely between foreground polls
can vanish from that interface. The new opt-in API is:

```c
uint32_t held, pressed;
plat_button_snapshot(&held, &pressed);
```

Both outputs use `B_*` bits. The implementation reads physical held and pressed
and clears pressed inside one short `fm1__lock()`/`fm1__unlock()` section. It then
maps both through `panel.btn[]`. Reading the latch and held level in separate
critical sections can instead report the same transition twice when an IRQ
arrives between those reads. A mock injects precisely that boundary condition.

Physical presses survive a release before the next snapshot. Physical edge bits
are consumed once; held buttons do not repeat. Multiple complete presses of the
same button between polls coalesce into one bit: this API is not a counted queue.
The released latch is left intact. Applications should have one snapshot owner
and must not concurrently consume the same BSP edges with `fm1_input_edges()`.

Remote and mirror inputs retain their existing foreground held-level contracts.
Their union contributes rising bits when the previous combined snapshot did not
already show the button held. Coincident sources produce one pressed bit; a
virtual press while the button is already held produces no second action.
A new latched physical press remains an event even if another source is held.
An entire remote/mirror down/up cycle processed before either level is sampled
is not newly queued by this API. No protocol or acknowledgement changed.

Call only from foreground, with IRQs initially enabled and outside an enclosing
interrupt lock. The BSP helper unconditionally re-enables IRQs; this is not an
ISR-safe or nesting-safe entrance. The device main loop calls `app_frame()` and
`app_input_only()` directly in foreground. Mapping, virtual-level reads and
application work occur after IRQs are restored. Either output pointer may be
null, but a call still consumes the physical pressed latch.

The host defines the same public function. Script press commands latch a rising
edge before changing held state, so a completed script tap also survives a slow
application poll. Repeated press commands while held do not retrigger. Existing
browser/RFM1 level inputs continue to supply level-derived rising edges; this
change does not add an event queue to their separate input ABI. Simulated reboot
resets snapshot history and seeds held script buttons as initial press bits.
The original `plat_buttons()` remains unchanged and side-effect-free everywhere.

Run `python tests/button_snapshot_test.py`. It extracts the actual production
device/host button function bodies into temporary includes, compiles them with
`-std=c11 -O2 -Wall -Wextra -Werror`, and runs narrow IRQ/panel/input mocks.
**566 assertions pass**, covering missed short presses, no-repeat held levels,
matrix/calibrated mapping, physical/remote/mirror unions, duplicate snapshots,
virtual handoff, null outputs, old-reader side effects, and IRQ-arrival timing.
The general test runner includes this check. No target build or physical timing
claim follows from the mock checks; the original rapid-control scene must be
rerun after application integration and a fresh package build.
