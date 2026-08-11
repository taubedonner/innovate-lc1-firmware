@echo off
rem ---------------------------------------------------------------------------
rem  LC-1 firmware: сборка в Windows без make.
rem
rem  Нужен avr-gcc в PATH. Подойдёт любой из:
rem    * WinAVR                     (C:\WinAVR-20100110\bin)
rem    * Microchip Studio           (...\toolchain\avr8\avr8-gnu-toolchain\bin)
rem    * avr-gcc с github.com/ZakKemble/avr-gcc-build
rem
rem  Если avr-gcc не в PATH, раскомментируйте строку ниже и поправьте путь.
rem set PATH=C:\WinAVR-20100110\bin;%PATH%
rem ---------------------------------------------------------------------------
setlocal enabledelayedexpansion

set MCU=atmega64
set TARGET=lc1
set CFLAGS=-mmcu=%MCU% -DF_CPU=16000000UL -Os -std=gnu99 -Wall -Wextra -Wno-unused-parameter -Wno-implicit-fallthrough -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -ffunction-sections -fdata-sections -Iinclude -include include/compat_oldavr.h

where avr-gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] avr-gcc not found in PATH. See the comment at the top of build.bat.
    exit /b 1
)

if not exist build mkdir build

set OBJS=
for %%F in (src\*.c) do (
    echo   CC   %%~nxF
    avr-gcc %CFLAGS% -c "%%F" -o "build\%%~nF.o"
    if errorlevel 1 (
        echo [ERROR] failed to compile %%F
        exit /b 1
    )
    set OBJS=!OBJS! build\%%~nF.o
)

echo   LD   %TARGET%.elf
avr-gcc -mmcu=%MCU% -Wl,--gc-sections -Wl,-Map=build\%TARGET%.map !OBJS! -o build\%TARGET%.elf
if errorlevel 1 exit /b 1

echo   HEX  %TARGET%.hex
avr-objcopy -O ihex -R .eeprom -R .fuse -R .lock -R .signature build\%TARGET%.elf build\%TARGET%.hex
avr-objcopy -O ihex -j .eeprom --set-section-flags=.eeprom="alloc,load" --change-section-lma .eeprom=0 build\%TARGET%.elf build\%TARGET%.eep 2>nul

echo.
rem --format=avr есть только в binutils от Atmel/WinAVR; на обычных
rem откатываемся на стандартный вывод.
avr-size --format=avr --mcu=%MCU% build\%TARGET%.elf 2>nul || avr-size build\%TARGET%.elf
echo.
echo Done: build\%TARGET%.hex
echo WARNING: read the "Zagruzchik" section in README before flashing.
endlocal
