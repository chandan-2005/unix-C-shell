CC = gcc
CFLAGS = -std=c23 -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 -Wall -Wextra -Werror -Wno-unused-parameter -fno-asm

# Directories
SRC_DIR = src
INC_DIR = include

# Tell GCC to look in the include/ directory for .h files
CFLAGS += -I$(INC_DIR)

# Automatically find all .c files in the src/ directory
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Convert the .c filenames into .o (object) filenames
OBJS = $(SRCS:.c=.o)

# The final executable name required by the assignment
TARGET = shell.out

# Default rule when you just type 'make' or 'make all'
all: $(TARGET)

# Link all the .o files together to create shell.out
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile each .c file into a .o file
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up compiled files (run 'make clean')
clean:
	rm -f $(SRC_DIR)/*.o $(TARGET)