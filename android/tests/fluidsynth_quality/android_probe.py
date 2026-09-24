"""Run the standalone FluidSynth contracts and offline benchmark on Android.

Does not launch or modify the game. Uses its own directory in /data/local/tmp.
"""
import argparse
import json
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--adb', default='adb')
    parser.add_argument('--serial', required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--ndk', type=Path, required=True)
    parser.add_argument('--font', type=Path, required=True)
    parser.add_argument('--midi', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    remote = '/data/local/tmp/dxx-fluidsynth-quality'

    def adb(*parts, timeout=60):
        result = subprocess.run([args.adb, '-s', args.serial, *map(str, parts)],
                                capture_output=True, text=True, timeout=timeout)
        if result.returncode:
            raise RuntimeError(f'ADB failed: {result.stdout}\n{result.stderr}')
        return result.stdout

    abi = adb('shell', 'getprop', 'ro.product.cpu.abi').strip()
    triples = {'x86_64': 'x86_64-linux-android', 'arm64-v8a': 'aarch64-linux-android',
               'armeabi-v7a': 'arm-linux-androideabi'}
    assert abi in triples, abi
    runtimes = list(args.ndk.glob(f'toolchains/llvm/prebuilt/*/sysroot/usr/lib/{triples[abi]}/libc++_shared.so'))
    assert len(runtimes) == 1
    adb('shell', 'mkdir', '-p', remote)
    for local, name in [(args.build / 'bin/fluid_render', 'fluid_render'),
                        (args.build / 'bin/libfluidsynth.so', 'libfluidsynth.so'),
                        (runtimes[0], 'libc++_shared.so'), (args.font, 'font.sf2'), (args.midi, 'song.mid')]:
        assert local.is_file(), local
        adb('push', local, f'{remote}/{name}')
    adb('shell', 'chmod', '700', f'{remote}/fluid_render')
    command = f'cd {remote} && LD_LIBRARY_PATH={remote} ./fluid_render'
    checks = adb('shell', command + ' --test font.sf2', timeout=120)
    (args.output / 'contracts.log').write_text(checks, encoding='utf8')
    runs = {}
    for label, reverb, chorus, send in [('dry', 0, 0, 0), ('both', 1, 1, 24)]:
        result = adb('shell', f'{command} font.sf2 song.mid {label}.wav 15 {reverb} {chorus} {send}', timeout=120)
        (args.output / f'{label}.log').write_text(result, encoding='utf8')
        runs[label] = json.loads(result.strip().splitlines()[-1])
        assert runs[label]['clipped_samples'] == 0
        adb('pull', f'{remote}/{label}.wav', args.output / f'{label}.wav')
    report = {'serial': args.serial, 'abi': abi, 'contracts': checks.strip(), 'runs': runs,
              'scope': 'Offline native renderer on this Android device; not an in-game underrun or phone benchmark'}
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
