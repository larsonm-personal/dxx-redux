"""Keep persistent host metadata workers on the shared joined-audio lifecycle."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MetadataWorkerAudioLifecycle(unittest.TestCase):
    def test_host_worker_uses_shared_cleanup_and_android_keeps_own_lifecycle(self):
        source = (ROOT / 'android/app/src/main/cpp/jni_level_metadata.cpp').read_text()
        self.assertIn('#ifndef __ANDROID__\n#include "headless/headless_audio.h"\n#endif', source)
        init = source.split('static int init_levelmeta_audio(void)', 1)[1].split('static int init_levelmeta_screen', 1)[0]
        self.assertIn('#ifndef __ANDROID__\n\treturn init_headless_audio();\n#else', init)
        android = init.split('#else', 1)[1]
        self.assertIn('digi_init()', android)
        self.assertNotIn('atexit', android)
        helper = (ROOT / 'android/app/src/main/cpp/headless/headless_audio.h').read_text()
        self.assertIn('atexit(close_headless_audio)', helper)
        self.assertLess(helper.index('digi_close();'), helper.index('SDL_Quit();'))

    def test_hosted_runtime_repeats_and_retires_real_workers(self):
        runtime = (ROOT / 'android/tests/test_native_metadata_worker.ps1').read_text()
        for fragment in ("Request 'repeat'", "Request 'switch-d2'", "Request 'switch-back-d1'", 'Stop-MetadataWorkerProcess -Worker $worker'):
            self.assertIn(fragment, runtime)
        runner = (ROOT / 'android/tests/run_engine_sanitizers.ps1').read_text()
        self.assertIn('-CheckProfileSwitch', runner)


if __name__ == '__main__':
    unittest.main()
