"""Experimental original-driver rules measured from local DOS OPL captures.

Only applied to the pinned BSD player copy. Rules are measured from original
DOS register captures, with controlled probes and independent song comparisons.
"""
from pathlib import Path


def replace_function(source, signature, body):
    start = source.index(signature + '\n{')
    end = source.index('\n// ----------------------------------------------------------------------------', start)
    return source[:start] + signature + '\n{\n' + body + '\n}\n' + source[end:]


def patch(source):
    source = '#include <cstdint>\n' + Path(__file__).with_name('hmi_pitch_table.inc').read_text() + '\n' + source
    source = source.replace('m_voices.resize(numChips * 18);', 'm_voices.resize(numChips * 9);')
    source = source.replace('note < 96 && voice.channel->pitch == 1.0 &&', 'note < 96 &&')
    marker = '\t\tvoice.freq = hmiFreq[note % 12] | ((note / 12) << 10);'
    assert source.count(marker) == 1
    source = source.replace(marker, '''
        const unsigned key = note % 12;
        unsigned freq = hmiFreq[key];
        unsigned block = note / 12;
        if (voice.channel->hmiPitchSeen) {
            // Measured quantized wheel curve, including HMI's boundary rounding
            const unsigned wheel = unsigned((voice.channel->basePitch + 1.0) * 8192.0) >> 7;
            const unsigned value = hmiBend[key][std::min(127u, wheel)];
            freq = value & 1023;
            block += value >> 10;
        }
        voice.freq = freq | (std::min(7u, block) << 10);''')
    source = replace_function(source, 'void OPLPlayer::write(int chip, uint16_t addr, uint8_t data)', r'''
    auto raw = [&](uint16_t reg) {
        fm_trace_write(chip, reg, data);
        if (reg < 0x100) m_opl3[chip]->write_address(uint8_t(reg));
        else m_opl3[chip]->write_address_hi(uint8_t(reg));
        m_opl3[chip]->write_data(data);
    };
    raw(addr);
    // Each logical DOS voice drives the same patch/pitch in both OPL3 banks
    // Levels and routing are explicitly written per side below
    if (addr >= 0x20 && addr <= 0xf5 && !(addr >= 0x40 && addr <= 0x55) &&
        !(addr >= 0xc0 && addr <= 0xc8)) raw(addr | 0x100);
''')
    source = replace_function(source, 'void OPLPlayer::updateVolume(OPLVoice& voice)', r'''
    if (!voice.patch || !voice.channel) return;
    // Independently measured using all velocities, volumes and pan positions
    // Index 63 is unreachable with both full HMI gain stages at 127
    static const uint8_t attenuation[63] = {
        63,58,53,48,44,41,37,36,35,34,33,32,31,30,29,28,
        27,26,25,24,23,22,21,20,19,18,17,16,15,14,14,13,
        13,12,12,11,11,10,10,9,9,8,8,7,7,6,6,6,
        5,5,5,4,4,4,4,3,3,3,2,2,2,1,1
    };
    const auto& p = *voice.patchVoice;
    unsigned volume = (((voice.channel->volume * 127) >> 7) * 127) >> 7;
    volume = (volume * voice.velocity) >> 7;
    for (unsigned side = 0; side < 2; ++side) {
        if (voice.channel->hmiPanUpdateSide >= 0 && unsigned(voice.channel->hmiPanUpdateSide) != side) continue;
        unsigned pan = voice.channel->hmiPanSeen ? (side ? voice.channel->pan : 127 - voice.channel->pan) : 64;
        unsigned level = (volume * std::min(64u, pan)) >> 6;
        unsigned atten = attenuation[level >> 1];
        unsigned carrier = atten + ((63 - atten) * p.op_level[1]) / 63;
        unsigned bank = side << 8;
        // HMI leaves the modulator level alone even for additive patches
        write(voice.chip, REG_OP_LEVEL + voice.op + bank, p.op_level[0] | p.op_ksr[0]);
        write(voice.chip, REG_OP_LEVEL + voice.op + bank + 3, carrier | p.op_ksr[1]);
    }
''')
    source = replace_function(source, 'void OPLPlayer::updatePanning(OPLVoice& voice)', r'''
    if (!voice.patch || !voice.channel) return;
    write(voice.chip, REG_VOICE_CNT + voice.num, voice.patchVoice->conn | 0x20);
    write(voice.chip, REG_VOICE_CNT + voice.num + 0x100, voice.patchVoice->conn | 0x10);
    updateVolume(voice);
''')
    source = replace_function(source, 'OPLVoice* OPLPlayer::findVoice(uint8_t channel, const OPLPatch *patch, uint8_t note)', r'''
    (void)patch;
    (void)note;
    for (auto& voice : m_voices)
        if (!voice.on) return &voice;
    // DOS steals the first voice on the lowest-numbered eligible MIDI channel
    // A pitch-wheel message excludes its melodic channel, even at center
    OPLVoice* selected = nullptr;
    for (auto& voice : m_voices)
        if (!voice.channel->hmiPitchSeen &&
            (!selected || voice.channel->num < selected->channel->num))
            selected = &voice;
    // All held channels have received pitch messages: measured on all 16 inputs
    return selected ? selected : &m_voices[(channel & 15) % 9];
''')
    marker = 'void OPLPlayer::midiPitchControl(uint8_t channel, double pitch)\n{'
    assert source.count(marker) == 1
    source = source.replace(marker, marker + '''
    if ((channel & 15) == 9) return;
    m_channels[channel & 15].hmiPitchSeen = true;
''')
    marker = '\t\t\tupdateFrequency(*voice);'
    assert source.count(marker) == 1
    source = source.replace(marker, '''
            // DOS keys on at nominal pitch, then applies its saved wheel value
            auto& ch = m_channels[channel & 15];
            const bool wheelSeen = ch.hmiPitchSeen;
            ch.hmiPitchSeen = false;
            updateFrequency(*voice);
            ch.hmiPitchSeen = wheelSeen;
            if (wheelSeen) updateFrequency(*voice);''')
    marker = '\tcase 10:\n\t\tch.pan = value;'
    assert source.count(marker) == 1
    source = source.replace(marker, marker + '\n\t\tch.hmiPanSeen = true;')
    source = source.replace('if (m_stereo)\n\t\t\tupdateChannelVoices(channel, &OPLPlayer::updatePanning);',
                            '''ch.hmiPanUpdateSide = value < 64 ? 1 : 0;
        updateChannelVoices(channel, &OPLPlayer::updateVolume);
        ch.hmiPanUpdateSide = -1;''')
    marker = '\t\tvoice->on = voice->justChanged = true;'
    assert source.count(marker) == 1
    source = source.replace(marker, marker + '\n\t\tvoice->hmiHeld = true;')
    source = replace_function(source, 'void OPLPlayer::midiNoteOff(uint8_t channel, uint8_t note)', r'''
    for (auto& voice : m_voices) {
        if (!voice.on || voice.channel != &m_channels[channel & 15] || voice.note != (note & 127)) continue;
        voice.hmiHeld = false;
        if (voice.channel->hmiSustain) continue;
        voice.justChanged = true;
        voice.on = false;
        write(voice.chip, REG_VOICE_FREQH + voice.num, voice.freq >> 8);
    }
''')
    marker = '\tswitch (control)\n\t{'
    assert source.count(marker) == 1
    source = source.replace(marker, marker + '''
    case 64:
        ch.hmiSustain = value >= 64;
        if (!ch.hmiSustain)
            for (auto& voice : m_voices) {
                if (!voice.on || voice.channel != &ch || voice.hmiHeld) continue;
                voice.justChanged = true;
                voice.on = false;
                write(voice.chip, REG_VOICE_FREQH + voice.num, voice.freq >> 8);
            }
        break;
''')
    source = source.replace('if (findVoice(channel, note, true))\n\t\treturn;', '')
    source = source.replace('silenceVoice(voice);\n\t\trunSamples(voice.chip, 48);',
                            'write(voice.chip, REG_VOICE_FREQH + voice.num, voice.freq >> 8);')
    marker = '\t\tupdatePatch(*voice, newPatch, i);'
    assert source.count(marker) == 1
    source = source.replace(marker, '\t\twrite(voice->chip, REG_VOICE_FREQH + voice->num, voice->freq >> 8);\n' + marker)
    return source


def patch_header(header):
    marker = '\tbool percussion = false;'
    assert header.count(marker) == 1
    header = header.replace(marker, marker + '\n\tbool hmiPitchSeen = false;\n\tbool hmiPanSeen = false;'
                            '\n\tbool hmiSustain = false;\n\tint hmiPanUpdateSide = -1;')
    marker = '\tbool on = false;'
    assert header.count(marker) == 1
    return header.replace(marker, marker + '\n\tbool hmiHeld = false;')
