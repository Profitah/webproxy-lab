#include "csapp.h"  // Robust I/O 함수들과 네트워크 관련 래퍼 함수들이 정의된 헤더
void echo(int connfd); 

int main(int argc, char **argv) {
    int listenfd, connfd;                     // [1] 수신용 소켓과 연결용 소켓 디스크립터
    socklen_t clientlen;                      // [2] 클라이언트 주소 구조체의 크기 저장용
    struct sockaddr_storage clientaddr;       // [3] 클라이언트 주소 정보를 저장할 구조체
    char client_hostname[MAXLINE];            // [4] 클라이언트 호스트 이름 저장용 문자열
    char client_port[MAXLINE];                // [5] 클라이언트 포트 번호 저장용 문자열

    // [6] 인자 개수 확인: 포트 번호가 없으면 에러 출력 후 종료 
    if (argc != 3) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    // [7] 서버용 수신 소켓 생성 (지정된 포트로 바인딩 및 리스닝 상태로 전환)
    listenfd = Open_listenfd(argv[1]);

    // [8] 무한 루프: 클라이언트 연결을 지속적으로 처리
    while (1) { 
        clientlen = sizeof(struct sockaddr_storage); // [8-1] 주소 구조체 크기 설정

        // [8-2] 클라이언트 연결 수락 → 연결용 소켓 디스크립터 반환
        connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);

        // [8-3] 연결된 클라이언트의 호스트 이름과 포트 번호를 문자열로 변환
        Getnameinfo((SA *)&clientaddr, clientlen,
                    client_hostname, MAXLINE,
                    client_port, MAXLINE, 0);

        // [8-4] 연결된 클라이언트 정보 출력
        printf("connected to (%s, %s)\n", client_hostname, client_port);

        // [8-5] echo 서비스 수행 (클라이언트 요청을 그대로 다시 응답)
        echo(connfd);

        // [8-6] 연결 종료
        Close(connfd);
    }
}
