all:run

main.o: main.c
	gcc -c $< -g

server.o: server.c
	gcc -c $< -g

client.o: client.c
	gcc -c $< -g 

clients.o: clients.c
	gcc -c $< -g

packet.o: packet.c
	gcc -c $< -g -Wpadded
	
udp_forwarder: main.o packet.o client.o clients.o server.o
	gcc -o $@ $^ -g

run: udp_forwarder
	./udp_forwarder > coso.txt

server: server.o
	gcc -o $@ $^ -g

client: clients.o client.o
	gcc -o $@ $^ -g

clean:
	rm *.o || true
	rm udp_forwarder || true
	rm server || true
	rm client || true
	rm *.log || true
