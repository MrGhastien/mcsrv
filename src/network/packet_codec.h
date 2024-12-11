/**
 * @file
 *
 * Part of the network subsystem that encodes and sends packets. 
 */

#ifndef PACKET_CODEC_H
#define PACKET_CODEC_H

#include "packet.h"
#include "connection.h"

/**
 * Packet handler function.
 *
 * Processing of packets is done by packet handlers. Most packet handlers
 * trigger events, or send other packets.
 * @param ctx Context information of the network sub-system.
 * @param[in] pkt The packet to handle.
 * @param[in] conn The connection the packet was received on.
 * @return @ref TRUE if the packet was handled successfully, @ref FALSE otherwise.
 */
typedef bool (*pkt_acceptor)(NetworkContext* ctx, const Packet* pkt, Connection* conn);
/**
 * Packet decoder function.
 *
 * Decoder functions are responsible for reading raw bytes received directly from the socket,
 * and initialize and populate the given @ref Packet structure.
 * Decoder funtions do the inverse operation of encoder functions.
 *
 * @param[out] pkt The packet to populate (put data in members).
 * @param arena The arena to make allocations with. Typically used to allocate the packet's payload.
 * @param[in] bytes The buffer containing the packet's raw bytes.
 */
typedef void (*pkt_decoder)(Packet* pkt, Arena* arena, ByteBuffer* bytes);
/**
 * Packet encoder function.
 *
 * Encoder functions are responsible for converting a @ref Packet structure to a stream of raw bytes.
 * Raw bytes are then sent over the network to clients.
 * Encoder funtions do the inverse operation of decoder functions.
 */
typedef void (*pkt_encoder)(const Packet* pkt, ByteBuffer* buffer);

/**
 * Infer a packet handler from the specified packet and connection.
 *
 * The packet handler is inferred from the packet type and the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The inferred packet handler, or NULL if no handler was registered
 * for this packet - connection combination.
 */
pkt_acceptor get_pkt_handler(const Packet* pkt, Connection* conn);
/**
 * Infer a packet decoder from the specified packet and connection.
 *
 * The packet decoder is inferred from the packet type and the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The inferred packet decoder, or NULL if no decoder was registered
 * for this packet - connection combination.
 */
pkt_decoder get_pkt_decoder(const Packet* pkt, Connection* conn);
/**
 * Infer a packet encoder from the specified packet and connection.
 *
 * The packet encoder is inferred from the packet type and the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @return The inferred packet encoder, or @ref NULL if no encoder was registered
 * for this packet - connection combination.
 */
pkt_encoder get_pkt_encoder(const Packet* pkt, Connection* conn);
/**
 * Get a packet type's name.
 *
 * This functions gets the name of the corresponding @ref PacketType enumeration constant.
 *
 * Because several constants have the same value, the name has to be inferred by the packet's
 * type value AND the connection state.
 *
 * @param[in] pkt The packet.
 * @param[in] conn The connection.
 * @param[in] clientbound @ref TRUE if the client-bound name should be returned, @ref FALSE if the
 *                        server-bound name should be taken.
 * @return The name of the packet's type, or NULL if no name was registered
 * for this packet - connection combination.
 */
const char* get_pkt_name(const Packet* pkt, const Connection* conn, bool clientbound);


/**
 * Reads, decodes, and handles a single packet from the given connection.
 *
 * @param[in] conn The connection to read from
 * @return A code indicating the current IO status:
 *         - @ref IOC_OK if a packet was read successfully in its entirety,
 *         - @ref IOC_AGAIN if the receiver could not finish reading a packet without waiting for IO,
 *         - @ref IOC_PENDING if the receiver has started IO operations which could not be completed immediately,
 *         - @ref IOC_ERROR if a reading error occurred,
 *         - @ref IOC_CLOSED if the connection was closed while reading a packet.
 */
enum IOCode receive_packet(NetworkContext* ctx, Connection* conn);

/**
 * Encodes and tries to send a packet immediately.
 *
 * If it is not possible to send all of the packet's bytes without waiting,
 * the remaining bytes are put in the connection's sending queue.
 *
 * Packets are also compressed and/or encrypted if enabled.
 * @param[in] pkt The packet to send.
 * @param[in] conn The connection to send a packet through.
 */
void send_packet(NetworkContext* ctx, const Packet* pkt, Connection* conn);

#endif /* ! PACKET_CODEC_H */
