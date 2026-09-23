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
    result = patch(Path(sys.argv[1]).read_text(), '--hmi-pitch' in sys.argv[3:])
    if '--hmi-driver' in sys.argv[3:]:
        from patch_hmi import patch as patch_hmi, patch_header
        result = patch_hmi(result)
        header_path = Path(sys.argv[sys.argv.index('--hmi-header') + 1])
        header_path.write_text(patch_header(header_path.read_text()), encoding='utf8')
    if '--runtime' in sys.argv[3:] and '--hmi-driver' not in sys.argv[3:]:
        # Live events must obey order even without a rendered sample between them
        old = 'while ((voice = findVoice(channel, note)) != nullptr)'
        assert result.count(old) == 1
        result = result.replace(old, 'while ((voice = findVoice(channel, note)) != nullptr ||\n'
                                '           (voice = findVoice(channel, note, true)) != nullptr)')
    if '--runtime' in sys.argv[3:]:
        header_path = Path(sys.argv[sys.argv.index('--runtime-header') + 1])
        header = header_path.read_text()
        marker = '\tstd::vector<ymfm::ymf262*> m_opl3;'
        assert header.count(marker) == 1
        header_path.write_text(header.replace(marker, marker + '\n\tstd::vector<std::vector<uint8_t>> m_resetState;'), encoding='utf8')
        # Reset sample history as well as chip state for reproducible seek/restart
        old = 'void OPLPlayer::reset()\n{'
        assert result.count(old) == 1
        result = result.replace(old, old + '''
    m_samplePos = 0;
    if (m_resetState.empty()) {
        m_resetState.resize(m_opl3.size());
        for (size_t i = 0; i < m_opl3.size(); ++i) {
            ymfm::ymfm_saved_state state(m_resetState[i], true);
            m_opl3[i]->save_restore(state);
        }
    } else {
        for (size_t i = 0; i < m_opl3.size(); ++i) {
            ymfm::ymfm_saved_state state(m_resetState[i], false);
            m_opl3[i]->save_restore(state);
        }
    }
    for (auto& fifo : m_sampleFIFO) fifo = {};
    for (int i = 0; i < 2; ++i) {
        m_lastOut[i] = m_hpLastIn[i] = m_hpLastOut[i] = 0;
        m_hpLastInF[i] = m_hpLastOutF[i] = 0;
    }
''')
        # CC120 must silence release tails as well as keyed notes
        old = '\tswitch (control)\n\t{'
        assert result.count(old) == 1
        result = result.replace(old, old + '''
    case 120:
        for (auto& voice : m_voices)
            if (voice.channel == &ch) silenceVoice(voice);
        break;
''')
    Path(sys.argv[2]).write_text(result, encoding='utf8')
