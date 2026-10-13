declare name "Entropy Dynamic Recovery";
declare author "Entropy Audio";
declare description "Full-band stereo-linked transient recovery and optional upward expansion";

// Faust is the source of the active FP64 signal path. The C ABI wrapper owns
// bypass, configuration validation and lifecycle.
sr = min(192000.0, max(1.0, fconstant(int fSamplingFreq, <math.h>)));
epsilon = 1.0e-12;
ln10 = 2.30258509299404568402;

transient_strength = hslider("transient_strength", 0.35, 0.0, 1.0, 0.001);
onset_threshold_db = hslider("onset_threshold_db", 2.0, 0.0, 12.0, 0.01);
max_attack_boost_db = hslider("max_attack_boost_db", 2.0, 0.0, 6.0, 0.01);
expansion_enabled = hslider("expansion_enabled", 0.0, 0.0, 1.0, 1.0);
expansion_ratio = hslider("expansion_ratio", 1.15, 1.0, 1.5, 0.001);
expansion_threshold_dbfs = hslider("expansion_threshold_dbfs", -18.0, -36.0, -6.0, 0.01);
max_expansion_boost_db = hslider("max_expansion_boost_db", 1.5, 0.0, 3.0, 0.01);
max_total_boost_db = hslider("max_total_boost_db", 3.0, 0.0, 6.0, 0.01);
wet_mix = hslider("wet_mix", 1.0, 0.0, 1.0, 0.001);

coefficient(ms) = exp(-1.0 / (0.001 * ms * sr));
envelope(x, attack, release) = e letrec {
  'e = x + ((x > e) * attack + (x <= e) * release) * (e - x);
};

process(l, r) = l * gain, r * gain with {
  activity = max(abs(l), abs(r));
  fast = envelope(activity, coefficient(0.3), coefficient(15.0));
  slow = envelope(activity, coefficient(20.0), coefficient(180.0));
  transient_db = (slow >= 0.0017782794100389228) *
    min(max_attack_boost_db,
        transient_strength * max(0.0, 20.0 * log((fast + epsilon) / (slow + epsilon)) / ln10 - onset_threshold_db));
  level_db = 20.0 * log(max(slow, epsilon)) / ln10;
  expansion_db = expansion_enabled * min(max_expansion_boost_db,
    max(0.0, (expansion_ratio - 1.0) * (level_db - expansion_threshold_dbfs)));
  target_db = min(max_total_boost_db, max(0.0, transient_db + expansion_db));
  smooth_db = envelope(target_db, coefficient(0.5), coefficient(20.0));
  gain = 1.0 + wet_mix * (exp(smooth_db * ln10 / 20.0) - 1.0);
};
