TARGET := exflashloader
OUT := build

SRC := main.c startup_stm32h750xx.s system_stm32h7xx.c \
../Src/stm32h7xx_hal_msp.c \
../sfud/src/sfud.c \
../sfud/src/sfud_port.c \
../sfud/src/sfud_sfdp.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_cortex.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_qspi.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_rcc_ex.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_pwr_ex.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal_gpio.c \
../Drivers/STM32H7xx_HAL_Driver/Src/stm32h7xx_hal.c

ARCH := \
	-mcpu=cortex-m7 -mthumb \
	-mfpu=fpv5-d16 -mfloat-abi=hard \

CFLAGS := $(ARCH) \
	-Og -g3 \
	-Wall -fdata-sections -ffunction-sections -nostartfiles \
	-DUSE_HAL_DRIVER \
	-DSTM32H750xx \
	-I../Inc \
	-I../sfud/inc \
	-I../Drivers/STM32H7xx_HAL_Driver/Inc \
	-I../Drivers/CMSIS/Include \
	-I../Drivers/CMSIS/Device/ST/STM32H7xx/Include

LDFLAGS := $(ARCH) \
	--specs=rdimon.specs -nostartfiles \
	-TSTM32H750VBTx_RAM.ld \
	-Wl,-gc-sections \
	-Wl,--print-memory-usage

PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
AS = $(PREFIX)gcc -x assembler-with-cpp
CP = $(PREFIX)objcopy
SZ = $(PREFIX)size

OBJECTS := $(addprefix $(OUT)/, $(notdir $(patsubst %.c, %.o, $(filter %.c, $(SRC))) $(patsubst %.s, %.o, $(filter %.s, $(SRC))) ))

ELF := $(OUT)/$(TARGET).elf

vpath % $(sort $(dir $(SRC)))

all: $(OUT) $(ELF)

$(OUT):
	mkdir $@

$(OUT)/%.o: %.c
	@echo CC $<
	@$(CC) -c $(CFLAGS) -o $@ $<

$(OUT)/%.o: %.s
	@echo AS $<
	@$(CC) -c $(CFLAGS) -o $@ $<

$(ELF) : $(OBJECTS) STM32H750VBTx_RAM.ld
	@echo LD $@
	@$(CC) -o $@ $(LDFLAGS) $(OBJECTS)
	@$(PREFIX)objdump -dxSD $@ > $(OUT)/$(TARGET).lst

run: all
	@echo Running...
	@openocd -f interface/cmsis-dap.cfg -f target/stm32h7x.cfg \
		-c "init" \
		-c "set firmware \"$(ELF)\"" \
		-f do.tcl
