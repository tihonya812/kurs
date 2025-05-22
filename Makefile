CC = gcc
CFLAGS = -Wall -fPIC -I./src
LDFLAGS = -shared
LDLIBS = -lc
VISUAL_LDLIBS = -lSDL2 -lSDL2_ttf -lpthread -lc

# Директории
SRC_DIR = src
BUILD_DIR = build

# Файлы библиотеки (без визуализатора)
LIB_SRC = $(SRC_DIR)/Lib.c $(SRC_DIR)/b_tree.c
LIB_OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(LIB_SRC))
LIB = $(BUILD_DIR)/libtreealoc.so

# Файлы визуализатора
VISUAL_SRC = $(SRC_DIR)/visual.c
VISUAL_OBJ = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(VISUAL_SRC))

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

# Сборка тестового проекта (с визуализатором)
$(TEST): $(TEST_SRC) $(LIB) $(VISUAL_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_SRC) $(VISUAL_OBJ) -L$(BUILD_DIR) -ltreealoc -Wl,--wrap=malloc,--wrap=free,--wrap=realloc,--wrap=calloc,-rpath=$(BUILD_DIR) $(VISUAL_LDLIBS)

# Компиляция исходных файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

install: $(LIB)
	cp $(LIB) /usr/local/lib/
	cp $(SRC_DIR)/Lib.h /usr/local/include/

.PHONY: all clean install