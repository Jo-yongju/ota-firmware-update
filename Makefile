CC = gcc
CFLAGS = -Wall -I./include
LIBS = -llgpio
SRC_DIR = src
SRCS = $(SRC_DIR)/main.c $(SRC_DIR)/tcp_client.c $(SRC_DIR)/gpio_trigger.c $(SRC_DIR)/spi_master.c
TARGET = ota_main

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LIBS)

clean:
	rm -f $(TARGET)
