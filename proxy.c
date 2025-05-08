#include "csapp.h"
#include <stdio.h>
#include <string.h> // strncasecmp, strcat, strcpy 등 사용

/* 기본 헤더 정의 */
static const char *user_agent_hdr_val = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
static const char *connection_val = "close";
static const char *proxy_connection_val = "close";

#define MAX_CACHE_SIZE (1024*1024) // 1MB, 예시 크기
#define MAX_OBJECT_SIZE (100*1024) // 100KB, 예시 크기

/* 함수 프로토타입 선언 */
void doit_proxy(int client_fd); // doit_proxy로 변경
int parse_uri(char *uri, char *target_host, char *target_port_str, char *target_path);
void build_request_to_server(char *request_buf, size_t buf_size, char *method, char *target_path, char *http_version,
                               char *target_host, char *target_port_str, rio_t *client_rio , const char *full_url_for_scheme);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *proxy_thread(void *vargp);


int main(int argc, char **argv) {
    int listenfd, *connfdp;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }
    Signal(SIGPIPE, SIG_IGN);

    listenfd = Open_listenfd(argv[1]);
    printf("Proxy listening on port %s\n", argv[1]);

    while (1) {
        clientlen = sizeof(clientaddr);
        connfdp = Malloc(sizeof(int)); // 각 스레드에 connfd를 안전하게 전달하기 위해 동적 할당
        *connfdp = Accept(listenfd, (SA *)&clientaddr, &clientlen);
        
        Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);

        Pthread_create(&tid, NULL, proxy_thread, connfdp);
    }
    return 0;
}

void *proxy_thread(void *vargp) {
    int connfd = *((int *)vargp);
    Pthread_detach(pthread_self()); // 스레드가 종료될 때 자동으로 리소스 회수
    Free(vargp); // 동적 할당된 메모리 해제

    doit_proxy(connfd);
    Close(connfd);
    // 연결 종료 로그는 main 루프 또는 여기서도 가능 (여기서는 doit_proxy 후 close하므로 이 위치가 적절)
    // printf("Connection closed for client fd %d\n", connfd); // Getnameinfo는 여기서는 힘듦
    return NULL;
}


void doit_proxy(int client_fd) {
    int origin_server_fd = -1;
    char client_req_buf[MAXLINE];
    char request_to_server_buf[MAX_CACHE_SIZE]; // MAX_CACHE_SIZE를 적절히 정의해야 함
    char server_resp_buf[MAXLINE]; // 서버 응답 청크용 버퍼

    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char target_host[MAXLINE], target_port_str[MAXLINE], target_path[MAXLINE];
    rio_t client_rio, origin_server_rio;
    ssize_t n;

    Rio_readinitb(&client_rio, client_fd);
    if (Rio_readlineb(&client_rio, client_req_buf, MAXLINE) <= 0) {
        fprintf(stderr, "Proxy: Error reading request line from client or client closed connection.\n");
        return;
    }
    printf("=== FROM CLIENT (Request Line) ===\n%s", client_req_buf);

    if (sscanf(client_req_buf, "%s %s %s", method, uri, version) != 3) {
        clienterror(client_fd, client_req_buf, "400", "Bad Request", "Malformed request line");
        return;
    }

    if (strcasecmp(method, "GET")) {
        clienterror(client_fd, method, "501", "Not Implemented", "Proxy does not implement this method (only GET)");
        return;
    }
    
    // --- URI 파싱 수정 시작 ---
    char actual_uri_to_parse[MAXLINE];
    // 클라이언트가 "/http://host/path" 형태로 보낸 경우 (HTML fetch가 이렇게 보냄)
    if (uri[0] == '/' && (strncasecmp(uri + 1, "http://", 7) == 0 || strncasecmp(uri + 1, "https://", 8) == 0)) {
        strcpy(actual_uri_to_parse, uri + 1); // 맨 앞 '/' 제거
    }
    // 클라이언트가 "http://host/path" 형태로 보낸 경우 (예: curl, telnet)
    else if (strncasecmp(uri, "http://", 7) == 0 || strncasecmp(uri, "https://", 8) == 0) {
        strcpy(actual_uri_to_parse, uri);
    }
    // 그 외 형식 (예: "Host:" 헤더를 사용하는 일반적인 웹서버 요청 "/path")은 현재 이 프록시에서 지원하지 않음
    else {
        clienterror(client_fd, uri, "400", "Bad Request",
                    "Proxy request URI must be absolute and start with /http:// or http://");
        return;
    }
    // --- URI 파싱 수정 끝 ---

    if (!parse_uri(actual_uri_to_parse, target_host, target_port_str, target_path)) {
        clienterror(client_fd, actual_uri_to_parse, "400", "Bad Request", "Cannot parse destination URI");
        return;
    }
    printf("Parsed URI for origin: host=[%s], port=[%s], path=[%s]\n", target_host, target_port_str, target_path);

    origin_server_fd = Open_clientfd(target_host, target_port_str);
    if (origin_server_fd < 0) {
        clienterror(client_fd, target_host, "502", "Bad Gateway", "Could not connect to origin server");
        return;
    }
    printf("Connected to origin server: %s:%s\n", target_host, target_port_str);
    
    // --- 목적지 서버 소켓에 읽기 타임아웃 설정 (선택적이지만 권장) ---
    struct timeval timeout;
    timeout.tv_sec = 5; 
    timeout.tv_usec = 0;
    if (setsockopt(origin_server_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        fprintf(stderr, "Proxy: Error setting socket receive timeout for origin_server_fd\n");
    } else {
        printf("Proxy: Receive timeout set for origin_server_fd (%d seconds)\n", (int)timeout.tv_sec);
    }
    // ---------------------------------------------

    Rio_readinitb(&origin_server_rio, origin_server_fd);

    build_request_to_server(request_to_server_buf, sizeof(request_to_server_buf),
                              method, target_path, "HTTP/1.0", // HTTP/1.0으로 단순화
                              target_host, target_port_str, &client_rio, actual_uri_to_parse);

    printf("=== TO SERVER (Full Request by Proxy) ===\n%s", request_to_server_buf);
    if (rio_writen(origin_server_fd, request_to_server_buf, strlen(request_to_server_buf)) < 0) {
        fprintf(stderr, "Proxy: Error writing request to origin server.\n");
        clienterror(client_fd, target_host, "502", "Bad Gateway", "Proxy failed to send request to origin server");
        Close(origin_server_fd);
        return;
    }
    
    // --- 응답 전달 및 CORS 헤더 삽입 로직 ---
    printf("Forwarding response from origin server to client (with CORS header injection)...\n");
    char server_header_line[MAXLINE];
    char all_origin_headers_buf[MAX_CACHE_SIZE] = ""; // 원본 서버 헤더 전체 저장용
    size_t current_headers_len = 0;
    int is_first_header_line = 1;

    // 1. 원본 서버로부터 모든 헤더를 읽어 버퍼에 저장
    while (Rio_readlineb(&origin_server_rio, server_header_line, MAXLINE) > 0) {
        if (is_first_header_line) {
            printf("=== FROM ORIGIN SERVER (Status Line) ===\n%s", server_header_line);
            is_first_header_line = 0;
        } else {
            printf("%s", server_header_line); // 나머지 헤더 출력
        }

        // 버퍼 크기 확인하며 헤더 추가
        if (current_headers_len + strlen(server_header_line) < MAX_CACHE_SIZE) {
            strcat(all_origin_headers_buf, server_header_line);
            current_headers_len += strlen(server_header_line);
        } else {
            fprintf(stderr, "Proxy: Origin server's headers too large for buffer.\n");
            clienterror(client_fd, target_host, "502", "Bad Gateway", "Origin server response headers too large");
            Close(origin_server_fd);
            return;
        }
        if (strcmp(server_header_line, "\r\n") == 0) { // 헤더의 끝
            break;
        }
    }
    if (n <= 0 && strcmp(server_header_line, "\r\n") != 0) { // 헤더 읽기 중 오류 또는 조기 종료
         fprintf(stderr, "Proxy: Error reading headers from origin server or connection closed before end of headers.\n");
         clienterror(client_fd, target_host, "502", "Bad Gateway", "Error reading response headers from origin server");
         Close(origin_server_fd);
         return;
    }

    // 2. 클라이언트에게 보낼 최종 헤더 구성 (원본 헤더 + CORS 헤더)
    char final_headers_to_client[MAX_CACHE_SIZE + 200]; // CORS 헤더 공간 추가
    strcpy(final_headers_to_client, ""); // 초기화

    char *end_of_original_headers_marker = strstr(all_origin_headers_buf, "\r\n\r\n");
    if (end_of_original_headers_marker) {
        // 원본 헤더에서 마지막 CRLFCRLF 중 앞의 CRLF까지만 복사
        strncat(final_headers_to_client, all_origin_headers_buf, (end_of_original_headers_marker - all_origin_headers_buf));
    } else { // 헤더가 "\r\n" 하나로 끝난 경우 등 (매우 짧은 응답)
        if (current_headers_len >= 2 && strcmp(all_origin_headers_buf + current_headers_len - 2, "\r\n") == 0) {
            strncat(final_headers_to_client, all_origin_headers_buf, current_headers_len - 2); // 마지막 \r\n 제외
        } else { // 비정상적이지만, 일단 복사
            strcat(final_headers_to_client, all_origin_headers_buf);
        }
    }

    // CORS 헤더 추가 (필요한 헤더들을 여기에 추가 가능)
    strcat(final_headers_to_client, "\r\nAccess-Control-Allow-Origin: *"); // 모든 출처 허용 (개발용)
    // 특정 출처만 허용하려면:
    // strcat(final_headers_to_client, "\r\nAccess-Control-Allow-Origin: http://127.0.0.1:5500");
    // 필요시 다른 CORS 헤더 추가:
    // strcat(final_headers_to_client, "\r\nAccess-Control-Allow-Methods: GET, POST, OPTIONS");
    // strcat(final_headers_to_client, "\r\nAccess-Control-Allow-Headers: Content-Type, Authorization");

    strcat(final_headers_to_client, "\r\n\r\n"); // 최종 헤더 끝 표시

    printf("=== TO CLIENT (Proxy Modified Headers) ===\n%s", final_headers_to_client);
    if (rio_writen(client_fd, final_headers_to_client, strlen(final_headers_to_client)) < 0) {
        fprintf(stderr, "Proxy: Error writing modified headers to client.\n");
        Close(origin_server_fd);
        return;
    }

    // 3. 원본 서버로부터 응답 본문을 읽어 클라이언트로 전달
    while ((n = Rio_readnb(&origin_server_rio, server_resp_buf, MAXLINE)) > 0) {
        if (rio_writen(client_fd, server_resp_buf, n) != n) {
            fprintf(stderr, "Proxy: Error writing response body to client.\n");
            goto end_proxy_logic; // 에러 발생 시 정리 로직으로 점프
        }
    }
    if (n < 0) { // 읽기 오류 또는 타임아웃
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "Proxy: Timeout reading body from origin server %s:%s\n", target_host, target_port_str);
            clienterror(client_fd, target_host, "504", "Gateway Timeout", "Proxy timed out waiting for response body from origin server");
        } else {
            fprintf(stderr, "Proxy: Error reading response body from origin server (errno: %d %s)\n", errno, strerror(errno));
            clienterror(client_fd, target_host, "502", "Bad Gateway", "Proxy error reading response body from origin server");
        }
    } else if (n == 0) {
         printf("Proxy: Origin server %s:%s closed connection (EOF) after sending body or if no body.\n", target_host, target_port_str);
    }

end_proxy_logic:
    if (origin_server_fd >= 0) {
        Close(origin_server_fd);
        printf("Disconnected from origin server: %s:%s\n", target_host, target_port_str);
    }
    // 클라이언트 fd는 proxy_thread 함수에서 닫습니다.
}


// build_request_to_server 함수 (기존 코드 사용, 약간의 안전성 강화)
void build_request_to_server(char *request_buf, size_t buf_size, char *method, char *target_path, char *http_version,
    char *target_host, char *target_port_str, rio_t *client_rio, const char *actual_uri_to_parse) {
    char client_hdr_line[MAXLINE];
    char other_hdrs_temp[MAX_CACHE_SIZE] = "";
    char final_host_hdr[MAXLINE];
    size_t current_len = 0;
    int space_left;


    // 1. 요청 라인 생성
    space_left = buf_size - current_len -1; // -1 for null terminator
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "%s %s %s\r\n", method, target_path, http_version);

    // 2. 클라이언트로부터 헤더를 읽고 other_hdrs_temp에 누적
    printf("--- Reading headers from client to forward ---\n");
    while (Rio_readlineb(client_rio, client_hdr_line, MAXLINE) > 0) {
        if (strcmp(client_hdr_line, "\r\n") == 0) {
            break;
        }
        printf("%s", client_hdr_line);

        if (strncasecmp(client_hdr_line, "Host:", 5) == 0 ||
            strncasecmp(client_hdr_line, "User-Agent:", 11) == 0 ||
            strncasecmp(client_hdr_line, "Connection:", 11) == 0 ||
            strncasecmp(client_hdr_line, "Proxy-Connection:", 17) == 0) {
            continue;
        }
        if (strlen(other_hdrs_temp) + strlen(client_hdr_line) < MAX_CACHE_SIZE) {
            strcat(other_hdrs_temp, client_hdr_line);
        } else {
            fprintf(stderr, "Proxy: Client headers too large for 'other_hdrs_temp' buffer.\n");
            // Optionally, stop processing or truncate. For now, just log.
        }
    }
    printf("--- Finished reading headers from client ---\n");

    // 3. 필수/기본 헤더 구성
    space_left = buf_size - current_len -1;
    if (strcmp(target_port_str, "80") == 0 && strncasecmp(actual_uri_to_parse, "http://", 7) == 0) { // HTTP이고 기본 포트
        snprintf(final_host_hdr, MAXLINE, "Host: %s\r\n", target_host);
    } else if (strcmp(target_port_str, "443") == 0 && strncasecmp(actual_uri_to_parse, "https://", 8) == 0) { // HTTPS이고 기본 포트
        snprintf(final_host_hdr, MAXLINE, "Host: %s\r\n", target_host);
    }
    else {
        snprintf(final_host_hdr, MAXLINE, "Host: %s:%s\r\n", target_host, target_port_str);
    }
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "%s", final_host_hdr);
    
    space_left = buf_size - current_len -1;
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "User-Agent: %s\r\n", user_agent_hdr_val);
    space_left = buf_size - current_len -1;
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "Connection: %s\r\n", connection_val);
    space_left = buf_size - current_len -1;
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "Proxy-Connection: %s\r\n", proxy_connection_val);

    // 4. 클라이언트로부터 전달받은 기타 헤더 추가
    space_left = buf_size - current_len -1;
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "%s", other_hdrs_temp);

    // 5. HTTP 요청 헤더의 끝을 알리는 빈 줄 추가
    space_left = buf_size - current_len -1;
    current_len += snprintf(request_buf + current_len, space_left > 0 ? space_left : 0, "\r\n");

    if (current_len >= buf_size -1) { // 버퍼 오버플로우 체크
        fprintf(stderr, "Proxy: Request buffer overflow in build_request_to_server.\n");
        // request_buf[buf_size - 1] = '\0'; // 강제 널 종료
        // Handle error appropriately
    }
}

// parse_uri 함수 (기존 코드 사용, 약간의 안전성 강화 및 HTTPS 스킴 고려)
int parse_uri(char *uri, char *target_host, char *target_port_str, char *target_path) {
    char *host_start_ptr, *port_start_ptr, *path_start_ptr;
    char scheme[10]; // "http" 또는 "https" 저장

    // 1. 스킴 확인 및 건너뛰기
    if (strncasecmp(uri, "http://", 7) == 0) {
        strcpy(scheme, "http");
        host_start_ptr = uri + 7;
    } else if (strncasecmp(uri, "https://", 8) == 0) {
        strcpy(scheme, "https"); // HTTPS 지원 준비 (실제 CONNECT 메소드 처리는 별도)
        host_start_ptr = uri + 8;
    } else {
        fprintf(stderr, "URI Parse Error: Invalid scheme (must be http or https): %s\n", uri);
        return 0;
    }

    // 2. 경로 시작점 찾기 ('/' 또는 문자열 끝)
    path_start_ptr = strchr(host_start_ptr, '/');
    char temp_host_port[MAXLINE]; // 호스트와 포트 부분을 임시 저장

    if (path_start_ptr == NULL) { // 경로가 없으면 (예: "http://www.example.com")
        strcpy(target_path, "/");
        strncpy(temp_host_port, host_start_ptr, MAXLINE -1); // 호스트[:포트] 부분 복사
        temp_host_port[MAXLINE-1] = '\0';
    } else {
        snprintf(target_path, MAXLINE, "%s", path_start_ptr);
        // 호스트[:포트] 부분 복사 (path_start_ptr 앞까지)
        size_t host_port_len = path_start_ptr - host_start_ptr;
        if (host_port_len >= MAXLINE) host_port_len = MAXLINE -1;
        strncpy(temp_host_port, host_start_ptr, host_port_len);
        temp_host_port[host_port_len] = '\0';
    }

    // 3. 포트 번호 찾기 (temp_host_port 내에서 ':' 기준)
    port_start_ptr = strchr(temp_host_port, ':');
    if (port_start_ptr != NULL) { // 포트 번호 명시
        *port_start_ptr = '\0'; // 호스트명과 포트 분리
        snprintf(target_host, MAXLINE, "%s", temp_host_port);
        snprintf(target_port_str, MAXLINE, "%s", port_start_ptr + 1);
    } else { // 포트 번호 없음
        snprintf(target_host, MAXLINE, "%s", temp_host_port);
        if (strcmp(scheme, "http") == 0) strcpy(target_port_str, "80");
        else if (strcmp(scheme, "https") == 0) strcpy(target_port_str, "443");
        else strcpy(target_port_str, "80"); // 기본값 (예외)
    }

    if (strlen(target_path) == 0) { // 경로가 비었으면 "/"
        strcpy(target_path, "/");
    }
    return 1;
}


// clienterror 함수 (기존 코드 사용)
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
    char buf[MAXLINE], body[MAXBUF];
    int body_len = 0;

    body_len += snprintf(body + body_len, MAXBUF - body_len -1, "<html><title>Proxy Error</title>");
    body_len += snprintf(body + body_len, MAXBUF - body_len -1, "<body bgcolor=\"ffffff\">\r\n");
    body_len += snprintf(body + body_len, MAXBUF - body_len -1, "%s: %s\r\n", errnum, shortmsg);
    body_len += snprintf(body + body_len, MAXBUF - body_len -1, "<p>%s: %s\r\n", longmsg, cause);
    body_len += snprintf(body + body_len, MAXBUF - body_len -1, "<hr><em>The Tiny Proxy server</em>\r\n");
    body_len += snprintf(body + body_len, MAXBUF - body_len -1, "</body></html>");
    
    // body_len이 MAXBUF를 넘지 않도록 보장하지만, snprintf가 알아서 처리함
    // 마지막 null terminator를 위해 -1 처리했으므로, body_len은 실제 쓰인 길이.

    snprintf(buf, MAXLINE, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-type: text/html\r\n");
    Rio_writen(fd, buf, strlen(buf));
    // CORS 헤더 추가 (에러 응답에도 추가해주는 것이 좋을 수 있음)
    snprintf(buf, MAXLINE, "Access-Control-Allow-Origin: *\r\n");
    Rio_writen(fd, buf, strlen(buf));
    snprintf(buf, MAXLINE, "Content-length: %d\r\n\r\n", body_len);
    Rio_writen(fd, buf, strlen(buf));
    Rio_writen(fd, body, body_len);
}

// 전역 변수 actual_uri_to_parse 선언 제거 (doit_proxy 내 지역 변수로 사용)
// char actual_uri_to_parse[MAXLINE];