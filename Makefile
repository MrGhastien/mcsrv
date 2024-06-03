CC = gcc
CFLAGS = -O0 -g

SRC_DIR = ./src
INC_DIR = ./include
OBJ_DIR = ./obj

SRCS = $(wildcard $(SRC_DIR)/*.c)
HDRS = $(wildcard $(INC_DIR)/*.h) $(wildcard $(SRC_DIR)/*.h)
OBJS = $(patsubst %.c,%.o,$(SRCS))

TARGET = mcsrv

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS) $(HDRS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

clean:
	rm $(OBJS)
	rm $(TARGET)
