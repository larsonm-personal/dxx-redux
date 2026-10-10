# Preserve WAV mission filename display fallback, 2026-10-09

Diagnosis and proposed remediation only. No product/test mutation or runtime execution in this tranche. Canonical owner GQF0271/GQR0257 admitted through GQ2-CHUNK-0089; implementation remains TODO

## Concrete behavior

Both song parsers admit SONG_EXT_WAV. Shared mix_play_file dispatches .wav to music_decode_wav and permits successful PCM playback. D2 normal built-in playback and shared next/previous/specific-track controls call track_overlay_notify_mission_music after success. Without an explicit mission sidecar name, this helper recognizes .ogg/.mp3/.flac as audio filenames but routes .wav to MIDI Track N. A playing WAV therefore displays a MIDI label even though the public header promises audio filename fallback. Shared controls give the same affected path to D1; do not assume its normal native song hooks are identical to D2

The audio_tag_metadata parser intentionally supports MP3/OGG/FLAC only; adding WAV tag parsing is not required to fix fallback. mission_embedded_names_load also submits WAV to the MIDI resolver, which cannot supply ordinary WAV summary, but no unrelated format/parser expansion is proposed

## Proposed change and acceptance

- Include supported WAV in filename fallback, preserving sidecar priority, placeholder handling, bounded basename/extension stripping and case-insensitive suffix behavior. Keep the change in shared track_names.c; no paired inherited song-hook rewrite is needed
- Leave actual MIDI/HMP/HMQ labels and audio tag supported-format contract intact. Consider skipping unsupported PCM metadata scans only if it simplifies existing ownership without introducing a new parser/classification abstraction
- At implementation time, verify sidecar named WAV and unnamed lowercase/uppercase WAV through the actual helper and a maintained overlay consumer; shared explicit controls in both games and D2 normal built-in playback should publish the filename label. Existing MIDI labels, OGG/MP3/FLAC embedded/sidecar precedence, placeholder and bounded basename behavior must remain valid
- Verify relevant Android build/overlay integration and paired desktop compile preservation. Source reasoning here proves dispatch mismatch, not a fresh successful playback/device observation

Existing BR0091/GQR0166 completed schema fixes and GQR0093 member identity/GQR0094 freshness address distinct roots. New fallback is a presentation defect, not sidecar schema reopening. Original1996 assigned paths absent; zero inherited saving

Provisional impact rating:39 (H/M/B/C/R=12/0/7/10/10); canonical owner:GQF0271/GQR0257; small shared presentation correction with exact bounded owner, no measured performance claim
