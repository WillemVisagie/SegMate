# Compiler settings
CC = gcc
# -Isrc and -I. tell the compiler to look for .h files in src/ and the root directory
CFLAGS = -Wall -Wextra -O3 -fno-common -Isrc -I.

# Directories
SRC_DIR = src
BIN_DIR = bin

# Target executable
TARGET = $(BIN_DIR)/segmate

# Source files and Object files
# Automatically maps src/file.c to bin/file.o
SRCS = $(SRC_DIR)/chess.c $(SRC_DIR)/referee.c $(SRC_DIR)/validation.c $(SRC_DIR)/randommove.c $(SRC_DIR)/fen.c $(SRC_DIR)/search.c $(SRC_DIR)/uci.c $(SRC_DIR)/match.c $(SRC_DIR)/perft.c $(SRC_DIR)/main.c
OBJS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SRCS))

# Default target
all: $(TARGET)

# Link the object files to create the final executable in bin/
$(TARGET): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) -lm

# Pattern rule: Compile ANY .c file in src/ to a .o file in bin/
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c | $(BIN_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Rules to create directories
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Run target: Executes the engine directly in the terminal
run: $(TARGET)
	./$(TARGET)

# Run-UCI target: Launches the engine in UCI mode for chess GUIs
run-uci: $(TARGET)
	./$(TARGET) --uci

# Match target: MCTS vs random-mover self-play
match: $(TARGET)
	./$(TARGET) --match 10

# Clean rule
clean:
	rm -rf $(BIN_DIR)