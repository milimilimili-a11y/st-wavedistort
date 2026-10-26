# st-wavedistort - NTS-3 kaoss pad kit effect

A sine waveshaper / wavefolder, ported from the Pure Data patch `st-wavedistort.pd`.

## What the patch does

Traced from its `workings` subpatch (the patch is stereo: two identical copies, one per channel):

    k = 0.1 * 50^strength     (strength 0..1, log scale 0.1 .. 5)
    g = 0.1 * 100^gain        (gain 0..1, log scale 0.1 .. 10)
    y = 0.2 * g * sin( (2*pi/3.14) * k * x )

The patch builds this from `abs~`, `/~ 3.14`, `+~ 0.25`, `cos~` and an `expr~ $v1>0` sign flip;
those combine into a plain sine of the scaled input. Small signals are passed through nearly
linearly, larger ones are folded back on themselves, so higher STRENGTH gives more harmonics.
The `*~ 0.6` in the patch is only the initial value; the STRENGTH slider replaces it.

## Controls

| Control | Parameter | Range | Notes |
|---|---|---|---|
| Pad X | STRENGTH | 0.000 - 1.000 | the patch's `strength` slider (default 0.490) |
| Pad Y | GAIN | 0.000 - 1.000 | the patch's `gain` slider (default 0.490) |
| FX depth strip | MIX | 0 - 100 % | not in the patch; 100 % is exactly the patch (default 100 %) |

Change the assignments on the device (or in `header.c`, `default_mappings`) if you prefer another layout.

## Output level

Output is bounded by `0.2 * g`, so at the maximum GAIN (g = 10) the output can reach 2.0 (+6 dBFS).
That is how the patch behaves, so it is kept. Keep GAIN moderate if the next stage clips.

## Build

Use the GitHub Actions workflow in the parent folder (it builds this unit as `st_wavedistort.nts3unit`),
or `./run_cmd.sh build nts-3_kaoss/wavedistort` inside Korg's logue-sdk Docker image.

`test_effect.cc` compares the real unit code against an op-by-op transcription of the patch;
the host build command is in its header comment.
