all: client

clean:
	rm -f *.o gpiots_client gpiots_client_safe

client: gpiots_client.c gpiots_client_safe.c
	$(CC) -o gpiots_client gpiots_client.c
	$(CC) -o gpiots_client_safe gpiots_client_safe.c
