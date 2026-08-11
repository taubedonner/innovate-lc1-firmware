# LC-1 firmware: сборка.
#
# Работает и в Unix-шелле, и в Windows (GNU make из WinAVR либо из состава
# Microchip Studio). Если make под рукой нет - есть build.bat.
#
#   make            собрать lc1.elf/.hex/.eep и показать размер
#   make clean      удалить build/
#   make size       размер секций
#   make flash      залить через avrdude (см. переменные PROGRAMMER/PORT)
#   make fuses      показать команду чтения фьюзов
#
# Эталонная сборка делается компилятором 2005 года, см. tools/build-oldgcc.sh.
#
# ВНИМАНИЕ: см. README, раздел "Загрузчик" - полное стирание кристалла
# уничтожит заводской bootloader, которого в этом проекте НЕТ.

MCU        := atmega64
F_CPU      := 16000000UL
TARGET     := lc1

CC         := avr-gcc
OBJCOPY    := avr-objcopy
OBJDUMP    := avr-objdump
SIZE       := avr-size

SRCDIR     := src
INCDIR     := include
BUILDDIR   := build

SRCS       := $(wildcard $(SRCDIR)/*.c)
OBJS       := $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$(SRCS))
DEPS       := $(OBJS:.o=.d)

# -Wno-implicit-fallthrough: в машине состояний оригинал действительно
# проваливается из ветки в ветку, это воспроизведено намеренно.
CFLAGS     := -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu99 \
              -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough \
              -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums \
              -ffunction-sections -fdata-sections \
              -I$(INCDIR) -include $(INCDIR)/compat_oldavr.h -MMD -MP \
              $(EXTRA_CFLAGS)
LDFLAGS    := -mmcu=$(MCU) -Wl,--gc-sections -Wl,-Map=$(BUILDDIR)/$(TARGET).map

# avrdude: подставьте свой программатор и порт
PROGRAMMER := usbasp
PORT       := usb
AVRDUDE    := avrdude -p m64 -c $(PROGRAMMER) -P $(PORT)

.PHONY: all clean size flash flash-eeprom fuses disasm format

all: $(BUILDDIR)/$(TARGET).hex $(BUILDDIR)/$(TARGET).eep size

$(BUILDDIR):
	@mkdir -p $(BUILDDIR) 2>/dev/null || mkdir $(subst /,\,$(BUILDDIR))

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/$(TARGET).elf: $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

$(BUILDDIR)/$(TARGET).hex: $(BUILDDIR)/$(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock -R .signature $< $@

$(BUILDDIR)/$(TARGET).eep: $(BUILDDIR)/$(TARGET).elf
	-$(OBJCOPY) -O ihex -j .eeprom \
	    --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 $< $@

disasm: $(BUILDDIR)/$(TARGET).elf
	$(OBJDUMP) -d -S $< > $(BUILDDIR)/$(TARGET).lst

# --format=avr понимают только binutils от Atmel/WinAVR
size: $(BUILDDIR)/$(TARGET).elf
	@$(SIZE) --format=avr --mcu=$(MCU) $< 2>/dev/null || $(SIZE) $<

flash: $(BUILDDIR)/$(TARGET).hex
	$(AVRDUDE) -U flash:w:$<:i

flash-eeprom: $(BUILDDIR)/$(TARGET).eep
	$(AVRDUDE) -U eeprom:w:$<:i

fuses:
	@echo "Прочитать фьюзы:  $(AVRDUDE) -U lfuse:r:-:h -U hfuse:r:-:h -U efuse:r:-:h"
	@echo "Записывать фьюзы вслепую НЕЛЬЗЯ - см. README, раздел Загрузчик."

format:
	clang-format -i $(SRCDIR)/*.c $(INCDIR)/*.h

clean:
	-rm -rf $(BUILDDIR)

-include $(DEPS)
