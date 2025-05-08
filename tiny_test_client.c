#include "csapp.h"

int main(int argc, char **argv) {
    int clientfd;
    char *host, *port;

    if (argc != 3) {
        fprintf(stderr, "사용법: %s <호스트명> <포트번호>\n", argv[0]);
        exit(1);
    }

    host = argv[1];
    port = argv[2];

    clientfd = Open_clientfd(host, port);  // 서버에 연결 시도

    if (clientfd < 0) {
        fprintf(stderr, "연결 실패\n");
        exit(1);
    }

    printf("연결 성공!\n");

    Close(clientfd);
    return 0;
}

