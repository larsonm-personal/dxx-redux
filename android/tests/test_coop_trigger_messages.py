"""Exercise production trigger notification functions with a simulated peer transport.

Run with python from a C++ developer shell (MSVC cl or c++ on PATH).
"""

from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


class CoopTriggerMessagesTest(unittest.TestCase):
    def test_notification_round_trip(self) -> None:
        compiler = shutil.which("cl") or shutil.which("c++")
        self.assertIsNotNone(compiler, "Run in a C++ developer shell")
        multi = (ROOT / "d2/main/multi.c").read_text()
        switch = (ROOT / "d2/main/switch.c").read_text()
        header = (ROOT / "d2/main/multi.h").read_text()
        size = re.search(r"#define MULTI_TRIGGER_MESSAGE_LEN (\d+)", header)[1]
        functions = "\n".join([
            function(multi, "void multi_send_trigger_message("),
            function(multi, "static void multi_do_trigger_message("),
            function(switch, "static void print_trigger_message("),
        ])
        # Use the production message formats, including their plural suffixes
        messages = re.findall(r'print_trigger_message \(pnum,trigger_num,shot,"([^"\n]+)"\)', switch)
        self.assertEqual(len(messages), 12)
        source = PREAMBLE.replace("MESSAGE_SIZE", size) + functions + MAIN.replace(
            "MESSAGE_FORMATS", ",".join('"' + message + '"' for message in messages)
        )
        (ROOT / "temp").mkdir(exist_ok=True)
        with tempfile.TemporaryDirectory(prefix="coop-trigger-", dir=ROOT / "temp") as directory:
            folder = Path(directory)
            (folder / "test.cpp").write_text(source)
            if Path(compiler).stem.lower() == "cl":
                command = [compiler, "/nologo", "/EHsc", "/W4", "/WX", "test.cpp", "/Fe:test.exe"]
            else:
                command = [compiler, "-Wall", "-Wextra", "-Werror", "test.cpp", "-o", "test.exe"]
            subprocess.run(command, cwd=folder, check=True)
            subprocess.run([str(folder / "test.exe")], cwd=folder, check=True)


PREAMBLE = r'''
#include <cassert>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#define NETWORK 1
#define MULTI_TRIGGER_MESSAGE_LEN MESSAGE_SIZE
#define GM_MULTI_COOP 1
#define HM_DEFAULT 0
#define CONNECT_PLAYING 1
#define MAX_PLAYERS 8
#define TF_NO_MESSAGE 1
#define MULTI_COOP_TRIGGER_MESSAGE 99
typedef unsigned char ubyte;
int Game_mode = GM_MULTI_COOP, Player_num = 0, N_players = 2;
struct { const char *callsign; int connected; } Players[MAX_PLAYERS] = {{"Ace%",1},{"Wing",1}};
struct { int flags, num_links; } Triggers[1] = {{0,1}};
bool observer = false;
std::vector<std::string> hud;
std::vector<ubyte> packet;
int sends = 0;
int is_observer() { return observer; }
int multi_who_is_master() { return 0; }
void HUD_init_message(int, const char *format, ...) {
    char text[256]; va_list args; va_start(args, format);
    vsnprintf(text, sizeof(text), format, args); va_end(args); hud.emplace_back(text);
}
void HUD_init_message_literal(int type, const char *text) { HUD_init_message(type, "%s", text); }
void multi_send_data(const ubyte *buf, int size, int priority) {
    assert(priority == 2); assert(size == 2 + MULTI_TRIGGER_MESSAGE_LEN);
    packet.assign(buf, buf + size); ++sends;
}
'''

MAIN = r'''
int main() {
    const char *formats[] = {MESSAGE_FORMATS};
    for (const char *format : formats) {
        for (int links : {1, 2}) {
            Triggers[0].num_links = links; Player_num = 0; hud.clear(); packet.clear();
            print_trigger_message(0, 0, 1, format);
            assert(hud.size() == 1 && !packet.empty());
            std::string local = hud.back();
            char expected[64]; snprintf(expected, sizeof(expected), format, links > 1 ? "s" : "");
            assert(local == std::string("Ace%: ") + expected);
            Player_num = 1;
            multi_do_trigger_message(packet.data(), 0);
            assert(hud.size() == 2 && hud.back() == local);
            print_trigger_message(0, 0, 1, format); // Replicated execution stays silent
            assert(hud.size() == 2);
        }
    }
    Player_num = 0; hud.clear(); int previous_sends = sends;
    print_trigger_message(0, 0, 0, "Wall%s opened!");
    Triggers[0].flags = TF_NO_MESSAGE;
    print_trigger_message(0, 0, 1, "Wall%s opened!");
    assert(hud.empty() && sends == previous_sends);
    Triggers[0].flags = 0; Game_mode = 0;
    print_trigger_message(0, 0, 1, "Wall%s opened!");
    assert(hud.back() == "Walls opened!" && sends == previous_sends);
    hud.clear(); multi_do_trigger_message(packet.data(), 0); assert(hud.empty());
    Game_mode = GM_MULTI_COOP;
    multi_do_trigger_message(packet.data(), 0); assert(hud.empty()); // Self echo
    Player_num = 1;
    multi_do_trigger_message(packet.data(), 1); assert(hud.empty()); // Wrong sender
    Players[0].connected = 0;
    multi_do_trigger_message(packet.data(), 0); assert(hud.empty());
    Players[0].connected = CONNECT_PLAYING;
    packet[1] = 255;
    multi_do_trigger_message(packet.data(), -1); assert(hud.empty());
    packet[1] = 0;
    memset(packet.data() + 2, 'x', MULTI_TRIGGER_MESSAGE_LEN);
    multi_do_trigger_message(packet.data(), 0);
    assert(hud.back() == std::string("Ace%: ") + std::string(MULTI_TRIGGER_MESSAGE_LEN - 1, 'x'));
    // Reliable messages from a third player are forwarded by the host
    N_players = 3; Player_num = 2; packet[1] = 1;
    memcpy(packet.data() + 2, "Wall opened!", 13);
    multi_do_trigger_message(packet.data(), 0);
    assert(hud.back() == "Wing: Wall opened!");
    observer = true; multi_send_trigger_message("test"); assert(sends == previous_sends);
    puts("Co-op trigger notification round-trip checks passed");
}
'''

if __name__ == "__main__":
    unittest.main()
