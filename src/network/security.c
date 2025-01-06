#include "security.h"
#include "data/json.h"
#include "containers/bytebuffer.h"
#include "logger.h"
#include "memory/arena.h"
#include "memory/mem_tags.h"
#include "network/connection.h"
#include "utils/string.h"

#include <curl/curl.h>
#include <openssl/crypto.h>
#include <openssl/encoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SHARED_SECRET_SIZE 16

static void encryption_get_errors(void) {
    u64 code;
    char buf[512];
    while ((code = ERR_get_error())) {
        ERR_error_string_n(code, buf, 512);
        log_errorf("OpenSSL: %s.", buf);
    }
}

bool encryption_init(EncryptionContext* ctx) {
    EVP_PKEY_CTX* keygen_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!keygen_ctx) {
        log_fatal("Failed to create the RSA key context.");
        return FALSE;
    }

    if (EVP_PKEY_keygen_init(keygen_ctx) <= 0) {
        log_fatal("Failed to initialize the RSA key context for key generation.");
        return FALSE;
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(keygen_ctx, 1024) <= 0) {
        log_fatal("Failed to set the RSA key length to 1024 bits.");
        return FALSE;
    }

    if (EVP_PKEY_keygen(keygen_ctx, &ctx->key_pair) <= 0) {
        log_fatal("Failed to generate the RSA key pair.");
        return FALSE;
    }

    OSSL_ENCODER_CTX* encoder_ctx = OSSL_ENCODER_CTX_new_for_pkey(
        ctx->key_pair,
        OSSL_KEYMGMT_SELECT_PUBLIC_KEY | OSSL_KEYMGMT_SELECT_DOMAIN_PARAMETERS,
        "DER",
        "SubjectPublicKeyInfo",
        NULL);

    if (!encoder_ctx) {
        log_fatal("Could not create an OpenSSL encoder context: No suitable encoder found.");
        return FALSE;
    }

    if (OSSL_ENCODER_to_data(encoder_ctx, &ctx->encoded_key, &ctx->encoded_key_size) <= 0) {
        log_fatal("Could not encode the generated RSA key pair to DER.");
        return FALSE;
    }

    EVP_PKEY_CTX_free(keygen_ctx);
    OSSL_ENCODER_CTX_free(encoder_ctx);

    ctx->key_ctx = EVP_PKEY_CTX_new(ctx->key_pair, NULL);
    if (!ctx->key_ctx) {
        return FALSE;
    }

    if (EVP_PKEY_decrypt_init(ctx->key_ctx) <= 0) {
        return FALSE;
    }

    if (EVP_PKEY_CTX_set_rsa_padding(ctx->key_ctx, RSA_PKCS1_PADDING) <= 0) {
        return FALSE;
    }

    return TRUE;
}

void encryption_cleanup(EncryptionContext* ctx) {
    EVP_PKEY_CTX_free(ctx->key_ctx);
    EVP_PKEY_free(ctx->key_pair);
    OPENSSL_free(ctx->encoded_key);
}

u8* encryption_decrypt(EncryptionContext* ctx, Arena* arena, u64* out_size, u8* in, u64 in_size) {
    u8* out;

    if (EVP_PKEY_decrypt(ctx->key_ctx, NULL, out_size, in, in_size) <= 0) {
        encryption_get_errors();
        log_error("Could not determine the decrypted buffer size.");
        return NULL;
    }

    out = arena_callocate(arena, *out_size, MEM_TAG_NETWORK);

    if (EVP_PKEY_decrypt(ctx->key_ctx, out, out_size, in, in_size) <= 0) {
        encryption_get_errors();
        log_error("Could not decrypt the given buffer.");
        return NULL;
    }

    return out;
}

bool encryption_init_peer(PeerEncryptionContext* ctx, Arena* arena, u8* shared_secret) {
    ctx->cipher_ctx = EVP_CIPHER_CTX_new();
    ctx->decipher_ctx = EVP_CIPHER_CTX_new();

    ctx->shared_secret = arena_allocate(arena, SHARED_SECRET_SIZE, MEM_TAG_NETWORK);
    memcpy(ctx->shared_secret, shared_secret, SHARED_SECRET_SIZE);

    if (EVP_EncryptInit_ex(
            ctx->cipher_ctx, EVP_aes_128_cfb8(), NULL, ctx->shared_secret, ctx->shared_secret) <=
        0) {
        encryption_get_errors();
        log_error("Failed to initialize symmetric encryption context.");
        return FALSE;
    }
    if (EVP_CIPHER_CTX_set_key_length(ctx->cipher_ctx, 16) <= 0) {
        encryption_get_errors();
        return FALSE;
    }

    if (EVP_DecryptInit_ex(
            ctx->decipher_ctx, EVP_aes_128_cfb8(), NULL, ctx->shared_secret, ctx->shared_secret) <=
        0) {
        encryption_get_errors();
        log_error("Failed to initialize symmetric decryption context.");
        return FALSE;
    }
    if (EVP_CIPHER_CTX_set_key_length(ctx->decipher_ctx, 16) <= 0) {
        encryption_get_errors();
        return FALSE;
    }

    return TRUE;
}

void encryption_cleanup_peer(PeerEncryptionContext* ctx) {
    EVP_CIPHER_CTX_free(ctx->cipher_ctx);
    EVP_CIPHER_CTX_free(ctx->decipher_ctx);
}

bool encryption_cipher(PeerEncryptionContext* ctx, ByteBuffer* buffer, u64 offset) {

    u64 region_count = 2;
    BufferRegion regions[2];
    bytebuf_get_read_regions(buffer, regions, &region_count, offset);

    for (u64 i = 0; i < region_count; i++) {
        BufferRegion* reg = &regions[i];
        i32 reg_new_size;
        if (!EVP_EncryptUpdate(ctx->cipher_ctx, reg->start, &reg_new_size, reg->start, reg->size)) {
            encryption_get_errors();
            log_error("Could not encrypt data.");
            return FALSE;
        }
        if ((u64) reg_new_size != reg->size) {
            log_fatalf("Encryption buffer size mismatch: %zu -> %i", reg->size, reg_new_size);
            abort();
        }
    }

    return TRUE;
}

bool encryption_decipher(PeerEncryptionContext* ctx, ByteBuffer* buffer, u64 offset) {

    u64 region_count = 2;
    BufferRegion regions[2];
    bytebuf_get_read_regions(buffer, regions, &region_count, offset);

    for (u64 i = 0; i < region_count; i++) {
        BufferRegion* reg = &regions[i];
        i32 reg_new_size;
        if (!EVP_DecryptUpdate(ctx->decipher_ctx, reg->start, &reg_new_size, reg->start, reg->size)) {
            encryption_get_errors();
            log_error("Could not decrypt data.");
            return FALSE;
        }
        if ((u64) reg_new_size != reg->size) {
            log_fatalf("Encryption buffer size mismatch: %zu -> %i", reg->size, reg_new_size);
            abort();
        }
    }

    return TRUE;
}

static void hash_to_string(u8* hash, u32 hash_size, Arena* arena, string* out) {
    bool negative = hash[0] >> 7;
    u32 offset = 0;
    if (negative) {
        u16 carry = 1;

        for (i32 i = hash_size - 1; i >= 0; i--) {
            u16 tmp = (u8) ~hash[i];
            tmp += carry;
            hash[i] = tmp & 0xff;
            carry = tmp >> 8;
        }
        *out = str_alloc(hash_size * 2 + 2, arena);
        out->base[0] = '-';
        offset = 1;
    } else {
        *out = str_alloc(hash_size * 2 + 1, arena);
    }

    for (u32 i = 0; i < hash_size; i++) {
        char tmp[3];
        snprintf(tmp, 3, "%02hhx", hash[i]);

        out->base[i * 2 + offset] = tmp[0];
        out->base[i * 2 + offset + 1] = tmp[1];
    }
}

static string
encryption_hash(Arena* arena, EncryptionContext* global_ctx, PeerEncryptionContext* peer_ctx) {
    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();

    string out = {0};

    if (EVP_DigestInit_ex(md_ctx, EVP_sha1(), NULL) <= 0) {
        encryption_get_errors();
        log_error("Could not initialize the digest context.");
        return out;
    }

    // Don't digest the server ID, as it is empty

    if (EVP_DigestUpdate(md_ctx, peer_ctx->shared_secret, 16) <= 0) {
        encryption_get_errors();
        log_error("Could not create the login hash.");
        return out;
    }

    if (EVP_DigestUpdate(md_ctx, global_ctx->encoded_key, global_ctx->encoded_key_size) <= 0) {
        encryption_get_errors();
        log_error("Could not create the login hash.");
        return out;
    }

    u8 buf[EVP_MAX_MD_SIZE];
    u32 size = EVP_MAX_MD_SIZE;
    if (EVP_DigestFinal_ex(md_ctx, buf, &size) <= 0) {
        encryption_get_errors();
        log_error("Could not generate the login hash.");
        return out;
    }

    hash_to_string(buf, size, arena, &out);

    EVP_MD_CTX_destroy(md_ctx);

    return out;
}

static u64 write_data_callback(void* ptr, u64 size, u64 nmemb, ByteBuffer* buffer) {
    u64 total = size * nmemb;
    bytebuf_write(buffer, ptr, total);
    return total;
}

bool encryption_authenticate_player(Connection* conn, JSON* json) {
    Arena scratch = conn->scratch_arena;
    string hash = encryption_hash(&scratch, conn->global_enc_ctx, &conn->peer_enc_ctx);
    log_infof("Hash: %s", hash.base);

    CURL* curl = curl_easy_init();
    if (!curl) {
        log_error("Failed to initialize libcurl.");
        return FALSE;
    }

    ByteBuffer buffer = bytebuf_create_fixed(8192, &scratch);
    char url[2048];
    char* error_buffer = arena_callocate(&scratch, CURL_ERROR_SIZE, MEM_TAG_NETWORK);
    snprintf(url,
             2048,
             "https://sessionserver.mojang.com/session/minecraft/"
             "hasJoined?serverId=%s&username=%s",
             hash.base,
             conn->player_name.base);

    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "curl/8.8.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_data_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);

    log_tracef("URL: %s", url);
    i64 res_code;
    CURLcode curl_code = curl_easy_perform(curl);
    if (curl_code != CURLE_OK) {
        log_errorf("Curl: %s.", error_buffer);
    }

    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &res_code);
    curl_easy_cleanup(curl);

    if (res_code != 200) {
        log_errorf("Failed request to sessionserver.mojang.com: %li", res_code);
    }

    bytebuf_write_varint(&buffer, 0);

    *json = json_parse(&buffer, &scratch);
    if (json->arena == NULL)
        return FALSE;

#ifdef TRACE
    string str;
    json_stringify(json, &str, 2048, &scratch);
    log_tracef("%s", str.base);
#endif

    return res_code == 200;
}
