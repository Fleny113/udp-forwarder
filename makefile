FLAGS = --std=c17 -g

ifeq (${DEBUG},1)
FLAGS += -DDEBUG
endif

all: client server

server.o: server.c
	gcc $(FLAGS) -c $<

client.o: client.c
	gcc $(FLAGS) -c $<

clients.o: clients.c
	gcc $(FLAGS) -c $<

server: server.o
	gcc $(FLAGS) -o $@ $^

client: clients.o client.o
	gcc $(FLAGS) -o $@ $^

clean:
	rm *.o || true
	rm server || true
	rm client || true
