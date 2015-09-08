
CFLAGS = -Wall

PROGRAMS = server client
DOMAIN_SOCK = unix_socket

all: $(PROGRAMS)

server: server.c
	$(CC) $(CFLAGS) $< -o $@

client: client.c
	$(CC) $(CFLAGS) $< -o $@

.PHONY: clean
clean:
	rm -f $(PROGRAMS)
	rm -f $(DOMAIN_SOCK)
