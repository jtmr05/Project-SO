all: server client

server: bin/sdstored

client: bin/sdstore

obj/auxStructs.o: src/auxStructs.c src/auxStructs.h
	gcc -Wall -g -c src/auxStructs.c -o obj/auxStructs.o

bin/sdstored: obj/sdstored.o obj/auxStructs.o
	gcc -g obj/sdstored.o obj/auxStructs.o -o bin/sdstored

obj/sdstored.o: src/sdstored.c
	gcc -Wall -g -c src/sdstored.c -o obj/sdstored.o

bin/sdstore: obj/sdstore.o
	gcc -g obj/sdstore.o -o bin/sdstore

obj/sdstore.o: src/sdstore.c
	gcc -Wall -g -c src/sdstore.c -o obj/sdstore.o

clean:
	rm obj/* tmp/* bin/*