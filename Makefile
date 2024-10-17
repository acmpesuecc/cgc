CC = gcc

SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = tests
BUILD_DIR = build

SRC_FILES = $(SRC_DIR)/allocator.c $(SRC_DIR)/gc.c
TEST_FILE = $(TEST_DIR)/tb_gc.c

OBJ_FILES = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRC_FILES)) $(BUILD_DIR)/tb_gc.o

TARGET = $(BUILD_DIR)/tb_gc

CFLAGS = -I$(INCLUDE_DIR) -O0 -g

all: test clean

test: $(BUILD_DIR) $(TARGET) run

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/tb_gc.o: $(TEST_FILE)
	$(CC) $(CFLAGS) -c $(TEST_FILE) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJ_FILES)
	$(CC) $(OBJ_FILES) -o $@

run: $(TARGET)
	@$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean

