#include "encoders.h"
#include "containers/bytebuffer.h"
#include "containers/vector.h"
#include "data/nbt.h"
#include "logger.h"
#include "packet.h"
#include "platform/platform.h"
#include "utils/bitwise.h"
#include "utils/iomux.h"
#include "utils/str_builder.h"

static void write_string(const string* str, ByteBuffer* buffer) {
    bytebuf_write_varint(buffer, str->length);

    bytebuf_write(buffer, str->base, str->length);
}

static void write_resid(const ResourceID* id, ByteBuffer* buffer) {
    i32 len = id->namespace.length + id->path.length + 1;

    bytebuf_write_varint(buffer, len);
    bytebuf_write(buffer, id->namespace.base, id->namespace.length);
    char c = ':';
    bytebuf_write(buffer, &c, sizeof c);
    bytebuf_write(buffer, id->path.base, id->path.length);
}

static void write_uuid(const u64 uuid[2], ByteBuffer* buffer) {
    u64 tmp[2] = {
        uhton64(uuid[1]),
        uhton64(uuid[0]),
    };

    bytebuf_write(buffer, tmp, sizeof *tmp * 2);
}

static bool write_nbt(const NBT* nbt, ByteBuffer* buffer) {
    IOMux iomux = iomux_wrap_buffer(buffer);
    if (iomux == -1) {
        log_error("Failed to write packet NBT.");
        return FALSE;
    }
    nbt_write(nbt, iomux, TRUE);
    iomux_close(iomux);
    return TRUE;
}

    

    /*
    static void write_u16(u16 num, ByteBuffer* buffer) {
        num = uhton16(num);
        bytebuf_write(buffer, &num, sizeof num);
    }
    */

    DEF_PKT_ENCODER(dummy) {
    (void) pkt;
    (void) buffer;
}

DEF_PKT_ENCODER(status) {
    PacketStatusResponse* payload = pkt->payload;
    write_string(&payload->data, buffer);
}

DEF_PKT_ENCODER(ping) {
    PacketPing* pong = pkt->payload;

    bytebuf_write_i64(buffer, pong->num);
}

DEF_PKT_ENCODER(crypt_request) {
    PacketCryptRequest* payload = pkt->payload;

    write_string(&payload->server_id, buffer);
    bytebuf_write_varint(buffer, payload->pkey_length);
    bytebuf_write(buffer, payload->pkey, payload->pkey_length);
    bytebuf_write_varint(buffer, payload->verify_tok_length);
    bytebuf_write(buffer, payload->verify_tok, payload->verify_tok_length);
    bytebuf_write(buffer, &payload->authenticate, 1);
}

DEF_PKT_ENCODER(compress) {
    PacketSetCompress* payload = pkt->payload;

    bytebuf_write_varint(buffer, payload->threshold);
}

DEF_PKT_ENCODER(login_success) {
    PacketLoginSuccess* payload = pkt->payload;

    write_uuid(payload->uuid, buffer);
    write_string(&payload->username, buffer);
    bytebuf_write_varint(buffer, payload->properties.size);
    for (u32 i = 0; i < payload->properties.size; i++) {
        PlayerProperty* prop = vect_ref(&payload->properties, i);
        write_string(&prop->name, buffer);
        write_string(&prop->value, buffer);
        bytebuf_write(buffer, &prop->is_signed, sizeof(bool));
        if (prop->is_signed)
            write_string(&prop->signature, buffer);
    }

    bytebuf_write(buffer, &payload->strict_errors, sizeof(bool));
}

/* === CONFIGURATION === */

DEF_PKT_ENCODER(cfg_custom) {
    PacketCustom* payload = pkt->payload;

    write_resid(&payload->channel, buffer);

    bytebuf_write(buffer, payload->data, payload->data_length);
}

DEF_PKT_ENCODER(cfg_set_feature_flags) {
    PacketSetFeatureFlags* payload = pkt->payload;

    i32 count = vect_size(&payload->features);

    bytebuf_write_varint(buffer, count);
    for (i32 i = 0; i < count; ++i) {
        ResourceID* id = vect_ref(&payload->features, i);
        write_resid(id, buffer);
    }
}

DEF_PKT_ENCODER(cfg_known_datapacks) {
    PacketKnownDatapacks* payload = pkt->payload;

    i32 count = vect_size(&payload->known_packs);

    bytebuf_write_varint(buffer, count);
    for (i32 i = 0; i < count; ++i) {
        KnownDatapack* pack = vect_ref(&payload->known_packs, i);
        write_string(&pack->namespace, buffer);
        write_string(&pack->id, buffer);
        write_string(&pack->version, buffer);
    }
}
DEF_PKT_ENCODER(cfg_registry_data) {
    PacketRegistryData* payload = pkt->payload;

    write_resid(&payload->registry_id, buffer);
    i32 count = vect_size(&payload->entries);

    bytebuf_write_varint(buffer, count);
    for (i32 i = 0; i < count; ++i) {
        RegistryDataEntry* entry = vect_ref(&payload->entries, i);
        write_resid(&entry->id, buffer);
        bool present = entry->data.arena == NULL;
        bytebuf_write(buffer, &present, sizeof present);
        if (present) {
            IOMux iomux = iomux_wrap_buffer(buffer);
            if (iomux == -1) {
                log_errorf("Failed to encode packet: Unable to serialize NBT: %s",
                           get_last_error());
                return;
            }
            if (nbt_write(&entry->data, iomux, TRUE) != NBTE_OK) {
                log_errorf("Failed to encode packet: Unable to serialize NBT: %s",
                           iomux_error(iomux, NULL));
                return;
            }
            iomux_close(iomux);
        }
    }
}

DEF_PKT_ENCODER(cfg_update_tags) {
    PacketUpdateTags* payload = pkt->payload;

    i32 registry_count = vect_size(&payload->tags);
    bytebuf_write_varint(buffer, registry_count);
    for (i32 i = 0; i < registry_count; i++) {
        TagRegistryData* tag_registry = vect_ref(&payload->tags, i);
        write_resid(&tag_registry->registry_id, buffer);

        i32 entry_count = vect_size(&tag_registry->entries);
        bytebuf_write_varint(buffer, entry_count);
        for (i32 j = 0; j < entry_count; j++) {
            TagEntry* entry = vect_ref(&tag_registry->entries, j);
            write_resid(&entry->id, buffer);

            i32 tag_element_count = vect_size(&entry->elements);
            bytebuf_write_varint(buffer, tag_element_count);
            for (i32 k = 0; k < tag_element_count; k++) {
                bytebuf_write_varint(buffer, tag_element_count);
            }
        }
    }
}

DEF_PKT_ENCODER(cfg_keep_alive) {
    PacketKeepAlive* payload = pkt->payload;

    bytebuf_write_i64(buffer, payload->keep_alive_id);
}

DEF_PKT_ENCODER(cfg_ping) {
    PacketPing* payload = pkt->payload;

    bytebuf_write_i64(buffer, payload->num);
}

DEF_PKT_ENCODER(cfg_disconnect) {
    PacketConfigDisconnect* payload = pkt->payload;

    IOMux iomux = iomux_wrap_buffer(buffer);
    if (iomux == -1) {
        log_error("Failed to write packet NBT.");
        return;
    }
    nbt_write(&payload->reason, iomux, TRUE);
    iomux_close(iomux);
}

DEF_PKT_ENCODER(cfg_add_respack) {
    PacketPushResourcePack* payload = pkt->payload;

    write_uuid(payload->uuid, buffer);
    write_string(&payload->url, buffer);
    write_string(&payload->hash, buffer);
    bytebuf_write(buffer, &payload->forced, sizeof payload->forced);

    bool present = payload->prompt_message.arena != NULL;
    bytebuf_write(buffer, &present, sizeof present);
    if (present) {
    }
}

DEF_PKT_ENCODER(cfg_remove_respack) {
    PacketPopResourcePack* payload = pkt->payload;

    bytebuf_write(buffer, &payload->present, sizeof payload->present);
    if (payload->present)
        write_uuid(payload->uuid, buffer);
}

DEF_PKT_ENCODER(cfg_transfer) {
    PacketTransfer* payload = pkt->payload;

    write_string(&payload->host, buffer);
    bytebuf_write_varint(buffer, payload->port);
}

DEF_PKT_ENCODER(cfg_custom_report) {
    PacketCustomReport* payload = pkt->payload;

    i32 count = vect_size(&payload->details);
    bytebuf_write_varint(buffer, count);

    for (i32 i = 0; i < count; i++) {
        CustomReportDetail* detail = vect_ref(&payload->details, i);
        write_string(&detail->title, buffer);
        write_string(&detail->description, buffer);
    }
}

DEF_PKT_ENCODER(cfg_server_links) {
    PacketServerLinks* payload = pkt->payload;

    i32 count = vect_size(&payload->links);
    for (i32 i = 0; i < count; i++) {
        ServerLink* link = vect_ref(&payload->links, i);

        bytebuf_write(buffer, &link->builtin, sizeof link->builtin);
        if (link->builtin)
            bytebuf_write_varint(buffer, link->label.builtin_type);
        else
            write_nbt(&link->label.custom, buffer);
        write_string(&link->url, buffer);
    }
}
