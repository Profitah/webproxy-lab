# 기본 변수 설정
CC = gcc
CFLAGS = -g -Wall -pthread
OBJS = proxy.o csapp.o

# 기본 타겟: proxy 실행파일 만들기
proxy: $(OBJS)
	$(CC) $(CFLAGS) -o proxy $(OBJS)

# 개별 소스 파일 컴파일
proxy.o: proxy.c csapp.h
	$(CC) $(CFLAGS) -c proxy.c

csapp.o: csapp.c csapp.h
	$(CC) $(CFLAGS) -c csapp.c

# 정리용 명령어
clean:
	rm -f *.o proxy
