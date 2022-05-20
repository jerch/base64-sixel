.PHONY : clean

CFLAGS = -fPIC
CFLAGS += -std=c99 -O3 -Wall -Wextra -pedantic -march=native
LDFLAGS = -shared

SOURCES=$(shell echo *.c)
HEADERS=$(shell echo *.h)
OBJECTS=$(SOURCES:.c=.o)

SHARED=libbase64sixel.so
STATIC=libbase64sixel.a

all: $(SHARED) $(STATIC)

clean:
		rm -f $(OBJECTS) $(SHARED) $(STATIC)

$(SHARED) : $(OBJECTS)
		$(CC) $(CPPFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

$(STATIC): $(OBJECTS)
		@ar rcs $@ $^
