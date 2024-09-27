#ifndef ENCRYPTION_H
#define ENCRYPTION_H

#include "definitions.h"
#include "memory/arena.h"
#include "json/json.h"

#include <openssl/encoder.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>

typedef struct connection Connection;

typedef struct enc_ctx {
    EVP_PKEY* key_pair;
    EVP_PKEY_CTX* key_ctx;
    u8* encoded_key;
    u64 encoded_key_size;
} EncryptionContext;

typedef struct {
    EVP_CIPHER_CTX* cipher_ctx;
    EVP_CIPHER_CTX* decipher_ctx;
    u8* shared_secret;
} PeerEncryptionContext;

bool encryption_init(EncryptionContext* ctx);
void encryption_cleanup(EncryptionContext* ctx);

u8* encryption_decrypt(EncryptionContext* ctx, Arena* arena, u64* out_size, u8* in, u64 in_size);

bool encryption_init_peer(PeerEncryptionContext* ctx, Arena* arena, u8* shared_secret);
void encryption_cleanup_peer(PeerEncryptionContext* ctx);

i32 encryption_cipher(PeerEncryptionContext* ctx, u8* in, i32 in_size);
i32 encryption_decipher(PeerEncryptionContext* ctx, u8* in, i32 in_size);

bool encryption_authenticate_player(Connection* conn, JSON* json);

#endif /* ! ENCRYPTION_H */
