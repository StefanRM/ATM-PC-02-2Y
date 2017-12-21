# Protocoale de comunicatii
# Tema 2
# ATM
# Makefile

CFLAGS = -Wall -g

# Server's listening port
PORT = 17548

# Server's IP address
IP_SERVER = "127.0.1.0"

# Data File with information about users
DATA_FILE = "users_data_file"

all: build

build: server client

# Compile server.c
server: server.c

# Compile client.c
client: client.c

.PHONY: clean run_server run_client

# Run server
run_server:
	./server ${PORT} ${DATA_FILE}

# Run client 	
run_client:
	./client ${IP_SERVER} ${PORT}

clean:
	rm -f server client

clean_logs:
	rm *.log