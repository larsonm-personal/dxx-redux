import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
ANDROID = ROOT / "android"


class DeploymentScriptContractsTest(unittest.TestCase):
    def test_install_failure_preserves_existing_app_data_by_default(self) -> None:
        text = (ANDROID / "install-aab.ps1").read_text(encoding="utf-8")
        failure = text[text.index("$installOutput =") :]
        self.assertIn("[switch]$ResetAppData", text)
        self.assertIn("INSTALL_FAILED_UPDATE_INCOMPATIBLE", failure)
        for guard in ("if (-not $ResetAppData)", "if (-not $signatureMismatch)"):
            self.assertLess(failure.index(guard), failure.index("& $adbExe uninstall"))
        self.assertIn("existing app data was preserved", failure)
        self.assertIn("if ($uninstallExitCode -ne 0)", failure)

    def test_play_upload_uses_the_selected_artifact_identity(self) -> None:
        wrapper = (ANDROID / "0_upload_to_test.ps1").read_text(encoding="utf-8")
        builder = (ANDROID / "1_build-aab.ps1").read_text(encoding="utf-8")
        deploy = (ANDROID / "2_deploy-playstore.ps1").read_text(encoding="utf-8")
        self.assertIn("-OutputPath $artifactPath", wrapper)
        self.assertIn("-AabPath $artifactPath", wrapper)
        self.assertIn("[string]$OutputPath", builder)
        self.assertIn("if ($OutputPath)", builder)
        self.assertIn("deployment will not guess among build artifacts", deploy)
        self.assertNotIn("Sort-Object LastWriteTime", deploy)
        self.assertIn("Get-FileHash -LiteralPath $AabPath -Algorithm SHA256", deploy)
        self.assertIn("$loadedDigest -ne $aabDigest", deploy)
        used = deploy[deploy.index('if ($errBody -match "already been used")') :]
        self.assertIn("Write-Error", used[:500])
        self.assertNotIn("$alreadyUploaded = $true", deploy)

    def test_powershell_bootstrap_is_root_aware(self) -> None:
        source = (ANDROID / "get_deps/helpers/get_powershell.sh").read_text(encoding="utf-8")
        start = source.index("run_with_privilege()")
        run = source[start : source.index("get_powershell_package_name()", start)]
        root = run[run.index("if [[ $EUID -eq 0 ]]") : run.index("elif", run.index("if [[ $EUID -eq 0 ]]"))]
        self.assertIn('"$@"', root)
        self.assertNotIn("sudo", root)
        for command in (
            "apt update",
            'apt install -y "$deb_file"',
            'dpkg -i "$deb_file"',
            'dnf install -y "$rpm_file"',
            'yum install -y "$rpm_file"',
        ):
            self.assertIn(f"run_with_privilege {command}", source)
            self.assertNotIn(f"sudo {command}", source)

    def test_game_data_push_is_additive_or_manifest_scoped(self) -> None:
        script = (ANDROID / "helpers/push_game_data.sh").read_text(encoding="utf-8")
        readme = (ROOT / "game_data_to_copy_to_emulator/README.md").read_text(encoding="utf-8")
        self.assertLess(
            script.index("Nothing to push; leaving all device files unchanged"),
            script.index("run-as $PACKAGE mkdir -p $DEST"),
        )
        for token in ('if [ "$SYNC_OWNED" = "1" ]', "requires an explicit ANDROID_SERIAL", ".push_game_data_owned"):
            self.assertIn(token, script)
        self.assertNotIn("ls $DEVICE_DOWNLOAD_DIR/", script)
        self.assertNotIn("ls $FILES_DIR/", script)
        dedicated = "/sdcard/Download/dxx-redux-test-data"
        self.assertIn(dedicated, script)
        self.assertIn(dedicated, readme)
        self.assertIn("default command is additive", readme)
        for token in ("sha256sum", "staged digest mismatch", "final digest mismatch", "mv -f $REMOTE_TEMP $REMOTE_PATH"):
            self.assertIn(token, script)

    def test_single_avd_rebuild_is_target_scoped(self) -> None:
        runner = (ANDROID / "Run-Emulator.ps1").read_text(encoding="utf-8")
        creator = (ANDROID / "get_deps/helpers/create_light_avds.ps1").read_text(encoding="utf-8")
        rebuild = runner[runner.index("if ($Rebuild)") : runner.index('Write-Host "=== Launching emulator')]
        for token in ("emu avd name", "-s $targetSerial emu kill", "-Force -AvdName $AVD_NAME"):
            self.assertIn(token, rebuild)
        self.assertNotIn("Stop-Process", rebuild)
        self.assertNotIn('Get-Process -Name "qemu-system*"', rebuild)
        for token in ("[string]$AvdName", "$selectedAvds", "foreach ($avd in $selectedAvds)"):
            self.assertIn(token, creator)


if __name__ == "__main__":
    unittest.main()
