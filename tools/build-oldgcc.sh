#!/bin/sh
# Сборка компилятором той же эпохи, что и оригинал: avr-gcc 3.4.3 +
# binutils 2.15 (комплектация WinAVR-20050214) в контейнере Debian sarge.
# Это эталонная сборка; avr-gcc 14 нужен только для проверки на чистую
# компиляцию, образ для прибора берут отсюда.
#
#   sh tools/build-oldgcc.sh              обычная сборка (ISR == SIGNAL)
#   LC1_ISR=interrupt sh tools/build-oldgcc.sh    обработчики через INTERRUPT()
#
# Результат в build-oldgcc/: объектники, .elf, .hex, .lst, размеры,
# список векторов прерываний.
#
# Нужен только docker. Образ собирается сам при первом запуске.
set -e
cd "$(dirname "$0")/.."

IMAGE=lc1-oldgcc

EXTRA=""
[ "$LC1_ISR" = "interrupt" ] && EXTRA="-DLC1_OLD_ISR_INTERRUPT"

docker image inspect "$IMAGE" >/dev/null 2>&1 || \
    docker build --platform linux/386 -t "$IMAGE" tools/oldgcc

rm -rf build-oldgcc
mkdir -p build-oldgcc

docker run --rm --platform linux/386 -v "$PWD":/work "$IMAGE" /bin/sh -c "
set -e
CFLAGS='-mmcu=atmega64 -DF_CPU=16000000UL -Os -std=gnu99 -Wall -Iinclude -include include/compat_oldavr.h $EXTRA'
for f in src/*.c; do
    o=build-oldgcc/\$(basename \$f .c).o
    avr-gcc \$CFLAGS -c \$f -o \$o
done
avr-gcc -mmcu=atmega64 build-oldgcc/*.o -o build-oldgcc/lc1-oldgcc.elf
avr-objcopy -O ihex -R .eeprom build-oldgcc/lc1-oldgcc.elf build-oldgcc/lc1-oldgcc.hex
avr-objdump -d build-oldgcc/lc1-oldgcc.elf > build-oldgcc/lc1-oldgcc.lst
avr-size build-oldgcc/lc1-oldgcc.elf | tee build-oldgcc/size.txt
avr-nm build-oldgcc/lc1-oldgcc.elf | grep ' T __vector_' \
    | awk '{print \$3}' | sort > build-oldgcc/vectors.txt
"

echo
echo "--- занятые векторы прерываний ---"
cat build-oldgcc/vectors.txt

if ! diff -u tools/expected-vectors.txt build-oldgcc/vectors.txt; then
    echo "ОШИБКА: набор обработчиков разошёлся с оригинальным образом" >&2
    exit 1
fi

echo
echo "готово: build-oldgcc/"
