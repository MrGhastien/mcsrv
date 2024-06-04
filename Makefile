CC = gcc
CFLAGS = -O0 -g -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
#LDFLAGS = -fsanitize=address

SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj

SRCS := $(SRC_DIR)/utils/bitwise.c \
		$(SRC_DIR)/memory/arena.c \
		$(SRC_DIR)/main.c \
		$(SRC_DIR)/network/handlers.c \
		$(SRC_DIR)/network/network.c \
		$(SRC_DIR)/network/packet.c \
		$(SRC_DIR)/network/decoders.c \
		$(SRC_DIR)/network/connection.c \
		$(SRC_DIR)/network/utils.c \
		$(SRC_DIR)/network/encoders.c

HDRS := $(SRC_DIR)/definitions.h \
		$(SRC_DIR)/utils/bitwise.h \
		$(SRC_DIR)/memory/arena.h \
		$(SRC_DIR)/network/decoders.h \
		$(SRC_DIR)/network/encoders.h \
		$(SRC_DIR)/network/handlers.h \
		$(SRC_DIR)/network/packet.h \
		$(SRC_DIR)/network/utils.h \
		$(SRC_DIR)/network/connection.h \
		$(SRC_DIR)/network/network.h

OBJS = $(patsubst %.c,%.o,$(SRCS))

TARGET = mcsrv

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm $(OBJS)
	rm $(TARGET)
