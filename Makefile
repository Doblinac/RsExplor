# 编译器设置
CC = clang
CFLAGS = -Wall -Wextra -pedantic -std=c11 -g -I./src
LDFLAGS = -lncurses -lpanel

# 目录设置
SRC_DIR = src
BUILD_DIR = build
TARGET = $(BUILD_DIR)/rsexplor

# 源文件列表
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))

# 默认目标
.PHONY: all
all: $(BUILD_DIR) $(TARGET)

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 链接可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# 通用编译规则
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# 运行
.PHONY: run
run: $(TARGET)
	./$(TARGET)
	
# ... 你已有内容 ...

COMMON_H = $(SRC_DIR)/common.h

.PHONY: check_unused

check_unused:
	$(CC) $(CFLAGS) -Wunused -fsyntax-only $(SRCS) 2>&1 | tee unused_warnings.log
	bash move_unused_macros.sh $(COMMON_H) $(SRC_DIR)