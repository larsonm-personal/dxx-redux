"""Create an experimental BSD ymfmidi source variant; leave the checkout intact.

The pinned version only clears justChanged/ages voices inside its file sequencer.
External MIDI users need that bookkeeping after rendering a nonempty batch too.
This experiment ages by render batch, not elapsed samples, matching the upstream
event-batch convention approximately. It is not an HMI voice-allocation fix.
"""
from pathlib import Path
import sys


def patch(source, hmi_pitch=False):
    source = ('#include <cstdint>\nextern void fm_trace_write(int, uint16_t, uint8_t);\n' + source)
    source = source.replace('void OPLPlayer::write(int chip, uint16_t addr, uint8_t data)\n{',
                            'void OPLPlayer::write(int chip, uint16_t addr, uint8_t data)\n{\n\tfm_trace_write(chip, addr, data);')
    marker = '\n}\n\n// ----------------------------------------------------------------------------\nvoid OPLPlayer::'
    for signature, following in [('generate(float *data, unsigned numSamples)', 'generate(int16_t'),
                                 ('generate(int16_t *data, unsigned numSamples)', 'updateMIDI()')]:
        start = source.index('void OPLPlayer::' + signature)
        end = source.index(marker + following, start)
        source = (source[:end] + '\n\t// Experiment: advance bookkeeping for external MIDI batches\n'
                  '\tif (numSamples && !m_sequence)\n\t{\n'
                  '\t\tfor (auto& voice : m_voices)\n\t\t{\n'
                  '\t\t\tvoice.justChanged = false;\n'
                  '\t\t\tif (voice.duration < UINT_MAX) ++voice.duration;\n'
                  '\t\t}\n\t}\n' + source[end:])
    if hmi_pitch:
        marker = '\tint octave = note / 12;'
        assert source.count(marker) == 1
        source = source.replace(marker, '''\t// Neutral-pitch table measured from original Level 7 OPL writes
\t// WOPL loader has already subtracted 12 from the MIDI note
\tstatic const uint16_t hmiFreq[12] = {343, 363, 385, 408, 432, 458, 485, 514, 544, 577, 611, 647};
\tif (note >= 0 && note < 96 && voice.channel->pitch == 1.0 && voice.patchVoice->finetune == 1.0)
\t{
\t\tvoice.freq = hmiFreq[note % 12] | ((note / 12) << 10);
\t\twrite(voice.chip, REG_VOICE_FREQL + voice.num, voice.freq & 0xff);
\t\twrite(voice.chip, REG_VOICE_FREQH + voice.num, (voice.freq >> 8) | (voice.on ? (1 << 5) : 0));
\t\treturn;
\t}
''' + marker)
    return source


if __name__ == '__main__':
    Path(sys.argv[2]).write_text(patch(Path(sys.argv[1]).read_text(), '--hmi-pitch' in sys.argv[3:]), encoding='utf8')
