HDRS := $(SRC_DIR)/definitions.h \
		$(SRC_DIR)/logger.h \
		$(SRC_DIR)/utils/bitwise.h \
		$(SRC_DIR)/utils/string.h \
		$(SRC_DIR)/utils/math.h \
		$(SRC_DIR)/utils/hash.h \
		$(SRC_DIR)/memory/dyn_arena.h \
		$(SRC_DIR)/memory/arena.h \
		$(SRC_DIR)/containers/dict.h \
		$(SRC_DIR)/containers/vector.h \
		$(SRC_DIR)/containers/ring_queue.h \
		$(SRC_DIR)/containers/bytebuffer.h \
		$(SRC_DIR)/containers/object_pool.h \
		$(SRC_DIR)/network/packet.h \
		$(SRC_DIR)/network/decoders.h \
		$(SRC_DIR)/network/utils.h \
		$(SRC_DIR)/network/connection.h \
		$(SRC_DIR)/network/packet_codec.h \
		$(SRC_DIR)/network/network.h \
		$(SRC_DIR)/network/handlers.h \
		$(SRC_DIR)/network/encoders.h \
		$(SRC_DIR)/network/security.h \
		$(SRC_DIR)/network/compression.h \
		$(SRC_DIR)/network/common_types.h \
		$(SRC_DIR)/platform/platform.h \
		$(SRC_DIR)/platform/socket.h \
		$(SRC_DIR)/platform/network.h \
		$(SRC_DIR)/platform/mc_thread.h \
		$(SRC_DIR)/platform/mc_mutex.h \
		$(SRC_DIR)/platform/mc_cond_var.h \
		$(SRC_DIR)/platform/linux/signal-handler.h \
		$(SRC_DIR)/platform/linux/mc_thread_linux.h \
		$(SRC_DIR)/platform/linux/mc_mutex_linux.h \
		$(SRC_DIR)/data/json.h \
		$(SRC_DIR)/data/json/json_internal.h \
		$(SRC_DIR)/data/nbt.h \
		$(SRC_DIR)/data/nbt/nbt_types.h \
		$(SRC_DIR)/registry/registry.h \
		$(SRC_DIR)/resource/resource_id.h \
		$(SRC_DIR)/event/event.h
