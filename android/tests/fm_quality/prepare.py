"""Generate isolated variants of the shipping renderer; never edit production files."""
from pathlib import Path
import shutil
import sys


def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError(f'Production source changed; review experiment patch: {old[:90]}')
    return text.replace(old, new, 1)


def prepare(repo, build):
    shared = repo / 'android/app/src/main/cpp/shared'
    original = (repo / 'android/app/src/main/cpp/extract/test_music_synth.c').read_text()
    synth = (shared / 'music_synth.cpp').read_text()
    # Preserve the original audition baseline after promoting clean FM to production
    synth = replace_once(synth, 'fm->setSampleRate(music_fm_resampler::native_rate);', 'fm->setSampleRate(s->rate);')
    synth = replace_once(synth, 'fm->setGain(1);', 'fm->setGain(std::pow(10.0, s->gain / 20.0));')
    synth = replace_once(synth, 's->fm_resampler.configure(rate);', '''s->fm->setSampleRate(rate);
        s->fm->setGain(std::pow(10.0, gain / 20.0));''')
    start = synth.index('void music_synth_render_short(')
    end = synth.index('void music_synth_channel_note_on(', start)
    synth = synth[:start] + '''void music_synth_render_short(music_synth *s, short *out, int frames, int mixing)
{
    if (s->fm) s->fm->generate(out, frames);
    else tsf_render_short(s->sf, out, frames, mixing);
}
''' + synth[end:]
    (build / 'legacy_synth.cpp').write_text(synth, encoding='utf8')
    synth += '''
// Experiment-only float outlet; the shipping interface remains unchanged
extern "C" void quality_render_float(music_synth *s, float *out, int frames)
{
    if (!s->fm) std::abort();
    s->fm->generate(out, frames);
}
'''
    (build / 'float_synth.cpp').write_text(synth, encoding='utf8')
    for variant, rate, gain in [('float', 48000, -10), ('native', 49715, 0)]:
        # 14318181 / (OPL3 prescale 8 * 36 operators), as used by pinned ymfmidi
        text = original.replace('48000', str(rate))
        text = replace_once(text, 'int main(int argc, char **argv)\n{', '''int main(int argc, char **argv)
{
    CHECK(argc >= 7 && argc <= 9 && !strcmp(argv[1], "--render"));''')
        text = replace_once(text, 'static FILE *trace_file;', '''static FILE *trace_file;
extern void quality_render_float(music_synth *s, float *out, int frames);
static float *quality_pcm, *quality_cursor;''')
        text = replace_once(text, 'music_synth_render_short(s, out, frames, 0);', '''
    quality_render_float(s, quality_cursor, frames);
    quality_cursor += (size_t) frames * 2;
    memset(out, 0, (size_t) frames * 2 * sizeof(short));''')
        # Restrict modifications to the offline export, keeping original conversion/dispatch
        start = text.index('static int render_comparison(')
        end = text.index('\nint main(', start)
        part = text[start:end]
        part = replace_once(part, f'{rate}, -10);', f'{rate}, {gain});')
        part = replace_once(part, 'CHECK(pcm);', '''CHECK(pcm);
    quality_pcm = malloc((size_t) frames * 2 * sizeof(float));
    CHECK(quality_pcm);
    quality_cursor = quality_pcm;''')
        part = part.replace('frames * 4', 'frames * 8')
        part = replace_once(part, 'write_le(file, 1, 2);', 'write_le(file, 3, 2);')
        part = replace_once(part, f'write_le(file, {rate} * 4, 4);', f'write_le(file, {rate} * 8, 4);')
        part = replace_once(part, 'write_le(file, 4, 2);', 'write_le(file, 8, 2);')
        part = replace_once(part, 'write_le(file, 16, 2);', 'write_le(file, 32, 2);')
        part = replace_once(part,
            'for (int i = 0; i < frames * 2; ++i) write_le(file, (unsigned short) pcm[i], 2);',
            '''CHECK(sizeof(float) == 4);
    for (int i = 0; i < frames * 2; ++i) {
        uint32_t bits;
        memcpy(&bits, quality_pcm + i, sizeof(bits));
        write_le(file, bits, 4);
    }
    free(quality_pcm);''')
        text = text[:start] + part + text[end:]
        (build / f'{variant}_driver.c').write_text(text, encoding='utf8')
    target = build / 'float-player'
    target.mkdir(exist_ok=True)
    for source in (build / 'music-ymfmidi').glob('*'):
        if source.suffix in ('.cpp', '.h'):
            shutil.copyfile(source, target / source.name)
    header = (target / 'player.h').read_text()
    header = replace_once(header, 'ymfm::ymf262::output_data m_output;',
                          'struct { double data[4] = {}; } m_output;')
    header = replace_once(header, 'int32_t m_lastOut[2] = {0};', 'double m_lastOut[2] = {0};')
    (target / 'player.h').write_text(header, encoding='utf8')
    # The unused integer outlet still has explicit conversion for a warning-free build
    cpp = (target / 'player.cpp').read_text()
    cpp = cpp.replace('m_hpLastIn[i] = m_output.data[i];', 'm_hpLastIn[i] = static_cast<int32_t>(m_output.data[i]);')
    cpp = cpp.replace('ymfm::clamp(m_output.data[0],', 'ymfm::clamp(static_cast<int32_t>(m_output.data[0]),')
    cpp = cpp.replace('ymfm::clamp(m_output.data[1],', 'ymfm::clamp(static_cast<int32_t>(m_output.data[1]),')
    (target / 'player.cpp').write_text(cpp, encoding='utf8')


if __name__ == '__main__':
    prepare(Path(sys.argv[1]), Path(sys.argv[2]))
