# Codec quality experiments

## Method

The benchmark used 14 deterministic synthetic clips at 22,050 Hz: silence,
near-silence, low/mid/high tones, AM multitone, chirp, transients, speech-like
harmonics, colored noise, a pathological square/impulse signal, a decaying
tone, independent stereo, and anti-phase stereo. Each full clip contained
8,192 frames (114,688 frames total). Deep-lookahead and `best`-effort tests
used shorter representative excerpts to keep the bounded recursive searches
practical.

Every result was measured after decoding. Metrics were PCM RMS error, peak
error, high-frequency error share, and an FFT-derived A-weighted error proxy.
The A-weighted number is useful for ranking experiments, but is not a listening
test or a full psychoacoustic metric. No real-hardware listening evaluation was
performed.

## Results

| Experiment | Result | Decision |
|---|---|---|
| SDX2 perceptual trellis cost, first-order weights 0.25–1.0 | Weight 0.25 moved error upward in frequency, but increased aggregate RMS by 7.5% and A-weighted error by 2.4%. Larger weights were progressively worse. | Reject this objective. |
| Static shaping strength | A weight of 0.25 reduced the A-weighted proxy by 1.4% for SDX2 and 2.0% for ADP4, while increasing RMS by 3.0% and 1.6%, respectively. It regressed transient/pathological clips. | Worth a listening-only optional control; not a new default. |
| Dynamic-shaping minimum | Raising SDX2's minimum from -0.25 to 0 improved its aggregate A-weighted proxy by 0.75%. ADP4's best aggregate proxy occurred near -0.75, only 0.46% better than the current -0.5. Winners varied by signal. | No universal default change. |
| Stereo shaping analysis | Anti-phase stereo made shared dynamic shaping byte-identical to shaping-off because the analysis sums channels. Per-channel analysis was then tested, but worsened both stereo probes. | Do not change based on current evidence. |
| Silence deadband, 1–256 PCM units | SDX2 thresholds above 1 damaged near-silence and decaying tails. ADP4 reproduced the near-silence clip exactly without a deadband; snapping introduced error. A large deadband helped the A-weighted proxy only on a decaying tail while raising RMS. | Reject as a default. |
| SDX2 candidates per parity, 4–128 | Moving from 4 to 8 improved aggregate RMS by 0.158%. Moving from 8 to 16 improved only 0.003%; 16–128 were identical at reported precision. The 128-code case took 7.8 times as long as 8. | Existing wide-8 search is the useful limit. |
| XQ lookahead beyond 16 | SDX2 depth 20 was 5.1 times slower with no aggregate gain; depths 24 and 32 became worse as the per-sample node budget was exhausted. ADP4 depth 32 improved aggregate RMS about 0.98%, but took 21.9 times as long and regressed the speech-like clip by 3.2%. | Keep the depth-16 limit. |
| `best` search effort | Relative to `fast`, `exhaustive` improved aggregate RMS by 0.19% for SDX2 and 0.26% for ADP4, while taking 18.7 and 27.1 times as long. Individual ADP4 material occasionally gained more, up to 3.9% in this corpus. | Keep as an explicit offline tradeoff; do not expand it further. |

## Conclusion

No tested change is a robust, across-the-board quality improvement. The fixed
SDX2 codebook and ADP4 step/index format now dominate error; additional search
mostly adds cost.

The resulting quality feature is perceptual candidate selection for
`--encoder=best`. It encodes and decodes the existing candidate set, then ranks
each channel by FFT-derived A-weighted error. This mode is the default;
`--selection=rms` retains exact unweighted squared-error ranking for comparison
and reproducibility.

A static shaping weight near 0.25 remains a candidate for controlled listening
trials, but its RMS and transient regressions rule out changing encoder defaults
from these synthetic measurements alone. Real speech/music masters and 3DO
hardware remain necessary for judging subjective quality.
