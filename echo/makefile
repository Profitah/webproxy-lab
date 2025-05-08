CC = gcc
CFLAGS = -Wall -O2 -I../tiny

# 각 실행파일에 필요한 소스 파일 명시
CLIENT_OBJS = client.o echo.o ../tiny/csapp.o
SERVER_OBJS = server.o echo.o ../tiny/csapp.o

all: client server

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS) -pthread

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS) -pthread

# 개별 컴파일 규칙 명시
client.o: client.c
	$(CC) $(CFLAGS) -c -o client.o client.c

server.o: server.c
	$(CC) $(CFLAGS) -c -o server.o server.c

echo.o: echo.c
	$(CC) $(CFLAGS) -c -o echo.o echo.c

../tiny/csapp.o: ../tiny/csapp.c ../tiny/csapp.h
	$(CC) $(CFLAGS) -c -o ../tiny/csapp.o ../tiny/csapp.c

clean:
	rm -f *.o client server ../tiny/csapp.o
