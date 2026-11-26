NAME     = MacroMix
VERSION  = 1.4
AUTHOR   = CMK

CC       = gcc
CFLAGS   = -Wall -O3
CPPFLAGS =
LDFLAGS  =
LDLIBS   =

SRC      = MacroMix.c
TARGET   = MacroMix

DEFINES  = -DENGINE_NAME='"$(NAME)"' \
           $(if $(VERSION),-DENGINE_VERSION='"$(VERSION)"') \
           -DENGINE_AUTHOR='"$(AUTHOR)"'

all: $(TARGET)

$(TARGET): $(SRC) Makefile
	$(CC) $(CPPFLAGS) $(DEFINES) $(CFLAGS) $(SRC) -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
