MAIN_SRC := $(SRC_DIR)/main.c

SRCS := $(SRC_DIR)/logger.c \
		$(SRC_DIR)/logger1.c \
		$(SRC_DIR)/utils/string.c \
		$(SRC_DIR)/utils/bitwise.c \
		$(SRC_DIR)/utils/math.c \
		$(SRC_DIR)/utils/hash.c \
		$(SRC_DIR)/memory/dyn_arena.c \
		$(SRC_DIR)/memory/arena.c \
		$(SRC_DIR)/containers/vector-init.c \
		$(SRC_DIR)/containers/dict-op.c \
		$(SRC_DIR)/containers/vector-op.c \
		$(SRC_DIR)/containers/dict-init.c \
		$(SRC_DIR)/containers/ring_queue.c \
		$(SRC_DIR)/containers/bytebuffer.c \
		$(SRC_DIR)/network/handlers.c \
		$(SRC_DIR)/network/network.c \
		$(SRC_DIR)/network/decoders.c \
		$(SRC_DIR)/network/connection.c \
		$(SRC_DIR)/network/receiver.c \
		$(SRC_DIR)/network/utils.c \
		$(SRC_DIR)/network/encoders.c \
		$(SRC_DIR)/network/sender.c \
		$(SRC_DIR)/network/security.c \
		$(SRC_DIR)/network/compression.c \
		$(SRC_DIR)/platform/signal-handler.c \
		$(SRC_DIR)/platform/mc_threads_linux.c \
		$(SRC_DIR)/platform/mc_mutex_linux.c \
		$(SRC_DIR)/platform/mc_cond_var_linux.c \
		$(SRC_DIR)/json/json.c \
		$(SRC_DIR)/json/json_parse.c \
		$(SRC_DIR)/registry/registry.c \
		$(SRC_DIR)/resource/resource_id.c \
		$(SRC_DIR)/event/event.c

