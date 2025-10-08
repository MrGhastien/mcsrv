#ifndef PLAYER_H
#define PLAYER_H

#include "definitions.h"
#include "network/packet.h"

typedef struct {
    i8 render_distance;
    bool chat_colors;
    u8 skin_part_mask;
    bool text_filtering;
    bool allow_server_listings;
    enum ChatMode chat_mode;
    enum MainHand dominant_hand;
    enum ParticleStatus particle_status;
} PlayerComponent;

#endif /* ! PLAYER_H */
