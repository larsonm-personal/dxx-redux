# Original Pumo Metillis save across desktop and Android

The campaign pilot earned the blue and gold keys in level 4 at score 35,360, with three lives and 20.69 shields. Ordinary steering and gunfire carried the ship through the room 843 ambush to room 948. The original v1.1 archive was imported through the Android launcher alongside the retail D2 data and XL resources.

The desktop export recorded 334 source entries. Android reconstructed 333. Two model references had requested Barrel-tankmap.tga with different capitalization. Both entries named the same owner and hashed to 169b64dd910347e100e20ef1ba64397c74dfbdf0140cc137d00d812a527d858b. The app previously displayed "Cannot load save" for this checkpoint. The revised comparison accepts repeated spellings of one owned path with identical content. The descriptor controls exercise missing files, different owners, changed hashes and conflicting aliases.

On an Android 14 x86_64 emulator, the ordinary developer-controls-off APK passed 37 hub steps using the unmodified published archive. The Load Game menu then restored the original earned desktop save in room 948 with both keys and the expected score. Android wrote its own quicksave (SHA-256 81aab943fe69d1430f4e8ddec48b15290071b1a7375b68fe45d4da979829743c). A fresh game process loaded that file with the same keys and score. The cold run passed all seven steps.

The ordinary APK explicitly declined a subsequent XL input recording. The combined capture attempt ended there and restored the protected profile. The two save checks completed before that recording request. The native Linux Observatory descriptor controls also passed using retail-only base data.
