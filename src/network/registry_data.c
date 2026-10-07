#include "registry_data.h"
#include "logger.h"
#include "network/connection.h"
#include "packet_codec.h"
#include "registry/registries.h"
#include "registry/registry.h"
#include "resource/resource_id.h"

struct registry_data_serialize_data {
    RegistryDataEntry* entries;
    Arena scratch_arena;
    Arena* persistent_arena;
    i32 idx;
    ResourceID registry_key;
};

static void registry_entry_action(ResourceID* id, const void* entry, void* user_data) {

    struct registry_data_serialize_data* ctx = user_data;

    ctx->entries[ctx->idx] = (RegistryDataEntry){
        .id = *id,
    };

    if (!registry_entry_to_nbt(entry,
                               ctx->registry_key,
                               &ctx->scratch_arena,
                               ctx->persistent_arena,
                               &ctx->entries[ctx->idx].data)) {
        log_errorf("Failed to send entry " RESID_FORMAT " of registry " RESID_FORMAT,
                   RESID_UNWRAP(*id),
                   RESID_UNWRAP(ctx->registry_key));
    }
    log_debugf("Added entry " RESID_FORMAT " of registry " RESID_FORMAT " to send",
               RESID_UNWRAP(*id),
               RESID_UNWRAP(ctx->registry_key));
    ctx->idx++;
}

void send_registry_data(Connection* conn, ResourceID registry_id) {

    PacketRegistryData reg_data_pkt = {
        .registry_id = registry_id,
        .entry_count = registry_count(registry_id),
    };
    reg_data_pkt.entries = arena_allocate(&conn->scratch_arena,
                                          sizeof *reg_data_pkt.entries * reg_data_pkt.entry_count);

    struct registry_data_serialize_data ctx = {
        .entries          = reg_data_pkt.entries,
        .scratch_arena    = conn->scratch_arena,
        .persistent_arena = &conn->persistent_arena,
        .registry_key     = registry_id,
    };

    log_debugf("Sending entries of registry " RESID_FORMAT, RESID_UNWRAP(registry_id));

    registry_foreach(registry_id, &registry_entry_action, &ctx);
    create_send_packet(PKT_CFG_REGISTRY_DATA, &reg_data_pkt, conn);
}

void send_all_registry_data(Connection* conn) {

    send_registry_data(conn, REGISTRY_DIMENSION_TYPE_KEY);
    send_registry_data(conn, REGISTRY_PAINTING_VARIANT_KEY);
    send_registry_data(conn, REGISTRY_DAMAGE_TYPE_KEY);
    send_registry_data(conn, REGISTRY_WOLF_VARIANT_KEY);
}
