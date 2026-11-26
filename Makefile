CC       = gcc
CFLAGS   = -Wall -O3
CPPFLAGS =
LDFLAGS  =
LDLIBS   =

SRC      = MacroMix.c
TARGET   = MacroMix

all: $(TARGET)

$(TARGET): $(SRC) Makefile
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
