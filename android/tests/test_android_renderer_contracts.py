import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


class AndroidRendererContractsTest(unittest.TestCase):
    def test_cmake_guards_ogl_target_fixups(self) -> None:
        text = (REPO / "android/app/src/main/cpp/CMakeLists.txt").read_text(encoding="utf-8")
        for game in ("d1", "d2"):
            start = text.index(f"Android-specific fixups for dxx-redux-{game} target")
            end = text.find("Android-specific fixups for", start + 1)
            section = text[start : None if end == -1 else end]
            mutation = section.index("target_compile_definitions(${DXX_TARGET_PREFIX}arch_ogl")
            guard = section.rfind("if(OPENGL)", 0, mutation)
            self.assertGreaterEqual(guard, 0)
            self.assertLess(mutation, section.index("endif() # OPENGL", guard))
            self.assertIn("INTROSPECT_ON", section[mutation : section.index(")", mutation)])

    def test_automap_failure_uses_window_as_allocation_owner(self) -> None:
        source = (REPO / "d2/main/automap.c").read_text(encoding="utf-8")

        def block(start: str, end: str) -> str:
            begin = source.index(start)
            return source[begin : source.index(end, begin)]

        allocation = block("if (!am->edges || !am->drawingListBright)", "am->zoom")
        self.assertIn("window_close(automap_wind);", allocation)
        self.assertNotIn("d_free(am->edges)", allocation)
        self.assertNotIn("d_free(am->drawingListBright)", allocation)

        pcx = block("if (pcx_error != PCX_ERROR_NONE)", 'AUTOMAP_LOGI("PCX loaded')
        android = pcx[pcx.index("#ifdef ANDROID") : pcx.index("#else", pcx.index("#ifdef ANDROID"))]
        self.assertIn("window_close(automap_wind);", android)
        self.assertNotIn("d_free(am)", android)
        self.assertNotIn("d_free(am->edges)", android)

    def test_merged_wall_cache_releases_texture_before_reset(self) -> None:
        source = (REPO / "android/app/src/main/cpp/shared/merged_wall_debug.c").read_text(encoding="utf-8")
        start = source.index("void android_merged_wall_cached_texmerge_clear(")
        end = source.index("void android_merged_wall_cached_texmerge_clear_cache", start)
        body = source[start:end]
        self.assertLess(
            body.index("free_texture(entries[i].texture)"),
            body.index("android_merged_wall_cached_texmerge_reset_entry(&entries[i])"),
        )
        cache_start = end
        cache_end = source.index("int android_merged_wall_cached_texmerge_choose_size", cache_start)
        self.assertIn(
            "MERGED_WALL_CACHED_TEXMERGE_COUNT,\n\t                                          ogl_freetexture",
            source[cache_start:cache_end],
        )

    def test_resolution_policy_and_paired_allocations(self) -> None:
        policy = (REPO / "android/app/src/main/cpp/shared/android_render_resolution.h").read_text(encoding="utf-8")
        self.assertRegex(policy, r"ANDROID_RENDER_MIN_WIDTH\s+320u")
        self.assertRegex(policy, r"ANDROID_RENDER_MIN_HEIGHT\s+200u")
        self.assertRegex(policy, r"ANDROID_RENDER_MAX_DIMENSION\s+4096u")
        self.assertRegex(policy, r"ANDROID_RENDER_MAX_PIXELS\s+\(3840u \* 2160u\)")
        self.assertIn("(size_t) width * (size_t) height", policy)

        for game in ("d1", "d2"):
            gr = (REPO / game / "arch/ogl/gr.c").read_text(encoding="utf-8")
            self.assertIn('#include "android_render_resolution.h"', gr)
            self.assertIn("android_render_resolution_valid(SM_W(mode), SM_H(mode))", gr)
            self.assertIn("(size_t)w * (size_t)h", gr)
            self.assertIn("if (!new_bm_data)", gr)

            ogl = (REPO / game / "arch/ogl/ogl.c").read_text(encoding="utf-8")
            allocation = ogl.index("new_pixels = d_malloc(pixels_size)")
            validation = ogl.index("if ((new_pixels == NULL) || (new_texbuf == NULL))", allocation)
            old_free = ogl.index("d_free(pixels)", validation)
            publication = ogl.index("pixels = new_pixels", old_free)
            self.assertLess(allocation, validation)
            self.assertLess(validation, old_free)
            self.assertLess(old_free, publication)

    def test_texture_binds_do_not_share_cache_across_units(self) -> None:
        shared = (REPO / "android/app/src/main/cpp/shared/ogl_texture_android.c").read_text(encoding="utf-8")
        start = shared.index("void android_ogl_bind_texture_2d")
        body = shared[start : shared.index("void android_ogl_enable_texture_2d", start)]
        self.assertIn("glBindTexture(GL_TEXTURE_2D, handle);", body)
        self.assertNotIn("handle != *state->last_bound_tex", body)

        for game in ("d1", "d2"):
            source = (REPO / game / "arch/ogl/ogl.c").read_text(encoding="utf-8")
            start = source.index("#define OGL_BINDTEXTURE(a)")
            macro = source[start : source.index("#else", start)]
            self.assertIn("glBindTexture(GL_TEXTURE_2D, a);", macro)
            self.assertNotIn("!= ogl_last_bound_tex", macro)

    def test_texture_extension_lookup_has_one_shared_owner(self) -> None:
        shared = (REPO / "android/app/src/main/cpp/shared/ogl_texture_android.c").read_text(
            encoding="utf-8"
        )
        start = shared.index("int android_ogl_read_texture_with_extensions(")
        end = shared.index("static void apply_bound_min_mag_filter", start)
        body = shared[start:end]
        self.assertIn('static const char *exts[] = { ".png", ".jpg", ".tga" };', body)
        self.assertLess(body.index("png_attempts++"), body.index("for (ei = 0; ei < 3; ei++)"))
        self.assertLess(body.index("png_slot_us[slot]"), body.index("png_hit_slot = slot"))
        self.assertLess(body.index("png_ext_us[ei]"), body.index("png_hit_ext = ei"))
        self.assertIn("clock_gettime(CLOCK_MONOTONIC, ts);", shared)
        self.assertIn("(end->tv_sec - start->tv_sec) * 1000000", shared)
        self.assertIn("(end->tv_nsec - start->tv_nsec) / 1000", shared)

        profile = (REPO / "android/app/src/main/cpp/shared/android_profile.c").read_text(
            encoding="utf-8"
        )
        self.assertEqual(profile.count("void android_profile_texture_lookup_note_ktx2("), 1)
        for game in ("d1", "d2"):
            source = (REPO / game / "arch/ogl/ogl.c").read_text(encoding="utf-8")
            self.assertNotIn("static int ogl_read_texture_with_extensions", source)
            self.assertNotIn("static void android_profile_texture_lookup_note_ktx2", source)
            self.assertIn("android_ogl_read_texture_with_extensions", source)
            self.assertIn("clock_gettime(CLOCK_MONOTONIC, ts);", source)
            self.assertIn("(end->tv_sec - start->tv_sec) * 1000000", source)
            self.assertIn("(end->tv_nsec - start->tv_nsec) / 1000", source)


if __name__ == "__main__":
    unittest.main()
