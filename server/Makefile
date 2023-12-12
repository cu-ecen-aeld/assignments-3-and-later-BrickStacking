CC ?= gcc
CROSS_COMPILE ?=

CFLAGS=-ggdb -Wall -pthread -lpthread -lrt

# Target and Source File Definitions
TARGET = aesdsocket
SRCS = aesdsocket.c
OBJS = $(SRCS:.c=.o)

# all: aesdsocket.c
# 	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.c
all: $(TARGET)
$(TARGET): $(OBJS)
	$(CROSS_COMPILE)$(CC) -o $@ $^ $(CFLAGS) 

# Compile Each Source File to Object Files
%.o: %.c
	$(CROSS_COMPILE)$(CC) -c -o $@ $< -Wall 


clean: 
	rm -f $(OBJS) $(TARGET)