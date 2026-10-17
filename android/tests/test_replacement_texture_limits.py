#!/usr/bin/env python3
"""Regression guards for replacement-texture decode and upload limits."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def read(relative: str) -> str:
    return (ROOT / relative).read_text(encoding="utf-8")


def main() -> None:
    loader = read("android/app/src/main/cpp/shared/pngfile.c")
    dxa_limits = read("android/app/src/main/cpp/shared/dxa_admission_limits.h")
    png = loader.split("int read_png", 1)[1].split("int write_png", 1)[0]
    assert "#define STBI_MAX_DIMENSIONS 16384" in loader
    assert png.index("stbi_info_from_memory") < png.index("stbi_load_from_memory")
    assert "ANDROID_REPLACEMENT_TEXTURE_MAX_DECODED_BYTES" in png
    assert "texture_lookup_build_file_index_recursive" not in loader
    assert "PHYSFS_enumerate(" in loader
    assert 'texture_lookup_path_equals_ci(leaf, "textures")' in loader
    assert "Do not recursively index unrelated mounted game data" in loader
    assert "type=texture_index" in loader
    for limit in (
        "TEXTURE_LOOKUP_MAX_DEPTH",
        "TEXTURE_LOOKUP_MAX_ENTRIES",
        "TEXTURE_LOOKUP_MAX_DIRECTORIES",
        "TEXTURE_LOOKUP_MAX_PATH_BYTES",
        "TEXTURE_LOOKUP_MAX_RETAINED_PATH_BYTES",
        "TEXTURE_LOOKUP_MAX_INDEXED_PATH_BYTES",
    ):
        assert limit in loader
    assert "#define DXX_DXA_MAX_ARCHIVE_ENTRIES      4096u" in dxa_limits
    assert "#define DXX_DXA_MAX_TEXTURE_ENTRIES      4096u" in dxa_limits
    assert "#define DXX_DXA_MAX_TOTAL_PATH_BYTES     (1024u * 1024u)" in dxa_limits
    assert "DXX_DXA_MAX_ENTRY_BYTES" in loader
    assert "DXX_DXA_MAX_TOTAL_EXPANDED_BYTES" in loader

    ktx = loader.split("int read_ktx2_file", 1)[1]
    assert "base->baseWidth > ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION" in ktx
    assert "base->baseHeight > ANDROID_REPLACEMENT_TEXTURE_MAX_DIMENSION" in ktx
    assert "image_size != expected_size" in ktx
    assert "total > UINT_MAX - 4u - image_size" in ktx

    upload = read("android/app/src/main/cpp/shared/ogl_texture_android.c")
    assert "glDeleteTextures(1, &texture->handle)" in upload
    assert "texture->handle = 0" in upload
    for game in ("d1", "d2"):
        ogl = read(f"{game}/arch/ogl/ogl.c")
        assert "android_ogl_upload_ktx2" in ogl

    scanner = read("android/app/src/main/java/com/dxxredux/app/DxaTextureScanner.kt")
    assert 'name.endsWith(".jpg")' in scanner
    assert "validateStructure" in scanner
    assert "MAX_PATH_DEPTH" in scanner
    assert "MAX_ARCHIVE_ENTRIES" in scanner
    assert "MAX_TEXTURE_ENTRIES" in scanner
    assert "private const val MAX_ARCHIVE_ENTRIES = ExtractionLimits.MAX_ENTRIES" in scanner
    assert "private const val MAX_TOTAL_PATH_BYTES = 1024 * 1024" in scanner
    assert "ExtractionLimits.MAX_DXA_SOURCE_BYTES" in scanner
    assert "ExtractionBudget()" in scanner
    manager = read("android/app/src/main/java/com/dxxredux/app/ModManager.kt")
    assert "Refusing to enable unsafe or unreadable mod" in manager
    assert manager.count("?.canEnable == true") == 1
    assert "enabled = scan.canEnable" in manager
    assert "scan?.canEnable != true" in manager

    generator = read("game_data/mods/d2x-xl/convert_d2xxl_textures.ps1")
    assert "$engineTextureCap = 2048" in generator
    assert "if ($skipDownscale) { 0 }" not in generator
    aggregate = read("game_data/mods/d2x-xl/convert_all.ps1")
    assert "if ($skipQualityDownscale -or $maxDim -le 0) { 2048 }" in aggregate

    print("replacement texture limit guards passed")


if __name__ == "__main__":
    main()
