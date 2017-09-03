CC = gcc
CFLAGS = -pedantic -Wextra -Wall -g
LDFLAGS = -lpthread
SOURCES = server.c manager.c
OBJECTS = $(SOURCES:.c=.o)
EXECUTABLE = server
all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@
.o:
	$(CC) $(CFLAGS) $< -c $@

clean:
	rm $(EXECUTABLE) *.o 
