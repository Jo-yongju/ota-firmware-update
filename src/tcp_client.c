#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <stdint.h>
#include "../include/tcp_client.h"

#define SERVER_IP    "3.238.95.165"
#define SERVER_PORT  8080
#define HEADER_SIZE  68
#define BUF_SIZE     4096
#define SAVE_DIR     "/home/ssafy/firmware"
#define SAVE_PATH    SAVE_DIR "/firmware.bin"

static int check_integrity(const char *filepath, const char *expected_hash)
{
    char command[512];
    char result_hash[65] = {0};

    snprintf(command, sizeof(command), "sha256sum %s | awk '{print $1}'", filepath);

    FILE *pipe = popen(command, "r");
    if (!pipe) return 0;

    if (fgets(result_hash, 65, pipe) != NULL) {
        pclose(pipe);
        if (strncmp(result_hash, expected_hash, 64) == 0) {
            return 1;
        }
    } else {
        pclose(pipe);
    }
    return 0;
}

static int recv_exact(int sock, void *buf, size_t n)
{
    size_t received = 0;
    while (received < n) {
        ssize_t ret = recv(sock, (char *)buf + received, n - received, 0);
        if (ret <= 0) return -1;
        received += ret;
    }
    return 0;
}

int tcp_receive_firmware(void)
{
    mkdir(SAVE_DIR, 0755);

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[TCP] socket 생성 실패");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    printf("\n[TCP] 서버 연결 시도... (%s:%d)\n", SERVER_IP, SERVER_PORT);
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("[TCP] 서버가 오프라인입니다.\n");
        close(sock);
        return -1;
    }
    printf("[TCP] 서버 연결 성공! 수신 대기 중...\n");

    uint8_t header[HEADER_SIZE];
    if (recv_exact(sock, header, HEADER_SIZE) < 0) {
        printf("[TCP] 헤더 수신 실패\n");
        close(sock);
        return -1;
    }

    uint32_t file_size = 0;
    file_size |= (uint32_t)header[0];
    file_size |= (uint32_t)header[1] << 8;
    file_size |= (uint32_t)header[2] << 16;
    file_size |= (uint32_t)header[3] << 24;

    char expected_sha256[65] = {0};
    memcpy(expected_sha256, header + 4, 64);

    printf("[TCP] 헤더 수신 완료\n");
    printf("  - 파일 크기: %u bytes\n", file_size);
    printf("  - SHA-256:   %.16s...\n", expected_sha256);

    FILE *fp = fopen(SAVE_PATH, "wb");
    if (!fp) {
        perror("[TCP] 파일 생성 실패");
        close(sock);
        return -1;
    }

    uint8_t buf[BUF_SIZE];
    uint32_t remaining = file_size;
    uint32_t total_recv = 0;
    int recv_error = 0;

    while (remaining > 0) {
        size_t to_recv = (remaining > BUF_SIZE) ? BUF_SIZE : remaining;
        ssize_t ret = recv(sock, buf, to_recv, 0);
        if (ret <= 0) {
            printf("\n[TCP] 수신 중 연결 끊김 (%u/%u bytes)\n", total_recv, file_size);
            recv_error = 1;
            break;
        }
        fwrite(buf, 1, ret, fp);
        remaining  -= ret;
        total_recv += ret;

        int percent = (int)((uint64_t)total_recv * 100 / file_size);
        printf("\r[TCP] 수신 중... %u / %u bytes (%d%%)", total_recv, file_size, percent);
        fflush(stdout);
    }
    fclose(fp);

    if (!recv_error) {
        printf("\n[TCP] 수신 완료. 무결성 검증 시작...\n");

        if (check_integrity(SAVE_PATH, expected_sha256)) {
            printf("[검증] 무결성 검사 통과! (OK)\n");
            send(sock, "OK", 2, 0);
            close(sock);
            return 0;
        } else {
            printf("[검증] 무결성 검사 실패! (HASH_MISMATCH)\n");
            send(sock, "FAIL", 4, 0);
            remove(SAVE_PATH);
            printf("[검증] 손상된 파일 삭제 완료\n");
        }
    } else {
        send(sock, "FAIL", 4, 0);
        remove(SAVE_PATH);
    }

    close(sock);
    return -1;
}
