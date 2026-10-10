# 02 — Transport, stage 1 stand-in

Stage 5 replaces this whole note with the electromechanical model (motor,
capstan, inertia, elastic tape, stick-slip). Until then:

## Capstan speed

    τ_m · dv/dt = v_target − v,   τ_m = 0.35 s

A first-order lag, so a speed change glides and every echo on the tape bends in
pitch the way a real one does. 0.35 s is the order of a studio capstan servo's
settling (an AC synchronous drive is slower, a DC servo faster); the real figure
comes from the stage 5 motor.

Speed range 4.75–76 cm/s. The control maps log-linearly.

## Head carriage

Each playback head is a carriage on a rail. Its position follows the control
through a critically damped second-order response (natural period ~0.25 s) and
its speed is limited to **12 cm/s**. The rail runs from 1.5 cm to 24 cm
downstream of the record head; in LOOP mode it is also clamped to 85 % of the
loop length minus the erase-head offset, so no head can sit past the point where
the tape comes back round to the erase head.

The head velocity is what makes the Doppler — nothing computes a pitch shift.

## Sync

The user picks a tempo head and a note value. Target speed
`v = h_tempo / (note duration)`; if that is outside 4.75–76 cm/s the speed is
clamped and the panel reports it. Changing tempo moves the target; the motor lag
does the glide.

## Splicing

Changing the LOOP length or switching REEL/LOOP fades the wet path out over
15 ms, clears the tape, applies the change and fades back in. A real machine
cannot stretch a loop; this is a new loop.

## State exposed for later

Motor angular velocity, capstan angle, reel radii and angles, roller angles and
head positions are published every block — the panel draws them, and stage 10's
mechanical sounds will be driven by them.

Capstan radius 5 mm (so 19 cm/s is 363 rpm). A 10.5" NAB reel: hub radius
57 mm, flange 132 mm. 730 m of 50 µm tape (1.5 mil base + coating) fills
`r² = r_hub² + L·t/π` → **122 mm** full. One reel's pack radius is
`sqrt(r_hub² + ℓ·t/π)` for the length ℓ wound on it; its angular velocity is v/r.
At 19 cm/s the pack lasts 64 minutes; then a fresh pack is threaded
instantly. Only the reel radii jump — the tape under the heads is the window
ring, so no echo in flight is touched.
