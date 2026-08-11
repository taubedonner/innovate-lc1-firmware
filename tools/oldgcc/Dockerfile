# Debian 3.1 "sarge" (2005) - эпоха WinAVR-20050214.
# gcc-avr там 3.4.3, ровно то, чем собиралась оригинальная прошивка LC-1.
#
# Образ 32-битный: на amd64-хосте запускается без эмуляции, на arm64
# (Apple Silicon) - через qemu в составе Docker Desktop.
FROM --platform=linux/386 debian/eol:sarge

RUN echo 'deb http://archive.debian.org/debian/ sarge main' > /etc/apt/sources.list && \
    echo 'Acquire::Check-Valid-Until "false";' > /etc/apt/apt.conf.d/99no-check-valid && \
    apt-get -y --force-yes update && \
    apt-get -y --force-yes install gcc-avr binutils-avr avr-libc make && \
    apt-get clean

WORKDIR /work
