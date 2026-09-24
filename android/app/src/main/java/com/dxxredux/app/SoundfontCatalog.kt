package com.dxxredux.app

import android.content.Context

/** Curated download catalog. Keep unavailable candidates commented out for future hosting. */
internal object SoundfontCatalog {
    // Matches SOUNDFONT_VERSION/URL in get_deps/tool_versions.conf and assets/gm.sf2
    // This is an APK asset, not a removable entry in SoundfontStore's manifest
    fun bundled(context: Context): SoundfontStore.Font =
        SoundfontStore.Font(
            id = "",
            name = "nitro-shoe SC-55-style 1.34 (bundled)",
            download =
                SoundfontDownload(
                    name = "nitro-shoe SC-55-style 1.34",
                    url = "https://github.com/nitro-shoe/sc-55-soundfont/releases/download/v1.34/Roland.SC-55.sf2",
                    description =
                        "Community SC-55-style sample bank by nitro-shoe (9.9 MiB). " +
                            "An approximation of the module, with limited GS variations and no SFX drum kit. " +
                            "Included with the app and always available; cannot be deleted.",
                    websiteUrl = "https://github.com/nitro-shoe/sc-55-soundfont",
                    license =
                        context.assets
                            .open(
                                "licenses/nitro-shoe-sc55.txt",
                            ).bufferedReader()
                            .use { it.readText() },
                ),
        )

    // Full upstream license remains available offline in saved Info
    private val GENERAL_USER_LICENSE =
        """
        Copyright (c) 1997-2025 S. Christian Collins
        Source: https://github.com/mrbumpy409/GeneralUser-GS/blob/main/documentation/LICENSE.txt
        
        *** GeneralUser GS v2.0.3 ***
        ***      License v2.0     ***
        
        ** License of the complete work **
        You may use GeneralUser GS without restriction for your own music creation,
        private or commercial. This SoundFont bank is provided to the community free of
        charge. Please feel free to use it in your software projects, and to modify the
        SoundFont bank or its packaging to suit your needs.
        
        ** License of contained samples **
        GeneralUser GS inherits the usage rights of the samples contained within, all of
        which allow full use in music production, including the ability to make profit
        from musical recordings created with GeneralUser GS.
        
        Many of the samples are original, but some were taken from other banks freely
        (and legally) available on the Internet from various SoundFont websites. Because
        GeneralUser GS originated as a personal project with no intention for
        publication, I cannot be 100% sure where all of the samples originated, although
        I do know that none of them came from commercially published SoundFont packages
        or sample CDs. Regardless, many "free" SoundFonts available on the web may
        indeed contain samples of questionable origin. My understanding of the
        copyrights of all samples is only as good as the information provided by the
        original sources. If you become aware of any restricted samples being used in
        GeneralUser GS, please let me know so I can replace them.
        
        This uncertainty may concern you if you intend to use GeneralUser GS in a
        commercial software product. That being said, I have never received any
        complaint regarding sample ownership since I published the original GeneralUser
        GS back in 2000, and as far as I am aware, neither have any of the companies
        creating commercial software products using GeneralUser GS.
        
        ** More info **
        If you plan to feature GeneralUser GS on your own website, please do not link
        directly to my download files. Either link to my website, or provide your own
        local copy instead.
        
        I hope you enjoy GeneralUser GS! This SoundFont bank is the product of many
        years of hard work.
        
        You can find updates to GeneralUser GS and more of my virtual instruments at:
        http://www.schristiancollins.com
        
        I can be reached via the contact page on my website here:
        https://www.schristiancollins.com/contact
        
        Thank you!
        -~Chris
        """.trimIndent()

    // Verified 2026-09-22; see android/ai tool plans/music/soundfont-source-catalog.md
    // Active entries must be direct HTTPS SF2 files within SoundfontStore.MAX_BYTES
    // Pin a release asset, retain its complete license/credits, and test the actual download
    // The source website belongs to the bank author; the download can be a credited mirror
    fun entries(context: Context): List<SoundfontDownload> =
        entries(
            context.assets
                .open("licenses/TimGM6mb.txt")
                .bufferedReader()
                .use { it.readText() },
        )

    internal fun entries(timGmLicense: String): List<SoundfontDownload> =
        listOf(
            // Former bundled bank; retained as an optional upstream download at the user's request
            // SHA-256 c5378b62028c920cb11e4803327983fee2f2cdff5dc89c708e39da417e51c854
            SoundfontDownload(
                name = "TimGM6mb",
                url = "https://github.com/arbruijn/TimGM6mb/releases/download/v20100822/TimGM6mb.sf2",
                description =
                    "Compact General MIDI bank by Tim Brechbill, with contributions by David Bolton. " +
                        "Version 20100822, 5.7 MiB. Previously bundled with the app.",
                websiteUrl = "https://github.com/arbruijn/TimGM6mb",
                license = timGmLicense,
            ),
            // Latest bank version available in the checked GitHub releases (Codetta mirror)
            // 32,319,396 bytes; SHA-256 9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe
            SoundfontDownload(
                name = "GeneralUser GS 2.0.3 beta",
                url = "https://github.com/krtw00/codetta/releases/download/soundfont-bundle/GeneralUser-GS.sf2",
                description =
                    "Compact GM/GS bank by S. Christian Collins (30.8 MiB). " +
                        "The SF2 identifies itself as a beta. Download hosted by Codetta. " +
                        "Some advanced instrument features are not reproduced by our current synthesizer.",
                websiteUrl = "https://github.com/mrbumpy409/GeneralUser-GS",
                license = GENERAL_USER_LICENSE,
            ),
            // RELEASE AVAILABLE, blocked by current 64 MiB limit: 148,398,306 bytes
            // Enable after larger-bank memory/loading validation; no new hosting needed
            // SoundfontDownload(
            //     name = "FluidR3 GM 3.1",
            //     url = "https://github.com/pianobooster/fluid-soundfont/releases/download/v3.1/FluidR3_GM.sf2",
            //     description = "Frank Wen's complete GM bank; 141.5 MiB, broad acoustic coverage.",
            //     websiteUrl = "https://github.com/pianobooster/fluid-soundfont",
            //     license = "MIT, Copyright (c) 2000-2002, 2008 Frank Wen. Preserve COPYING and README credits.",
            // ),
            // RELEASE AVAILABLE, blocked by current 64 MiB limit: 135,020,964 bytes
            // Enable after larger-bank validation and inclusion of the full CC license/credits
            // SoundfontDownload(
            //     name = "OPL-3 FM 128M 1.0",
            //     url = "https://github.com/Mindwerks/opl3-soundfont/releases/download/1.0/OPL-3_FM_128M.sf2",
            //     description = "Zandro Reveille's sampled Sound Blaster 16-style bank; 128.8 MiB. Sample playback, not live FM.",
            //     websiteUrl = "https://github.com/Mindwerks/opl3-soundfont",
            //     license = "CC BY-SA 4.0, Zandro Reveille. https://creativecommons.org/licenses/by-sa/4.0/",
            // ),
            // FUTURE HOSTING: publish a verified SF2 in our GitHub Releases
            // SF3 is unsupported; verify the decoded SF2 size, credits and memory use first
            // SoundfontDownload(
            //     name = "FluidR3Mono GM",
            //     url = "", // TODO: future GitHub release SF2 URL
            //     description = "Michael Cowgill's mono adaptation of FluidR3; commonly distributed as SF3.",
            //     websiteUrl = "https://github.com/musescore/MuseScore/tree/main/share/sound",
            //     license = "MIT; retain Frank Wen and Michael Cowgill notices and the selected artifact's complete credits.",
            // ),
            // FUTURE HOSTING: mirror the SF2 in our GitHub Releases after larger-bank validation
            // Current non-GitHub source below; approximately 206 MiB, not the smaller SF3
            // SoundfontDownload(
            //     name = "MuseScore General 0.2.1",
            //     url = "https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/MuseScore_General.sf2",
            //     description = "MuseScore's GM bank, derived from FluidR3Mono with replacement instruments.",
            //     websiteUrl = "https://ftp.osuosl.org/pub/musescore/soundfont/MuseScore_General/",
            //     license = "MIT; preserve MuseScore_General_License.md and the sample-source credits from this distribution.",
            // ),
            // FUTURE HOSTING: prepare and publish a licensed SF2 in our GitHub Releases
            // Current MuseScore MS Basic asset is SF3; conversion, size and notices need validation
            // SoundfontDownload(
            //     name = "MS Basic",
            //     url = "", // TODO: future GitHub release SF2 URL
            //     description = "Current MuseScore basic playback bank; assess separately from older MuseScore General.",
            //     websiteUrl = "https://github.com/musescore/MuseScore/tree/main/share/sound",
            //     license = "MIT ancestry; reconcile MS Basic_License.md with the exact selected asset and all credits before hosting.",
            // ),
            // FUTURE HOSTING: unpack and publish SF2 in our GitHub Releases after checking embedded notices
            // RKhive advertises CC0; verify each bank's own provenance instead of assuming the site owns all samples
            // SoundfontDownload(
            //     name = "Chaos Bank 1.9",
            //     url = "https://rkhive.com/new/new_banks/chaos_bank_v1_9.zip", // Replace with future GitHub SF2 URL
            //     description = "Older compact instrument bank archived by RKhive; listed at 9.63 MB.",
            //     websiteUrl = "https://rkhive.com/banks.html",
            //     license = "Archive claims CC0 1.0 at https://rkhive.com/legal.html; bank-level notices still to verify.",
            // ),
            // FUTURE HOSTING: unpack and publish SF2 in our GitHub Releases after checking embedded notices
            // SoundfontDownload(
            //     name = "JNS-GM 2",
            //     url = "https://rkhive.com/new/new_banks/jnsgm2.zip", // Replace with future GitHub SF2 URL
            //     description = "Jordi Navarro Subirana's GM bank; archive lists 28.85 MB.",
            //     websiteUrl = "https://www.polyphone-soundfonts.com/documents/27-instrument-sets/55-jns-gm-2",
            //     license = "Archive claims CC0 1.0; verify original author and embedded sample notices before redistribution.",
            // ),
            // FUTURE HOSTING: unpack and publish SF2 in our GitHub Releases after checking embedded notices
            // SoundfontDownload(
            //     name = "Masterpiece",
            //     url = "https://rkhive.com/new/new_banks/masterpiece.zip", // Replace with future GitHub SF2 URL
            //     description = "Legacy instrument bank archived by RKhive; listed at 25.23 MB.",
            //     websiteUrl = "https://rkhive.com/banks.html",
            //     license = "Archive claims CC0 1.0; author attribution and bank-level permissions still to verify.",
            // ),
            // FUTURE HOSTING: unpack and publish SF2 in our GitHub Releases after checking embedded notices
            // SoundfontDownload(
            //     name = "Unison",
            //     url = "https://rkhive.com/new/new_banks/unison.zip", // Replace with future GitHub SF2 URL
            //     description = "Legacy instrument bank archived by RKhive; listed at 22.32 MB.",
            //     websiteUrl = "https://rkhive.com/banks.html",
            //     license = "Archive claims CC0 1.0; author attribution and bank-level permissions still to verify.",
            // ),
            // FUTURE HOSTING: unpack and publish SF2 in our GitHub Releases after checking coverage and notices
            // SoundfontDownload(
            //     name = "Music Theory 2",
            //     url = "https://rkhive.com/new/new_banks/mustheory2.zip", // Replace with future GitHub SF2 URL
            //     description = "RKhive instrument-bank candidate; listed at 25.94 MB. GM coverage still to verify.",
            //     websiteUrl = "https://rkhive.com/banks.html",
            //     license = "Archive claims CC0 1.0; author attribution and bank-level permissions still to verify.",
            // ),
            // FUTURE HOSTING: publish in our GitHub Releases only after redistribution permission is established
            // SoundfontDownload(
            //     name = "Florestan Basic GM GS",
            //     url = "https://dev.nando.audio/_static/sf2/__Florestan_Basic_GM_GS.zip", // Future GitHub SF2 URL
            //     description = "Nando Florestan's small GM/GS bank, sampled from a Roland Sound Canvas; listed at 3.3 MB.",
            //     websiteUrl = "https://dev.nando.audio/pages/soundfonts.html",
            //     license = "Free download is documented; verify archive license and sample redistribution terms. Not established as CC0.",
            // ),
            // FUTURE HOSTING: publish in our GitHub Releases only after terms/permission and drum compatibility are checked
            // SoundfontDownload(
            //     name = "Vintage Dreams Waves 2.1",
            //     url = "", // TODO: future GitHub release SF2 URL; author download page below
            //     description = "Ian Wilson's synthetic instrument bank with short and single-cycle waveforms.",
            //     websiteUrl = "https://analoguesque.x10host.com/SoundFonts/",
            //     license = "Custom freeware; author prohibits sale without permission. Verify this version's redistribution terms.",
            // ),
            // FUTURE HOSTING: publish in our GitHub Releases only after third-party permissions and large-bank support
            // SoundfontDownload(
            //     name = "Arachno 1.0",
            //     url = "", // TODO: future GitHub release SF2 URL; author offers ZIP/sfArk on the source page
            //     description = "Maxime Abbey's GM bank with 128 instruments and 9 drum kits; roughly 150 MB unpacked.",
            //     websiteUrl = "https://www.arachnosoft.com/main/soundfont.php",
            //     license = "Custom freeware, primarily private/non-commercial; commercial use requires original authors' consent. Not cleared.",
            // ),
            // FUTURE HOSTING: publish in our GitHub Releases after original license/credits and large-bank validation
            // SoundfontDownload(
            //     name = "FatBoy",
            //     url = "", // TODO: future GitHub release SF2 URL; verify current author distribution
            //     description = "Large GM bank used by MIDI.js renderings; investigate the current original distribution.",
            //     websiteUrl = "https://fatboy.site/",
            //     license = "MIDI.js distributor reports CC BY-SA 3.0; original license, credits and sample provenance still to verify.",
            // ),
            // FUTURE HOSTING: publish in our GitHub Releases after original license/credits and large-bank validation
            // SoundfontDownload(
            //     name = "Musyng Kite",
            //     url = "", // TODO: future GitHub release SF2 URL; reported upstream distribution uses sfPack
            //     description = "Very large GM bank used by MIDI.js; reported uncompressed size about 1.75 GB.",
            //     websiteUrl = "https://github.com/gleitz/midi-js-soundfonts",
            //     license = "MIDI.js distributor reports CC BY-SA 3.0; trace original bank/credits before hosting, not the JS renderings.",
            // ),
            // FUTURE HOSTING: publish in our GitHub Releases only after explicit redistribution clearance and large-bank support
            // SoundfontDownload(
            //     name = "Timbres of Heaven 4.00(G)",
            //     url = "https://midkar.com/SoundFonts/Timbres%20of%20Heaven%20(XGM)%204.00(G).7z", // Future GitHub SF2 URL
            //     description = "Don Allen's GM/GS/XG-oriented bank distributed by MidKar as a large 7z archive.",
            //     websiteUrl = "https://midkar.com/SoundFonts/TOH.html",
            //     license = "Redistribution permission not established; inspect this exact version's terms. Older listings restrict sharing.",
            // ),
            // FUTURE HOSTING: unpack and publish in our GitHub Releases after license/credits and GM coverage checks
            // SoundfontDownload(
            //     name = "Aspirin 160 GMGS 2015",
            //     url = "https://www.synthfont.com/SoundFonts/Aspirin_160_GMGS_2015.sfpack", // Future GitHub SF2 URL
            //     description = "Legacy compact GM/GS bank; SynthFont lists 15.8 MB unpacked.",
            //     websiteUrl = "https://www.synthfont.com/soundfonts.html",
            //     license = "Redistribution license and original author notices not yet verified; free download alone is insufficient.",
            // ),
            // FUTURE HOSTING: unpack and publish in our GitHub Releases after license/credits and GM coverage checks
            // SoundfontDownload(
            //     name = "bennetng AnotherGS 2.1",
            //     url = "https://www.synthfont.com/SoundFonts/bennetng_AnotherGS_v2-1.sfArk", // Future GitHub SF2 URL
            //     description = "Alternative GS bank by bennetng; SynthFont lists 32.5 MB unpacked.",
            //     websiteUrl = "https://www.synthfont.com/soundfonts.html",
            //     license = "Redistribution license and original author notices not yet verified; inspect the original archive.",
            // ),
            // FUTURE HOSTING: unpack and publish in our GitHub Releases after license/credits and GM coverage checks
            // SoundfontDownload(
            //     name = "JCLive 2.1",
            //     url = "https://www.synthfont.com/SoundFonts/JClive21.sfArk", // Future GitHub SF2 URL
            //     description = "Legacy bank distributed by SynthFont; listed at 50 MB unpacked.",
            //     websiteUrl = "https://www.synthfont.com/soundfonts.html",
            //     license = "Redistribution license and original author notices not yet verified; inspect the original archive.",
            // ),
        )

    // Not future-hosting candidates under the current policy: FreePats
    // (GPL), the GPL-tagged open-soundfonts/SGM_V2_01_soundfonts mirror, proprietary
    // SC-55/GM.DLS/Creative ROM rips without redistribution grants
}
