# ROOTS DUB isolator — standalone DSP module

This increment adds a fixed-point C implementation and a host test, without
connecting it to the FM-1 master path yet.

## Signal topology

The signal is split successively at 120 Hz, 350 Hz and 2.5 kHz. Each split
uses two cascaded one-pole low-pass sections. The high output is the exact
sample-domain residual (input minus low), so low plus high reconstructs the
input before band gains, apart from integer rounding. This complementary
residual topology avoids assuming independently designed low/high filters sum
flat. It is a conservative first implementation, not a Linkwitz–Riley or
steep 24 dB/octave DJ isolator.

All arithmetic is integer/fixed-point. Gains are Q15 (32768 = unity; 0 = mute).
The module clamps input/output to +/-131071 to keep accumulation bounded.
When disabled, it returns the input bit-for-bit and leaves filter state alone.

## Limitations before integration

- Coefficients are for 44.1 kHz only.
- Cutoff slopes are gentle (two cascaded one-pole sections).
- Mono per call: instantiate one state per stereo channel.
- Parameter controls, smoothing, persistence and master-bus hook are not added.
- The standalone test does not replace SLOOP's full regression suite.

## Host test

From repository root:

    cc -std=c99 -Wall -Wextra -Ifirmware/src tests/roots_isolator_test.c -o /tmp/roots_isolator_test
    /tmp/roots_isolator_test

Tests cover bit-identical bypass, DC reconstruction at unity, all-band mute,
TOPS rejection of DC, and bounded full-scale stress.
