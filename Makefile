CC = gcc
CFLAGS = -Wall -fPIC -I./src
LDFLAGS = -shared
LDLIBS = -lSDL2 -lSDL2_ttf -lpthread -lc

# Директории
SRC_DIR = src
BUILD_DIR = build

# Файлы библиотеки
LIB_SRC = $(SRC_DIR)/treealoc.c $(SRC_DIR)/b_tree.c $(SRC_DIR)/visual.c
LIB_OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRC))
LIB = $(BUILD_DIR)/libtreealoc.so

# Файлы тестового проекта
TEST_SRC = $(SRC_DIR)/test.c
TEST = $(BUILD_DIR)/test

all: $(BUILD_DIR) $(LIB) $(TEST)

# Создание директории build
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Сборка библиотеки
$(LIB): $(LIB_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Сборка тестового проекта
$(TEST): $(TEST_SRC) $(LIB)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) -L$(BUILD_DIR) -ltreealoc -Wl,--wrap=malloc,--wrap=free,--wrap=realloc,--wrap=calloc,-rpath=$(BUILD_DIR)

# Компиляция исходных файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean