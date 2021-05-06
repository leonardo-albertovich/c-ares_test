AR      = ar
AC      = as
CC      = gcc
LINK    = gcc
NM      = nm
OBJDUMP = objdump

PYTHON  = python

CFLAGS  = -g -fPIC -D_REENTRANT -D_GNU_SOURCE -I.
LFLAGS  = -fPIC -L.
LIBS    = -lcares

SOURCES = ares_test.c
OBJECTS = ares_test.o
TARGET  = ares_test

.SUFFIXES: .c .s

.c.o:
	$(CC) -c $(CFLAGS) $(INCPATH) $<

.s.o:
	$(AC) $(AFLAGS) $<

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LINK) $(LFLAGS) -o $(TARGET) $(OBJECTS) $(LIBS)

clean:
	rm -f $(TARGET) $(OBJECTS)

test:
	./$(TARGET)