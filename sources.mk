MAIN_SRC := $(SRC_DIR)/main.c

SRCS := $(SRC_DIR)/logger1.c \
		$(SRC_DIR)/world/data/blockstate.c \
		$(SRC_DIR)/world/data/level.c \
		$(SRC_DIR)/world/data/entity/entity.c \
		$(SRC_DIR)/world/simulation/simulation.c \
		$(SRC_DIR)/logger.c \
		$(SRC_DIR)/network/network.c \
		$(SRC_DIR)/network/compression.c \
		$(SRC_DIR)/network/connection.c \
		$(SRC_DIR)/network/decoders.c \
		$(SRC_DIR)/network/encoders.c \
		$(SRC_DIR)/network/handlers.c \
		$(SRC_DIR)/network/security.c \
		$(SRC_DIR)/network/utils.c \
		$(SRC_DIR)/network/receiver.c \
		$(SRC_DIR)/network/sender.c \
		$(SRC_DIR)/memory/memory_common.c \
		$(SRC_DIR)/memory/allocators/arena.c \
		$(SRC_DIR)/memory/allocators/buddy.c \
		$(SRC_DIR)/memory/allocators/pool.c \
		$(SRC_DIR)/memory/stats/basic_pool.c \
		$(SRC_DIR)/utils/string.c \
		$(SRC_DIR)/utils/bitwise.c \
		$(SRC_DIR)/utils/hash.c \
		$(SRC_DIR)/utils/math.c \
		$(SRC_DIR)/utils/position.c \
		$(SRC_DIR)/utils/str_builder.c \
		$(SRC_DIR)/utils/iomux.c \
		$(SRC_DIR)/containers/dict-op.c \
		$(SRC_DIR)/containers/vector.c \
		$(SRC_DIR)/containers/bytebuffer.c \
		$(SRC_DIR)/containers/dict-init.c \
		$(SRC_DIR)/containers/ring_queue.c \
		$(SRC_DIR)/event/event.c \
		$(SRC_DIR)/registry/blocks.c \
		$(SRC_DIR)/registry/registry.c \
		$(SRC_DIR)/resource/resource_id.c \
		$(SRC_DIR)/data/json/json.c \
		$(SRC_DIR)/data/json/serial.c \
		$(SRC_DIR)/data/nbt/nbt.c \
		$(SRC_DIR)/data/nbt/serial.c \
		$(SRC_DIR)/data/nbt/snbt.c

# $(SRC_DIR)/containers/object_pool.c
# $(SRC_DIR)/memory/allocators/buddy.c
