SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj

CC = gcc
CFLAGS = -Wall -Wextra -Wreturn-type -Werror #-fsanitize=address
CPPFLAGS = -I$(SRC_DIR)
#LDFLAGS = -fsanitize=address


include sources.mk
include headers.mk

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
