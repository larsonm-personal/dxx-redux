/* Produce an inconsistent fixture using the engine's disk contracts */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "coop_recovery.h"
#include "coop_powerup_duplication.h"
#include "powerup.h"
#include "weapon.h"

_Static_assert(sizeof(coop_save_metadata) == 3624, "Coop v9 metadata ABI changed");
_Static_assert(sizeof(coop_powerup_collection) == 56, "Coop pickup ABI changed");
_Static_assert(sizeof(coop_recovery_item) == 105, "Coop recovery ABI changed");

int main(int argc, char **argv)
{
    if ((argc != 3 && argc != 4) || !strcmp(argv[1], argv[2])) {
        fprintf(stderr, "Usage: coop_restore_fixture INPUT OUTPUT [bad_counts], or INPUT --inspect\n");
        return 1;
    }
    int bad_counts = argc == 4 && !strcmp(argv[3], "bad_counts");
    if (argc == 4 && !bad_counts) return 1;
    FILE *file = fopen(argv[1], "rb");
    if (!file) return 1;
    if (fseek(file, 0, SEEK_END)) { fclose(file); return 1; }
    long length = ftell(file);
    if (length < (long) sizeof(coop_save_footer) || length > 64 * 1024 * 1024) { fclose(file); return 1; }
    unsigned char *data = malloc((size_t) length);
    if (!data) { fclose(file); return 1; }
    rewind(file);
    int ok = fread(data, 1, (size_t) length, file) == (size_t) length;
    fclose(file);
    if (!ok) { free(data); return 1; }
    for (long end = length; end >= (long) sizeof(coop_save_footer); end--) {
        coop_save_footer footer;
        memcpy(&footer, data + end - sizeof(footer), sizeof(footer));
        if (footer.tag != COOP_SAVE_FOOTER_TAG || footer.version != COOP_SAVE_META_VER ||
            footer.payload_size < sizeof(coop_save_metadata) || footer.payload_size > end - sizeof(footer)) continue;
        long start = end - sizeof(footer) - footer.payload_size;
        if (coop_save_checksum(data + start, footer.payload_size, 2166136261u) != footer.checksum) continue;
        coop_save_metadata meta;
        memcpy(&meta, data + start, sizeof(meta));
        if (!meta.num_active_players || meta.num_active_players > 8 ||
            sizeof(meta) + (size_t) footer.collection_count * sizeof(coop_powerup_collection) +
                (size_t) meta.recovery_count * sizeof(coop_recovery_item) != footer.payload_size) continue;
        if (!strcmp(argv[2], "--inspect")) {
            printf("mission=%.*s level=%d pickups=%u recovery=%u checksum=%08x\n",
                   8, meta.mission_name, meta.level_num, footer.collection_count, meta.recovery_count, footer.checksum);
            for (unsigned i = 0; i < meta.num_active_players; i++) {
                const coop_player_record *player = &meta.active_players[i];
                printf("player=%.*s homing=%u primary_flags=%u shields=%d energy=%d\n",
                       COOP_CALLSIGN_LEN, player->callsign, player->secondary_ammo[HOMING_INDEX],
                       player->primary_weapon_flags, player->shields, player->energy);
            }
            free(data);
            return 0;
        }
        coop_recovery_item missing = {0};
        missing.id = 81;
        const unsigned char *records = data + start + sizeof(meta) + footer.collection_count * sizeof(coop_powerup_collection);
        for (unsigned i = 0; i < meta.recovery_count; i++) {
            coop_recovery_item item;
            memcpy(&item, records + i * sizeof(item), sizeof(item));
            if (item.id >= missing.id) missing.id = item.id + 1;
        }
        if (!missing.id || missing.id == UINT32_MAX) { free(data); return 1; }
        missing.revision = 1;
        missing.signature = INT_MAX;
        missing.object_index = missing.remote_index = 254;
        missing.network_owner = 0;
        missing.powerup = POW_HOMING_AMMO_4;
        missing.state = COOP_RECOVERY_LIVE;
        missing.gear.missiles[1] = 4;
        memcpy(missing.client_id, meta.active_players[0].client_id, sizeof(missing.client_id));
        memcpy(missing.callsign, meta.active_players[0].callsign, sizeof(missing.callsign));
        meta.recovery_count++;
        memcpy(data + start, &meta, sizeof(meta));
        footer.checksum = coop_save_checksum(data + start, footer.payload_size, 2166136261u);
        if (!bad_counts) {
            footer.checksum = coop_save_checksum(&missing, sizeof(missing), footer.checksum);
            footer.payload_size += sizeof(missing);
        }
        file = fopen(argv[2], "wb");
        if (!file) { free(data); return 1; }
        size_t prefix = (size_t) end - sizeof(footer);
        ok = fwrite(data, 1, prefix, file) == prefix &&
             (bad_counts || fwrite(&missing, sizeof(missing), 1, file) == 1) &&
             fwrite(&footer, sizeof(footer), 1, file) == 1 &&
             fwrite(data + end, 1, (size_t) length - end, file) == (size_t) length - end;
        if (fclose(file)) ok = 0;
        free(data);
        if (ok) printf("Injected missing recovery ID %u, signature %d\n", missing.id, missing.signature);
        return ok ? 0 : 1;
    }
    free(data);
    fprintf(stderr, "No supported cooperative metadata trailer found\n");
    return 1;
}
