# ============================================================
#  Exynos4412 裸机工程 Makefile (STM32 库风格)
#
#  依赖: arm-none-eabi-gcc (GNU Arm Embedded Toolchain)
#
#  用法:
#    make            -> 生成 BL2 + 主程序两个镜像
#    make clean
#
#  产出:
#    build/bl2.bin   -> BL2 (IRAM 0x02023400 运行, <14KB, 含时钟+DDR 初始化+搬运)
#    build/main.bin  -> 主程序 (DDR 0x43E00000 运行, LCD/串口等全部业务代码)
#
#  烧录: 见 tools/burn_sd.ps1 (BL1 + BL2 + main 一起写入 SD 卡)
# ============================================================

CROSS_COMPILE ?= arm-none-eabi-

CC      := $(CROSS_COMPILE)gcc
LD      := $(CROSS_COMPILE)ld
OBJCOPY := $(CROSS_COMPILE)objcopy
OBJDUMP := $(CROSS_COMPILE)objdump
SIZE    := $(CROSS_COMPILE)size

BUILD_DIR := build

CFLAGS  := -mcpu=cortex-a9 -marm -mfloat-abi=soft \
           -O2 -g -Wall -Wextra \
           -ffreestanding -fno-builtin -fno-common \
           -nostdlib -nostartfiles \
           -I Libraries/inc -I User -I User/App
ASFLAGS := -mcpu=cortex-a9 -marm -g

# ---------------- BL2: IRAM 0x02023400 (只做初始化+搬运) ----------------
BL2_LDS  := startup/exynos4412.lds
BL2_SRCS := \
    startup/start.S \
    startup/aeabi_div.S \
    User/App/system_4412.c \
    Libraries/src/exynos4412_clock.c \
    Libraries/src/exynos4412_ddr.c \
    Libraries/src/exynos4412_gpio.c \
    Libraries/src/exynos4412_uart.c

BL2_OBJS := $(addprefix $(BUILD_DIR)/bl2/,$(filter %.o,$(BL2_SRCS:%.c=%.o) $(BL2_SRCS:%.S=%.o)))
BL2_ELF  := $(BUILD_DIR)/bl2.elf
BL2_BIN  := $(BUILD_DIR)/bl2.bin

# ---------------- MAIN: DDR 0x43E00000 (全部业务代码) ----------------
MAIN_LDS  := startup/main.lds
MAIN_SRCS := \
    startup/main_start.S \
    startup/aeabi_div.S \
    User/main.c \
    User/App/led.c \
    User/App/panel.c \
    User/App/debug.c \
    Libraries/src/exynos4412_lcd.c \
    Libraries/src/exynos4412_buzzer.c \
    Libraries/src/exynos4412_key.c \
    Libraries/src/exynos4412_clock.c \
    Libraries/src/exynos4412_gpio.c \
    Libraries/src/exynos4412_uart.c

MAIN_OBJS := $(addprefix $(BUILD_DIR)/main/,$(filter %.o,$(MAIN_SRCS:%.c=%.o) $(MAIN_SRCS:%.S=%.o)))
MAIN_ELF  := $(BUILD_DIR)/main.elf
MAIN_BIN  := $(BUILD_DIR)/main.bin

all: $(BL2_BIN) $(MAIN_BIN) dis size

$(BUILD_DIR)/bl2/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DEXYNOS4412_BOOT_SD -c -o $@ $<

$(BUILD_DIR)/bl2/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -DEXYNOS4412_BOOT_SD -c -o $@ $<

$(BUILD_DIR)/main/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILD_DIR)/main/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c -o $@ $<

$(BL2_ELF): $(BL2_OBJS)
	@mkdir -p $(dir $@)
	$(LD) -T $(BL2_LDS) --defsym __LINK_BASE=0x02023400 \
	      -Map $(BUILD_DIR)/bl2.map -o $@ $(BL2_OBJS)

$(BL2_BIN): $(BL2_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "BL2 size: $$(stat -c%s $@) bytes (limit 14332)"

$(MAIN_ELF): $(MAIN_OBJS)
	@mkdir -p $(dir $@)
	$(LD) -T $(MAIN_LDS) -Map $(BUILD_DIR)/main.map -o $@ $(MAIN_OBJS)

$(MAIN_BIN): $(MAIN_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "MAIN size: $$(stat -c%s $@) bytes (limit 524288)"

dis: $(BL2_ELF) $(MAIN_ELF)
	$(OBJDUMP) -D $(BL2_ELF) > $(BUILD_DIR)/bl2.dis
	$(OBJDUMP) -D $(MAIN_ELF) > $(BUILD_DIR)/main.dis

size: $(BL2_ELF) $(MAIN_ELF)
	$(SIZE) $(BL2_ELF) $(MAIN_ELF)

.PHONY: all clean dis size

clean:
	rm -rf $(BUILD_DIR)
