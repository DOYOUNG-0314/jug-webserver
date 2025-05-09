#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "csapp.h"  // Robust I/O 및 소켓 함수 포함

/* 캐시 관련 상수 (과제 3에서 사용 예정) */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define MAXLINE 8192  // 버퍼 최대 크기

/* User-Agent 헤더 상수 */
static const char *user_agent_hdr =
    "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

/* 클라이언트의 요청 하나를 처리하는 함수 */
void doit(int clientfd);

/* main - 프록시 서버 메인 루프 */
int main(int argc, char **argv) {
    int listenfd, clientfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  // 클라이언트 주소
    char port[8];

    // 포트 인자 체크
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(1);
    }

    // 프록시 서버 리스닝 시작
    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        clientfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);  // 클라이언트 연결 수락
        doit(clientfd);    // 요청 처리
        Close(clientfd);   // 클라이언트 소켓 종료
    }
}

/* parse_uri - URI를 파싱하여 hostname, path, port로 분리 */
void parse_uri(char *uri, char *hostname, char *path, int *port) {
    char *hostbegin, *hostend, *pathbegin;
    int default_port = 80;

    *port = default_port;

    // "http://" 또는 "//" 뒤부터 호스트 시작
    hostbegin = strstr(uri, "//");
    hostbegin = (hostbegin != NULL) ? hostbegin + 2 : uri;

    // '/'를 기준으로 path 분리
    hostend = strchr(hostbegin, '/');
    if (hostend != NULL) {
        strcpy(path, hostend);  // path에 '/' 이하 복사
        *hostend = '\0';        // host부분만 남도록 널 종료
    } else {
        strcpy(path, "/");      // 경로가 없으면 기본으로 '/'
    }

    // 포트 번호가 명시된 경우 ':' 이후를 추출
    char *portpos = strchr(hostbegin, ':');
    if (portpos != NULL) {
        *portpos = '\0';        // ':'을 '\0'로 바꿔 host만 남김
        *port = atoi(portpos + 1);  // 포트 문자열 → 정수
    }

    strcpy(hostname, hostbegin);  // hostname 복사
}

/* doit - 클라이언트 요청 처리 */
void doit(int clientfd) {
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], path[MAXLINE];
    int serverfd, port;
    rio_t client_rio, server_rio;
    char request_hdr[MAXLINE];

    // 클라이언트로부터 요청 읽기
    Rio_readinitb(&client_rio, clientfd);
    Rio_readlineb(&client_rio, buf, MAXLINE);
    sscanf(buf, "%s %s %s", method, uri, version);  // 요청 라인 파싱

    // GET 메소드 외에는 지원하지 않음
    if (strcasecmp(method, "GET") != 0) {
        printf("501 Not Implemented\n");
        return;
    }

    char uri_copy[MAXLINE];
    strcpy(uri_copy, uri);
    parse_uri(uri_copy, hostname, path, &port);
    printf("[DEBUG] parsed URI: hostname=%s, path=%s, port=%d\n", hostname, path, port);



    // 포트를 문자열로 변환하여 Open_clientfd에 전달
    char port_str[8];
    sprintf(port_str, "%d", port);
    printf("[DEBUG] connecting to %s:%s\n", hostname, port_str);
    serverfd = Open_clientfd(hostname, port_str);

    if (serverfd < 0) {
        fprintf(stderr, "Unable to connect to server %s:%s\n", hostname, port_str);
        return;
    }

    // 요청 헤더 생성
    snprintf(request_hdr, MAXLINE, "GET %s HTTP/1.0\r\n", path);
    snprintf(request_hdr + strlen(request_hdr), MAXLINE - strlen(request_hdr),
             "Host: %s\r\n", hostname);
    snprintf(request_hdr + strlen(request_hdr), MAXLINE - strlen(request_hdr),
             "%s", user_agent_hdr);
    snprintf(request_hdr + strlen(request_hdr), MAXLINE - strlen(request_hdr),
             "Connection: close\r\nProxy-Connection: close\r\n\r\n");

    // 서버 연결 초기화 및 요청 전송
    Rio_readinitb(&server_rio, serverfd);
    printf("[DEBUG] sending request to server:\n%s\n", request_hdr);
    Rio_writen(serverfd, request_hdr, strlen(request_hdr));
    

    // 서버 응답 → 클라이언트로 전달
    size_t n;
    while ((n = Rio_readlineb(&server_rio, buf, MAXLINE)) > 0) {
        Rio_writen(clientfd, buf, n);
    }

    Close(serverfd);  // 서버 소켓 종료
    
}

