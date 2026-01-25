all: client server

server.o: server.c
	gcc --std=c17 -c $< -g

client.o: client.c
	gcc --std=c17 -c $< -g

clients.o: clients.c
	gcc --std=c17 -c $< -g

server: server.o
	gcc --std=c17 -o $@ $^ -g

client: clients.o client.o
	gcc --std=c17 -o $@ $^ -g

clean:
	rm *.o || true
	rm server || true
	rm client || true
