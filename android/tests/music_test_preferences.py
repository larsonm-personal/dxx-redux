"""Back up and restore game preferences around installed-app music tests."""
from pathlib import Path
import xml.etree.ElementTree as ET

PACKAGE = 'com.dxxredux.app'
TARGET = 'shared_prefs/dxx_prefs.xml'


def save_preferences(call, output):
    result = call('shell', 'run-as', PACKAGE, 'cat', TARGET, check=False)
    path = Path(output) / 'original-game-preferences.xml'
    if result.returncode == 0:
        path.write_text(result.stdout, encoding='utf8')
        return path
    return None


def publish_preferences(call, path):
    remote = '/data/local/tmp/music-game-preferences.xml'
    call('push', str(path), remote)
    call('shell', 'run-as', PACKAGE, 'mkdir', '-p', 'shared_prefs')
    call('shell', 'run-as', PACKAGE, 'cp', remote, TARGET)
    call('shell', 'rm', '-f', remote)


def restore_preferences(call, path):
    if path:
        publish_preferences(call, path)
    else:
        call('shell', 'run-as', PACKAGE, 'rm', '-f', TARGET)


def clear_midi_preferences(call, original, output):
    root = ET.parse(original).getroot() if original else ET.Element('map')
    for entry in list(root):
        if entry.get('name') in ('midi_renderer', 'midi_soundfont', 'midi_reverb', 'midi_chorus'):
            root.remove(entry)
    path = Path(output) / 'fresh-midi-preferences.xml'
    ET.ElementTree(root).write(path, encoding='utf-8', xml_declaration=True)
    publish_preferences(call, path)
