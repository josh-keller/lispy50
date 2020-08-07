# -*- MakeFile -*-

TARGET = lispy
LIBS = -lm -ledit
CC = gcc
CFLAGS = -g -Wall -MMD -MP -std=c99

default: $(TARGET)

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
DEPS = $(OBJECTS:.o=.d)

-include $(DEPS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@


.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	    $(CC) $(OBJECTS) -Wall $(LIBS) -o $@

.PHONY: clean

clean:
	rm -f *.o
	rm -f $(TARGET)

