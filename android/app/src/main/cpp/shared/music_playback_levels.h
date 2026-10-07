#ifndef DXX_MUSIC_PLAYBACK_LEVELS_H
#define DXX_MUSIC_PLAYBACK_LEVELS_H

/* Android music/effects loudness tuning
 * Tuned with tests/compare_music_loudness.py: 120-second samples of seven D2
 * bundled SC-55-like / balanced EQ MIDI tracks, eight Definitive Collection CD
 * tracks, and four D1 controls, at music volume 8
 * Current D2/CD median gap: 0.175 dB; highest D2 true peak: -2.82 dBFS
 * No sampled clipping; other tracks and the final effects mix can peak higher
 * Match by attenuation, preserving dynamics; effects calibration is separate
 * from the sliders and launcher previews retain their own levels
 * Reproduction and measurement limits: tests/music_spectral/README.md */

/* Units and combination rules
 * *_DB values are decibels of gain: 0 dB leaves the level unchanged, negative
 * values attenuate, positive values boost; consecutive dB gains ADD
 * *_SCALE values multiply audio sample amplitudes directly: 1 = unchanged,
 * 0.5 = half amplitude (about -6.02 dB), 0 = silence; consecutive scales MULTIPLY
 * Convert dB to amplitude scale with pow(10, dB / 20); convert a positive scale
 * back with 20 * log10(scale); the 20 is for amplitude, not audio power
 * These are amplitude ratios, not percentages of perceived loudness
 * Gameplay sliders are integers 0..8, converted to a linear scale of value / 8
 * Launcher previews use their own settings below, not the gameplay slider
 * Synth gain acts during rendering; output scales act afterward and cannot
 * repair clipping that has already happened inside the synth */

/* Gameplay MIDI: synth gain in dB, relative to the synth's 0 dB reference
 * Add the applicable D2 boost below, then render; multiply rendered samples
 * by music slider / 8 afterward; -7 dB alone is about 0.447 times amplitude */
#define MUSIC_GAMEPLAY_GAIN_DB -7.0f

/* Extra dB added to gameplay synth gain for D2 bundled balanced SF2 only
 * Current total is -7 + 0 = -7 dB; 0 means no extra boost, not silence
 * Keeping this at zero retains the measured headroom */
#define MUSIC_D2_SF2_BOOST_DB 0.0f

/* Linear sample multiplier implementing -14 dB: pow(10, -14.0 / 20.0)
 * This keeps about 19.95% of the original CD amplitude, matched to D2 MIDI
 * Gameplay CD sample = original sample * this scale * (music slider / 8)
 * CD preview uses this scale too, with its separate preview multiplier below */
#define MUSIC_CD_VOLUME_SCALE 0.1995262315f

/* Gameplay MP3/OGG/FLAC/WAV sample multiplier: -14 dB, about 19.95% amplitude
 * Output = decoded sample * this scale * (music slider / 8)
 * Starts at the measured CD attenuation for CD-like recordings; independently
 * tunable because file tracks vary in loudness; no per-track normalization
 * This replaces the MIDI/CD gain path for files, rather than stacking with it */
#define MUSIC_FILE_VOLUME_SCALE 0.1995262315f

/* Fixed Android SDL_mixer effects calibration, applied once to channel gain
 * 0.25 is -12.04 dB, in addition to sound level/distance and panning
 * At slider 8 this preserves the corrected mixer's former slider-2 level
 * Both sliders default to maximum; device volume controls overall loudness */
#define AUDIO_EFFECTS_VOLUME_SCALE 0.25f

/* Initial effects slider position, integer 0..8; saved choices override */
#define AUDIO_DEFAULT_EFFECTS_VOLUME 8

/* Launcher MIDI synth gain in dB; replaces the gameplay gain/boost for previews
 * -10 dB is about 0.316 times the synth's 0 dB amplitude, before output scaling */
#define MUSIC_MIDI_PREVIEW_GAIN_DB -10.0f

/* Linear multiplier on rendered MIDI preview samples: 0.7 is about -3.10 dB
 * Together with -10 dB synth gain: 0.316 * 0.7 = 0.221, about -13.10 dB
 * This retains the earlier preview level; CD attenuation does not apply here */
#define MUSIC_MIDI_PREVIEW_VOLUME_SCALE 0.7f

/* Extra linear multiplier on CD previews: 0.8 is about -1.94 dB
 * Combined with CD attenuation: 0.199526 * 0.8 = 0.159621, about -15.94 dB
 * MIDI and CD have different source levels; equal numeric gains would not
 * imply equal perceived playback loudness */
#define MUSIC_CD_PREVIEW_VOLUME_SCALE 0.8f

#endif
