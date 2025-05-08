#include "csapp.h"  // Robust I/O 함수들과 네트워크 래퍼 함수들이 정의된 헤더

int main(int argc, char **argv) {
    int clientfd;               // 서버와 연결할 소켓 디스크립터
    char *host, *port;          // 명령줄 인자로 받은 서버 주소(host)와 포트 번호(port)
    char buf[MAXLINE];          // 사용자 입력 및 서버 응답을 저장할 버퍼
    rio_t rio;                  // Robust I/O용 구조체 (버퍼링 및 읽기 상태 저장용)

    // [1] 인자 개수 검사: 프로그램 실행 시 host와 port 인자가 없으면 사용법 안내 후 종료
    if (argc != 3) { // 즉, 사용자가 입력해야 할 인자는 2개이지만,
        fprintf(stderr, "usage: %s <host> <port>\n", argv[0]); // 운영체제가 argv[0]에 실행 파일 이름을 자동으로 추가하기 때문에 argc == 3이 됨.
        exit(0); // 인자 개수가 3이 아니면 프로그램 종료
    }

    // [2] 명령줄 인자로부터 호스트와 포트 값 설정
    host = argv[1];             // ex: "localhost"
    port = argv[2];             // ex: "8080"

    // [3] 서버에 연결 (host, port 기반 TCP 연결 시도)
    clientfd = Open_clientfd(host, port);  // 성공 시 소켓 디스크립터(소켓을 식별하는 것) 반환, 실패 시 -1

    // [4] Robust I/O 초기화: 소켓 디스크립터를 기반으로 rio 구조체 준비
    Rio_readinitb(&rio, clientfd);

    // [5] 사용자 입력 → 서버 전송 → 서버 응답 수신 → 화면 출력 반복
    while (Fgets(buf, MAXLINE, stdin) != NULL) { // 사용자 입력 한 줄 읽기
        Rio_writen(clientfd, buf, strlen(buf));  // 입력한 줄을 서버로 전송
        Rio_readlineb(&rio, buf, MAXLINE);       // 서버로부터 응답 한 줄 수신
        Fputs(buf, stdout);                      // 서버 응답을 화면에 출력
    }

    // [6] 연결 종료: 소켓 닫기
    Close(clientfd);

    return 0;
}
