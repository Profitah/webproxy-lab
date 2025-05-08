#include "csapp.h"

int main(void) {
    char *buf;
    int n1 = 0, n2 = 0;
    int has_input = 0; // 사용자가 값을 입력했는지 여부
    char content[MAXLINE];
    int content_len = 0;

    // 쿼리 스트링에서 num1, num2 파싱
    if ((buf = getenv("QUERY_STRING")) != NULL && strlen(buf) > 0) {
        char *p1 = strstr(buf, "num1=");
        char *p2 = strstr(buf, "num2=");
        if (p1) n1 = atoi(p1 + strlen("num1="));
        if (p2) n2 = atoi(p2 + strlen("num2="));
        has_input = 1;
    }

    // HTML 내용 생성
    content_len += snprintf(content + content_len, MAXLINE - content_len,
        "<html><head><meta charset=\"UTF-8\"><title>Adder</title></head><body>");

    content_len += snprintf(content + content_len, MAXLINE - content_len,
        "<h2> Web 기반 덧셈기</h2>");
    
    // 입력 폼 출력
    content_len += snprintf(content + content_len, MAXLINE - content_len,
        "<form action=\"/cgi-bin/adder\" method=\"GET\">"
        "<input type=\"text\" name=\"num1\"> + "
        "<input type=\"text\" name=\"num2\"> "
        "<input type=\"submit\" value=\"더하기\">"
        "</form><p>");

    // 결과 출력
    if (has_input) {
        content_len += snprintf(content + content_len, MAXLINE - content_len,
            "<p><strong>결과:</strong> %d + %d = %d</p>", n1, n2, n1 + n2);
    }

    content_len += snprintf(content + content_len, MAXLINE - content_len,
        "</body></html>");

    // HTTP 응답 헤더 출력
    printf("Content-type: text/html; charset=UTF-8\r\n");
    printf("Content-length: %d\r\n\r\n", content_len);

    // HTML 본문 출력
    printf("%s", content);
    fflush(stdout);

    exit(0);
}