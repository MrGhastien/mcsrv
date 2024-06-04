CC = gcc
CFLAGS = -O0 -g -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
#LDFLAGS = -fsanitize=address

SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj

SRCS = 	$(SRC_DIR)/network/connection.c \
		$(SRC_DIR)/network/decoders.c \
		$(SRC_DIR)/network/handlers.c \
		$(SRC_DIR)/network/main.c \
		$(SRC_DIR)/network/packet.c \
		$(SRC_DIR)/network/utils.c

HDRS = 	$(SRC_DIR)/network/decoders.h \
		$(SRC_DIR)/network/definitions.h \
		$(SRC_DIR)/network/handler.h \
		$(SRC_DIR)/network/packet.h \
		$(SRC_DIR)/network/utils.h \
		$(SRC_DIR)/network/connection.h

OBJS = $(patsubst %.c,%.o,$(SRCS))

TARGET = mcsrv

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm $(OBJS)
	rm $(TARGET)
