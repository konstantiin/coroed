SRC_DIR   = source
BUILD_DIR = build
OBJ_DIR   = $(BUILD_DIR)/obj
BIN_DIR   = $(BUILD_DIR)/bin

CC        = clang
CFLAGS    = -g -O0 -I $(SRC_DIR) -Wall -Werror -fno-omit-frame-pointer -fsanitize=address,leak
LDFLAGS   = $(CFLAGS)

SRCS     := $(wildcard $(SRC_DIR)/*.c) $(wildcard $(SRC_DIR)/*.S)

OBJS     := $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
OBJS     := $(patsubst $(SRC_DIR)/%.S, $(OBJ_DIR)/%.o, $(OBJS))

TARGET    = app

run: $(BIN_DIR)/$(TARGET)
	./$(BIN_DIR)/$(TARGET)

compile: clean $(BIN_DIR)/$(TARGET)

clean:
	rm -rf $(BUILD_DIR)
	rm -rf ./**/*.o

$(BIN_DIR)/$(TARGET): $(BIN_DIR) $(OBJ_DIR) $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.S
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR): $(BUILD_DIR)
	mkdir -p $(OBJ_DIR)

$(BIN_DIR): $(BUILD_DIR)
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

info:
	$(info CC        = $(CC))
	$(info CFLAGS    = $(CFLAGS))
	$(info LDFLAGS   = $(LDFLAGS))
	$(info SRC_DIR   = $(SRC_DIR))
	$(info BUILD_DIR = $(BUILD_DIR))
	$(info SRCS      = $(SRCS))
	$(info OBJS      = $(OBJS))
