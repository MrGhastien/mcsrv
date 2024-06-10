CC = gcc
CFLAGS = -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
#LDFLAGS = -fsanitize=address

SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj

SRCS := $(SRC_DIR)/utils/bitwise.c \
		$(SRC_DIR)/utils/string.c \
		$(SRC_DIR)/memory/arena.c \
		$(SRC_DIR)/memory/dyn_arena.c \
		$(SRC_DIR)/main.c \
		$(SRC_DIR)/network/handlers.c \
		$(SRC_DIR)/network/network.c \
		$(SRC_DIR)/network/packet.c \
		$(SRC_DIR)/network/decoders.c \
		$(SRC_DIR)/network/connection.c \
		$(SRC_DIR)/network/utils.c \
		$(SRC_DIR)/network/encoders.c \
		$(SRC_DIR)/json/json.c \
		$(SRC_DIR)/containers/vector-init.c \
		$(SRC_DIR)/containers/vector-op.c \
		$(SRC_DIR)/containers/dict-init.c \
		$(SRC_DIR)/containers/dict-op.c

HDRS := $(SRC_DIR)/definitions.h \
		$(SRC_DIR)/utils/bitwise.h \
		$(SRC_DIR)/utils/string.h \
		$(SRC_DIR)/memory/arena.h \
		$(SRC_DIR)/memory/dyn_arena.h \
		$(SRC_DIR)/network/decoders.h \
		$(SRC_DIR)/network/encoders.h \
		$(SRC_DIR)/network/handlers.h \
		$(SRC_DIR)/network/packet.h \
		$(SRC_DIR)/network/utils.h \
		$(SRC_DIR)/network/connection.h \
		$(SRC_DIR)/network/network.h \
		$(SRC_DIR)/json/json.h \
		$(SRC_DIR)/containers/dict.h \
		$(SRC_DIR)/containers/vector.h

OBJS = $(patsubst %.c,%.o,$(SRCS))

TARGET = mcsrv

.PHONY: all clean

debug: CFLAGS += -DDEBUG -O0 -g
debug: $(TARGET)

release: CFLAGS += -O2
release: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm $(OBJS)
	rm $(TARGET)
