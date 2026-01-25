server.o: server.c
	gcc -c $< -g

client.o: client.c
	gcc -c $< -g

clients.o: clients.c
	gcc -c $< -g

server: server.o
	gcc -o $@ $^ -g

client: clients.o client.o
	gcc -o $@ $^ -g

clean:
	rm *.o || true
	rm server || true
	rm client || true
