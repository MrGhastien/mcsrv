MAIN_SRC := $(SRC_DIR)/main.c

SRCS := $(SRC_DIR)/logger.c \
		$(SRC_DIR)/logger1.c \
		$(SRC_DIR)/utils/string.c \
		$(SRC_DIR)/utils/str_builder.c \
		$(SRC_DIR)/utils/bitwise.c \
		$(SRC_DIR)/utils/math.c \
		$(SRC_DIR)/utils/hash.c \
		$(SRC_DIR)/utils/iomux.c \
		$(SRC_DIR)/memory/dyn_arena.c \
		$(SRC_DIR)/memory/arena.c \
		$(SRC_DIR)/memory/memory_common.c \
		$(SRC_DIR)/containers/dict-op.c \
		$(SRC_DIR)/containers/dict-init.c \
		$(SRC_DIR)/containers/vector.c \
		$(SRC_DIR)/containers/ring_queue.c \
		$(SRC_DIR)/containers/bytebuffer.c \
		$(SRC_DIR)/containers/object_pool.c \
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
		$(SRC_DIR)/platform/linux/platform_linux.c \
		$(SRC_DIR)/platform/linux/network_linux.c \
		$(SRC_DIR)/platform/linux/time_linux.c \
		$(SRC_DIR)/platform/linux/signal-handler.c \
		$(SRC_DIR)/platform/linux/mc_thread_linux.c \
		$(SRC_DIR)/platform/linux/mc_mutex_linux.c \
		$(SRC_DIR)/platform/linux/mc_cond_var_linux.c \
		$(SRC_DIR)/platform/windows/platform_windows.c \
		$(SRC_DIR)/platform/windows/network_windows.c \
		$(SRC_DIR)/platform/windows/mc_thread_windows.c \
		$(SRC_DIR)/platform/windows/mc_mutex_windows.c \
		$(SRC_DIR)/platform/windows/mc_cond_var_windows.c \
		$(SRC_DIR)/platform/windows/time_windows.c \
		$(SRC_DIR)/data/json/json.c \
		$(SRC_DIR)/data/json/json_parse.c \
		$(SRC_DIR)/data/nbt/nbt.c \
		$(SRC_DIR)/data/nbt/snbt.c \
		$(SRC_DIR)/data/nbt/serial.c \
		$(SRC_DIR)/registry/registry.c \
		$(SRC_DIR)/resource/resource_id.c \
		$(SRC_DIR)/event/event.c

