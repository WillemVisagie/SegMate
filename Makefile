# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -O3

# Directories
BIN_DIR = bin

TARGET = $(BIN_DIR)/segmate
OBJS = $(BIN_DIR)/chess.o $(BIN_DIR)/referee.o $(BIN_DIR)/validation.o $(BIN_DIR)/randommove.o

# Default target
all: $(TARGET)

# Link the object files to create the final executable in bin/
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# Compile C files directly into the bin/ folder
$(BIN_DIR)/chess.o: chess.c chess.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c chess.c -o $@

$(BIN_DIR)/validation.o: validation.c validation.h chess.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c validation.c -o $@

$(BIN_DIR)/randommove.o: randommove.c randommove.h validation.h chess.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c randommove.c -o $@

$(BIN_DIR)/referee.o: referee.c referee.h chess.h validation.h randommove.h | $(BIN_DIR)
	$(CC) $(CFLAGS) -c referee.c -o $@

# Rules to create directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Run target: Executes the engine directly in the terminal
run: $(TARGET)
	./$(TARGET)

# Clean rule
clean:
	rm -rf $(BIN_DIR)