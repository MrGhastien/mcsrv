#include "encryption.h"
#include "logger.h"
#include "memory/arena.h"

#include <openssl/crypto.h>
#include <openssl/encoder.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <string.h>

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

    out = arena_callocate(arena, *out_size);

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

    ctx->shared_secret = arena_allocate(arena, SHARED_SECRET_SIZE);
    memcpy(ctx->shared_secret, shared_secret, SHARED_SECRET_SIZE);

    if (EVP_EncryptInit_ex(
            ctx->cipher_ctx, EVP_aes_128_cfb8(), NULL, ctx->shared_secret, ctx->shared_secret) <=
        0) {
        encryption_get_errors();
        log_error("Failed to initialize symmetric encryption context.");
        return FALSE;
    }
    if(EVP_CIPHER_CTX_set_key_length(ctx->cipher_ctx, 16) <= 0) {
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
    if(EVP_CIPHER_CTX_set_key_length(ctx->decipher_ctx, 16) <= 0) {
        encryption_get_errors();
        return FALSE;
    }

    return TRUE;
}

void encryption_cleanup_peer(PeerEncryptionContext* ctx) {
    EVP_CIPHER_CTX_free(ctx->cipher_ctx);
    EVP_CIPHER_CTX_free(ctx->decipher_ctx);
}

i32 encryption_cipher(PeerEncryptionContext* ctx, u8* in, i32 in_size) {

    if (!EVP_EncryptUpdate(ctx->cipher_ctx, in, &in_size, in, in_size)) {
        encryption_get_errors();
        log_error("Could not encrypt data.");
        return -1;
    }

    return in_size;
}

i32 encryption_decipher(PeerEncryptionContext* ctx, u8* in, i32 in_size) {

    if (!EVP_DecryptUpdate(ctx->decipher_ctx, in, &in_size, in, in_size)) {
        encryption_get_errors();
        log_error("Could not decrypt data.");
        return -1;
    }

    return in_size;
}
