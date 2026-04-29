ifeq ($(OS),Windows_NT)
    TARGET = ubot.exe
    LIBS = -lws2_32
    CLEAN_CMD = del /Q $(TARGET) *.o
else
    TARGET = ubot
    LIBS = 
    CLEAN_CMD = rm -f $(TARGET) *.o
    UNAME_S := $(shell uname -s)
endif

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O3 -march=native

all: $(TARGET)

$(TARGET): main.c uirc.h
	$(CC) $(CFLAGS) main.c -o $(TARGET) $(LIBS)

clean:
	$(CLEAN_CMD)

run: $(TARGET)
	./$(TARGET)