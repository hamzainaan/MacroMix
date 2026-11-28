CC       = gcc
CFLAGS   = -Wall -O3
SRC      = MacroMix.c
TARGET   = MacroMix

all: $(TARGET)

$(TARGET): $(SRC) Makefile
	$(CC) $(CFLAGS) $(SRC) -o $@ -lm

clean:
	rm -f $(TARGET)

.PHONY: all clean
