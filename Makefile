# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g

# Target executable
TARGET = my_IPC

# Source files
SRCS = my_IPC.c
OBJS = $(SRCS:.c=.o)

# Default target
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

# Rule to build object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(TARGET)

# Run the program with specific configurations
run_config_3_100: $(TARGET)
	./$(TARGET) 3 my_textfile.txt config_3_100.txt

run_config_3_1000: $(TARGET)
	./$(TARGET) 3 my_textfile.txt config_3_1000.txt

run_config_10_10000: $(TARGET)
	./$(TARGET) 10 my_textfile.txt config_10_10000.txt

# Run with valgrind for specific configurations
valgrind_config_3_100: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) 3 my_textfile.txt config_3_100.txt

valgrind_config_3_1000: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) 3 my_textfile.txt config_3_1000.txt

valgrind_config_10_10000: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) 10 my_textfile.txt config_10_10000.txt

# Run the program with custom arguments
run: $(TARGET)
	@echo "Usage: make run ARGS='<max_children> <textfile> <configfile>'"
	./$(TARGET) $(ARGS)

# Run the program with custom arguments through Valgrind
valgrind: $(TARGET)
	@echo "Usage: make valgrind ARGS='<max_children> <textfile> <configfile>'"
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET) $(ARGS)

.PHONY: all clean run_config_3_100 run_config_3_1000 run_config_10_10000 valgrind_config_3_100 valgrind_config_3_1000 valgrind_config_10_10000 run valgrind
