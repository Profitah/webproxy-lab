#include "csapp.h"  // Robust I/O 함수들과 네트워크 관련 래퍼 함수들이 정의된 헤더

void echo(int connfd) {
    size_t n;               // [1] 클라이언트가 보낸 데이터의 바이트 수를 저장할 변수
    char buf[MAXLINE];      // [2] 데이터를 읽고 쓸 때 사용할 버퍼 (최대 MAXLINE 바이트)
    rio_t rio;              // [3] Robust I/O용 구조체 (내부 버퍼와 상태를 저장)

    // [4] Robust I/O 초기화: 소켓 디스크립터 connfd를 기반으로 rio 구조체 설정
    Rio_readinitb(&rio, connfd);

    // [5] 클라이언트로부터 한 줄씩 읽어서 그대로 다시 클라이언트에게 보내는 반복 루프
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {  // [5-1] 한 줄 읽기 (0이면 EOF)
        printf("server received %zu bytes\n", n);           // [5-2] 받은 바이트 수 출력
        Rio_writen(connfd, buf, n);                         // [5-3] 같은 내용을 다시 클라이언트에 전송 (echo)
    }
}
