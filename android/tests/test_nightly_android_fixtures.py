import hashlib
import importlib.util
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "android/helpers" / (name + ".py"))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


corpus = module("setup_demo_import_corpus")
audio = module("generate_preview_test_audio")
wifi = module("prepare_lan_wifi")


class SdkWorkflowContractTests(unittest.TestCase):
    def test_emulator_runtime_is_provisioned_and_checked_before_build(self):
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        runtime = source.split("- name: Provision emulator host runtime", 1)[1].split(
            "- name: Provision emulator image", 1
        )[0]
        self.assertIn("if: inputs.lane != 'jvm'", runtime)
        self.assertIn("timeout-minutes: 8", runtime)
        self.assertIn("apt-get install -y --no-install-recommends libpulse0", runtime)
        check = source.split("- name: Check emulator runtime before APK build", 1)[1].split(
            "- name: Require shared Wi-Fi", 1
        )[0]
        self.assertIn("set -euo pipefail", check)
        self.assertIn("emulator -version", check)
        self.assertLess(source.index("- name: Check emulator runtime"), source.index("- name: Build emulator APKs"))

    def test_managed_sdk_uses_plain_private_directory(self):
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        setup = source.split("- name: Prepare dependency locations", 1)[1].split("- name: Require accelerated", 1)[0]
        self.assertIn('sdk_root="$dep_base/android-sdk"', setup)
        self.assertIn('mkdir -p "$sdk_root"', setup)
        self.assertNotIn("ln -s", setup)
        self.assertNotIn("${ANDROID_HOME:-", setup)
        self.assertIn('echo "ANDROID_HOME=$sdk_root"', setup)
        self.assertIn('echo "DXX_NIGHTLY_SDK_CACHE=$sdk_root"', setup)
        inventory = (ROOT / "android/helpers/sdk_package_inventory.ps1").read_text()
        self.assertIn("Assert-DxxPlainSdkPath $sdkRoot", inventory)
        self.assertIn("Cannot assess linked SDK/reference path:", inventory)


@unittest.skipUnless(sys.platform == "linux", "Linux-only KVM ACL refresh helper")
class KvmRefreshTests(unittest.TestCase):
    BASE = "# file: /dev/kvm\n# owner: 0\n# group: 993\nuser::rw-\ngroup::rw-\nother::---\n\n"

    def validate(self, current, original=None):
        with tempfile.TemporaryDirectory() as d:
            original_path, current_path = Path(d, "before"), Path(d, "current")
            original_path.write_text(self.BASE if original is None else original)
            current_path.write_text(current)
            before = original_path.read_bytes()
            helper = ROOT / "android/helpers/refresh_nightly_kvm_access.sh"
            result = subprocess.run(
                [
                    "bash",
                    "-c",
                    'source "$1"; validate_nightly_kvm_acl "$2" "$3" 1001',
                    "check",
                    str(helper),
                    str(original_path),
                    str(current_path),
                ],
                capture_output=True,
            )
            self.assertEqual(original_path.read_bytes(), before)
            return result.returncode

    def test_original_and_disappeared_grant_can_be_refreshed(self):
        self.assertEqual(self.validate(self.BASE), 0)
        self.assertEqual(self.validate(self.BASE.replace("other::", "mask::rw-\nother::")), 0)

    def test_existing_same_uid_grant_is_accepted(self):
        acl = self.BASE.replace("group::", "user:1001:rw-\ngroup::").replace("other::", "mask::rw-\nother::")
        self.assertEqual(self.validate(acl), 0)

    def test_unexpected_security_changes_are_rejected(self):
        for acl in (
            self.BASE.replace("group::", "user:1002:rw-\ngroup::"),
            self.BASE.replace("other::---", "other::rw-"),
            self.BASE.replace("# owner: 0", "# owner: 1001"),
            self.BASE.replace("other::", "mask::rwx\nother::"),
        ):
            with self.subTest(acl=acl):
                self.assertNotEqual(self.validate(acl), 0)

    def test_extended_original_snapshot_is_rejected_without_overwriting(self):
        extended = self.BASE.replace("group::", "user:1002:rw-\ngroup::")
        self.assertNotEqual(self.validate(self.BASE, extended), 0)

    def test_refresh_never_replaces_original_snapshot_or_authorized_uid(self):
        helper = (ROOT / "android/helpers/refresh_nightly_kvm_access.sh").read_text()
        self.assertIn('[[ "$uid" == "$approved" ]]', helper)
        self.assertIn('sudo setfacl -m "u:$uid:rw" /dev/kvm', helper)
        self.assertNotIn('> "$original"', helper)
        self.assertNotIn('> "$RUNNER_TEMP/dxx-kvm-before.acl"', helper)
        self.assertNotIn('> "$RUNNER_TEMP/dxx-kvm-runner.uid"', helper)
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        self.assertEqual(source.count('> "$RUNNER_TEMP/dxx-kvm-before.acl"'), 1)
        self.assertEqual(source.count('> "$RUNNER_TEMP/dxx-kvm-runner.uid"'), 1)
        self.assertEqual(source.count("refresh_nightly_kvm_access.sh"), 2)


class KvmWorkflowContractTests(unittest.TestCase):
    def test_grant_is_only_existing_runner_user_on_real_kvm(self):
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        grant = source.split("- name: Require accelerated emulator access", 1)[1].split("- name: Restore pinned", 1)[0]
        self.assertIn("[[ ! -c /dev/kvm ]]", grant)
        self.assertIn("[[ ! -r /dev/kvm || ! -w /dev/kvm ]]", grant)
        self.assertIn('sudo setfacl -m "u:$(id -u):rw" /dev/kvm', grant)
        self.assertLess(grant.index("getfacl -p -n /dev/kvm >"), grant.index("sudo setfacl -m"))
        self.assertIn("grep -Eq '^(mask::|user:[^:]+:|group:[^:]+:)'", grant)
        self.assertLess(grant.index("grep -Eq"), grant.index("touch"))
        self.assertLess(grant.index('touch "$RUNNER_TEMP/dxx-kvm-acl-changed"'), grant.index("sudo setfacl -m"))
        for forbidden in ("chmod", "0666", "usermod", "useradd", "groupadd"):
            self.assertNotIn(forbidden, grant)

    def test_each_launch_records_actual_access_without_expanding_permissions(self):
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        loop = source.split("foreach ($avd in $avds) {", 1)[1].split("if ($env:SWEEP_LANE -eq 'android-dual')", 1)[0]
        self.assertIn("& getfacl -p -n /dev/kvm", loop)
        self.assertIn('os.open("/dev/kvm", os.O_RDWR)', loop)
        self.assertIn("os.close(fd)", loop)
        self.assertLess(loop.index("os.open"), loop.index("emu_health.ps1"))
        self.assertNotIn("setfacl", loop)
        cleanup = source.split("- name: Collect emulator diagnostics", 1)[1].split(
            "- name: Restore original KVM access", 1
        )[0]
        self.assertLess(cleanup.index("kvm-before-cleanup.log"), cleanup.index("emu kill"))

    def test_cleanup_is_unconditional_and_restores_prior_acl(self):
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        cleanup = source.split("- name: Restore original KVM access", 1)[1].split("- name: Record lane lifecycle", 1)[0]
        self.assertIn("if: always() && inputs.lane != 'jvm'", cleanup)
        self.assertIn("timeout-minutes: 1", cleanup)
        self.assertIn('[[ -f "$RUNNER_TEMP/dxx-kvm-acl-changed" ]]', cleanup)
        self.assertIn('sudo setfacl --restore="$RUNNER_TEMP/dxx-kvm-before.acl"', cleanup)
        self.assertIn('cmp "$RUNNER_TEMP/dxx-kvm-before.acl" results/setup/kvm-restored.acl', cleanup)
        self.assertLess(
            source.index("- name: Collect emulator diagnostics"), source.index("- name: Restore original KVM access")
        )
        self.assertLess(
            source.index("- name: Restore original KVM access"), source.index("- name: Upload execution evidence")
        )


class FixtureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.data = b"public demo fixture witness"
        self.entry = {"archive": "public demo.zip", "sha256": hashlib.sha256(self.data).hexdigest()}
        self.manifest = {
            "path": "game_data/demo installers/public demo.zip",
            "sha256": self.entry["sha256"],
            "size": len(self.data),
        }
        mapping = patch.dict(corpus.PUBLIC_ASSETS, {"public demo.zip": "public demo.zip"})
        mapping.start()
        self.addCleanup(mapping.stop)
        self.write_manifests()
        self.output = self.root / "corpus"

    def write_manifests(self):
        for path, value in [
            ("android/tests/fixtures/demo_import_oracles.json", {"archives": [self.entry]}),
            ("game_data/test_data_manifest.json", {"files": [self.manifest]}),
        ]:
            target = self.root / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(json.dumps(value))

    def test_identity_and_encoded_fixed_release(self):
        (entry,) = corpus.packages(self.root)
        self.assertEqual(corpus.RELEASE + "public%20demo.zip", entry["url"])
        self.assertEqual(len(self.data), entry["bytes"])

    def test_unpublished_original_is_not_substituted(self):
        with patch.dict(corpus.PUBLIC_ASSETS, {}, clear=True):
            self.assertIsNone(corpus.packages(self.root)[0]["url"])
            with self.assertRaisesRegex(ValueError, "no reviewed public URL"):
                corpus.stage(self.root, self.output, True)
        self.assertFalse((self.output / self.entry["archive"]).exists())

    def test_oracle_manifest_disagreement_rejected(self):
        self.manifest["sha256"] = "0" * 64
        self.write_manifests()
        with self.assertRaises(ValueError):
            corpus.packages(self.root)

    def test_archive_traversal_rejected(self):
        self.entry["archive"] = "../retail.zip"
        self.write_manifests()
        with self.assertRaises(ValueError):
            corpus.packages(self.root)

    def test_package_budget_rejected(self):
        self.manifest["size"] = 33 * 1024 * 1024
        self.write_manifests()
        with self.assertRaises(ValueError):
            corpus.packages(self.root)

    def test_missing_requires_explicit_download(self):
        with self.assertRaises(ValueError):
            corpus.stage(self.root, self.output)

    def test_valid_cache_never_downloads(self):
        self.output.mkdir()
        (self.output / self.entry["archive"]).write_bytes(self.data)
        with patch.object(corpus.subprocess, "run", side_effect=AssertionError("network forbidden")):
            result = corpus.stage(self.root, self.output)
        self.assertEqual(1, result["archive_count"])
        self.assertTrue((self.output / "nightly-demo-corpus-provenance.json").is_file())

    def test_invalid_existing_package_preserved(self):
        self.output.mkdir()
        target = self.output / self.entry["archive"]
        target.write_bytes(b"existing user file")
        with self.assertRaises(ValueError):
            corpus.stage(self.root, self.output, True)
        self.assertEqual(b"existing user file", target.read_bytes())

    def test_bounded_download_verified_before_publish(self):
        def download(argv, **kwargs):
            self.assertEqual(130, kwargs["timeout"])
            self.assertEqual("120", argv[argv.index("--max-time") + 1])
            self.assertEqual(str(len(self.data)), argv[argv.index("--max-filesize") + 1])
            self.assertEqual("=https", argv[argv.index("--proto-redir") + 1])
            Path(argv[argv.index("--output") + 1]).write_bytes(self.data)

        with patch.object(corpus.subprocess, "run", side_effect=download):
            corpus.stage(self.root, self.output, True)
        self.assertEqual(self.data, (self.output / self.entry["archive"]).read_bytes())

    def test_bad_download_not_published(self):
        def download(argv, **kwargs):
            Path(argv[argv.index("--output") + 1]).write_bytes(b"corrupt")

        with patch.object(corpus.subprocess, "run", side_effect=download), self.assertRaises(ValueError):
            corpus.stage(self.root, self.output, True)
        self.assertFalse((self.output / self.entry["archive"]).exists())

    @unittest.skipUnless(shutil.which("ffmpeg") and shutil.which("ffprobe"), "FFmpeg tools required")
    def test_real_generated_audio_repeats_and_refuses_overwrite(self):
        first = audio.generate(self.root / "first")
        second = audio.generate(self.root / "second")
        self.assertEqual(first["sha256"], second["sha256"])
        self.assertGreater(float(first["probe"]["format"]["duration"]), 20)
        with self.assertRaises(ValueError):
            audio.generate(self.root / "first")


class LanWifiTopologyTests(unittest.TestCase):
    def run_case(self, serials=None, fault=None, ethernet_prefix=8, unaddressed=False, initially_down=False):
        serials = serials or ["emulator-5554", "emulator-5556"]
        calls, disabled = [], set()
        pending_route = [fault == "transient"]

        def adb(command, **kwargs):
            self.assertEqual(kwargs["timeout"], 10)
            serial, args = command[2], command[3:]
            calls.append((serial, args))
            address = {"emulator-5554": "10.0.2.16", "emulator-5556": "10.0.2.17", "emulator-5558": "10.0.2.18"}[serial]
            text, code, stderr = "", 0, ""
            if args == ["get-state"]:
                text = "offline" if fault == "offline" and serial == serials[-1] else "device"
            elif args == ["shell", "getprop", "ro.kernel.qemu"]:
                text = "0" if fault == "physical" and serial == serials[-1] else "1"
            elif args == ["shell", "su", "0", "id", "-u"]:
                text = "2000" if fault == "root" else "0"
            elif args[:5] == ["shell", "ip", "-4", "addr", "show"]:
                ip = "10.0.2.15" if args[-1] == "eth0" else address
                if fault == "duplicate" and args[-1] == "wlan0":
                    ip = "10.0.2.16"
                if fault == "unexpected-ethernet" and args[-1] == "eth0":
                    ip = "192.168.1.2"
                prefix = ethernet_prefix if args[-1] == "eth0" else 24
                if fault == "wrong-wifi-prefix" and args[-1] == "wlan0":
                    prefix = 8
                if fault == "wrong-ethernet-prefix" and args[-1] == "eth0":
                    prefix = 16
                text = f"inet {ip}/{prefix}"
                if fault == "extra-ethernet-address" and args[-1] == "eth0":
                    text += "\ninet 192.168.1.2/24"
                if unaddressed and serial == serials[-1] and args[-1] == "eth0":
                    text = ""
                if fault == "missing-wifi-address" and args[-1] == "wlan0":
                    text = ""
                if fault == "address-command-error" and args[-1] == "eth0":
                    code, text = 1, ""
                if fault == "malformed-ethernet-address" and args[-1] == "eth0":
                    text = "inet 10.0.2.15/33"
            elif args == ["shell", "su", "0", "ip", "link", "set", "dev", "eth0", "down"]:
                if fault == "mutation-error":
                    code = 1
                elif fault == "timeout":
                    raise subprocess.TimeoutExpired(command, 10)
                else:
                    disabled.add(serial)
            elif args[:5] == ["shell", "ip", "-o", "link", "show"]:
                code, stderr = 1, "request send failed: Permission denied"
            elif args[:7] == ["shell", "su", "0", "ip", "-o", "link", "show"]:
                up = fault == "still-up" or (serial not in disabled and not initially_down)
                text = "2: eth0: <BROADCAST,MULTICAST" + (",UP" if up else "") + "> mtu 1500"
                if fault == "missing-link":
                    code, text = 1, ""
                if fault == "root-link-denied" or (fault == "post-link-denied" and serial in disabled):
                    code, text, stderr = 1, "", "request send failed: Permission denied"
                if fault == "empty-link":
                    text = ""
                if fault == "wrong-link":
                    text = text.replace("eth0:", "wlan0:")
            elif args[:4] == ["shell", "ip", "route", "get"]:
                dev, src = ("wlan0", address) if serial in disabled and fault != "bad-route" else ("eth0", "10.0.2.15")
                if fault == "bad-source" and serial in disabled:
                    src = "10.0.2.15"
                text = f"{args[-1]} dev {dev} table 1016 src {src} uid 2000"
                if serial in disabled and pending_route[0]:
                    pending_route[0] = False
                    text, code = "", 2
            else:
                text = "diagnostic output"
            return subprocess.CompletedProcess(command, code, text, stderr or ("denied" if code else ""))

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory, "receipt.json")
            env = {"GITHUB_ACTIONS": "true", "SWEEP_LANE": "android-dual"}
            if fault == "wrong-lane":
                env["SWEEP_LANE"] = "android-single"
            with (
                patch.dict(wifi.os.environ, env),
                patch.object(wifi.subprocess, "run", side_effect=adb),
                patch.object(wifi.time, "monotonic", side_effect=[0, 1, 31] if fault == "transient" else [0, 31]),
                patch.object(wifi.time, "sleep"),
            ):
                if fault and fault != "transient":
                    with self.assertRaises((RuntimeError, ValueError, subprocess.TimeoutExpired)):
                        wifi.prepare(serials, output)
                else:
                    wifi.prepare(serials, output)
            receipt = json.loads(output.read_text())
            self.assertEqual(receipt["passed"], fault in (None, "transient"))
            if fault and fault != "transient":
                self.assertIn("error", receipt)
            return calls, receipt

    def test_private_ethernet_route_becomes_wifi_for_all_peers(self):
        for serials in (["emulator-5554", "emulator-5556"], ["emulator-5554", "emulator-5556", "emulator-5558"]):
            calls, receipt = self.run_case(serials)
            mutations = [i for i, (_, args) in enumerate(calls) if args[-1] == "down"]
            self.assertEqual(len(mutations), len(serials))
            for serial in serials:
                self.assertTrue(any(s == serial and a[-1] == "ro.kernel.qemu" for s, a in calls[: mutations[0]]))
            routes = [c["stdout"] for c in receipt["commands"] if c["arguments"][:4] == ["shell", "ip", "route", "get"]]
            self.assertTrue(any("dev eth0" in row for row in routes))
            self.assertTrue(all("dev wlan0" in row for row in routes[-len(serials) * (len(serials) - 1) :]))

    def test_link_inspection_uses_verified_root_but_routes_keep_shell_identity(self):
        calls, receipt = self.run_case()
        for serial in ("emulator-5554", "emulator-5556"):
            inspections = [
                i for i, (target, args) in enumerate(calls) if target == serial and "link" in args and "show" in args
            ]
            root_check = calls.index((serial, ["shell", "su", "0", "id", "-u"]))
            self.assertGreaterEqual(len(inspections), 2)
            for index in inspections:
                self.assertGreater(index, root_check)
                self.assertEqual(calls[index][1], ["shell", "su", "0", "ip", "-o", "link", "show", "dev", "eth0"])
        routes = [entry for entry in receipt["commands"] if "get" in entry["arguments"]]
        self.assertTrue(routes)
        self.assertTrue(all(entry["arguments"][:4] == ["shell", "ip", "route", "get"] for entry in routes))

    def test_root_link_permission_failure_remains_failure_with_evidence(self):
        for fault in ("root-link-denied", "post-link-denied"):
            calls, receipt = self.run_case(fault=fault)
            denied = receipt["commands"][-1]
            self.assertEqual(denied["returncode"], 1)
            self.assertEqual(denied["stdout"], "")
            self.assertEqual(denied["stderr"], "request send failed: Permission denied")
            self.assertFalse(receipt["passed"])
            self.assertEqual(any(args[-1] == "down" for _, args in calls), fault == "post-link-denied")

    def test_route_withdrawal_can_converge_without_repeating_mutation(self):
        calls, receipt = self.run_case(fault="transient")
        self.assertEqual(sum(args[-1] == "down" for _, args in calls), 2)
        self.assertTrue(any(c.get("returncode") == 2 for c in receipt["commands"]))

    def test_observed_ethernet_slash8_and_legacy_slash24_preserve_wifi_routes(self):
        for prefix in (8, 24):
            calls, receipt = self.run_case(ethernet_prefix=prefix)
            self.assertTrue(receipt["passed"])
            self.assertEqual(sum(args[-1] == "down" for _, args in calls), 2)

    def test_prefix_and_address_failures_prevent_all_mutations(self):
        for fault in (
            "wrong-wifi-prefix",
            "wrong-ethernet-prefix",
            "extra-ethernet-address",
            "missing-wifi-address",
            "address-command-error",
            "malformed-ethernet-address",
        ):
            calls, _ = self.run_case(fault=fault)
            self.assertFalse(any(args[-1] == "down" for _, args in calls), fault)

    def test_unaddressed_client_ethernet_requires_verified_link_and_wifi_routes(self):
        for initially_down in (False, True):
            calls, receipt = self.run_case(unaddressed=True, initially_down=initially_down)
            self.assertTrue(receipt["passed"])
            self.assertEqual(sum(args[-1] == "down" for _, args in calls), 2)
            for serial in ("emulator-5554", "emulator-5556"):
                first_mutation = next(i for i, (_, args) in enumerate(calls) if args[-1] == "down")
                self.assertTrue(
                    any(
                        s == serial and a == ["shell", "su", "0", "ip", "-o", "link", "show", "dev", "eth0"]
                        for s, a in calls[:first_mutation]
                    )
                )

    def test_unaddressed_ethernet_cannot_bypass_link_or_route_checks(self):
        for fault in ("missing-link", "empty-link", "wrong-link", "bad-route", "bad-source", "still-up"):
            calls, receipt = self.run_case(fault=fault, unaddressed=True)
            self.assertFalse(receipt["passed"])
            if fault in ("missing-link", "empty-link", "wrong-link"):
                self.assertFalse(any(args[-1] == "down" for _, args in calls))

    def test_invalid_targets_fail_before_mutation(self):
        for fault in ("offline", "physical", "root", "duplicate", "unexpected-ethernet", "wrong-lane"):
            calls, _ = self.run_case(fault=fault)
            self.assertFalse(any(args[-1] == "down" for _, args in calls), fault)
        for serials in (["phone", "emulator-5556"], ["emulator-5554", "emulator-5554"]):
            calls, _ = self.run_case(serials=serials, fault="invalid-serials")
            self.assertFalse(calls)

    def test_failed_mutation_and_incorrect_postconditions_remain_failure(self):
        for fault in ("mutation-error", "timeout", "still-up", "bad-route", "bad-source"):
            _, receipt = self.run_case(fault=fault)
            self.assertTrue(receipt["commands"])

    def test_only_dual_ci_setup_invokes_helper_before_tests(self):
        source = (ROOT / ".github/workflows/android-nightly-tooling.yml").read_text()
        boot = source.split("- name: Prepare isolated headless emulators", 1)[1].split(
            "- name: Execute reviewed test selection", 1
        )[0]
        dual = boot.split("if ($env:SWEEP_LANE -eq 'android-dual') {", 2)[-1]
        self.assertIn("control/android/helpers/prepare_lan_wifi.py", dual)
        self.assertIn("if ($LASTEXITCODE -ne 0) { throw 'Disposable emulator Wi-Fi topology setup failed' }", dual)
        self.assertIn("--serials @($avds.Serial)", dual)


if __name__ == "__main__":
    unittest.main()
