CFLAGS = -g -std=c99

# kompilerer alle delene av programmene
all: client server

%:%c
	gcc $(CFLAGS) $^ -o $@

# brukes til å kompliere og kjøre programmene
valc: client
	valgrind --leak-check=full ./client 127.0.0.1 4041 list_of_filenames.txt 20

vals: server
	valgrind --leak-check=full ./server 4041 mini_set server_out.txt

# kjører programmene normalt
rc: client
	./client 127.0.0.1 4041 list_of_filenames.txt 20

rs: server
	./server 4041 big_set server_out.txt



server: send_packet.o pgmread.o readfiles.o package.o server.o
	gcc send_packet.o pgmread.o readfiles.o package.o server.o -o $@

client: LList.o send_packet.o pgmread.o readfiles.o package.o client.o
	gcc LList.o send_packet.o pgmread.o readfiles.o package.o client.o -o $@


# kompilerer objektfilene som brukes i server og klient
LList.o: LList.c
	gcc -c $(CFLAGS) $^ -o $@

send_packet.o: send_packet.c
	gcc -c -g -std=gnu99 $^ -o $@

pgmread.o: pgmread.c
	gcc -c -g -std=gnu99 $^ -o $@

readfiles.o: readfiles.c
	gcc -c $(CFLAGS) $^ -o $@

package.o: package.c
	gcc -c $(CFLAGS) $^ -o $@

client.o: client.c
	gcc -c $(CFLAGS) $^ -o $@

server.o: server.c
	gcc -c $(CFLAGS) $^ -o $@


# sletter alle objekt- og eksekverbare filer
clean:
	rm -f server.o server
	rm -f client.o client
	rm -f readfiles.o
	rm -f package.o
	rm -f LList.o
	rm -f pgmread.o send_packet.o
