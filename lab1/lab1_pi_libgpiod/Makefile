# Raspberry Pi 4 (aarch64) için. Pi'nin kendisinde derlemek için: make CROSS=
CROSS  ?= aarch64-linux-gnu-
CC      = $(CROSS)gcc
CFLAGS  = -O2 -Wall -Wextra -std=gnu11
LDLIBS  = -lgpiod -lm

pi_gpio: pi_gpio.c
	$(CC) $(CFLAGS) $< -o $@ $(LDLIBS)

temizle:
	rm -f pi_gpio

.PHONY: temizle
