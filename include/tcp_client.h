#ifndef TCP_CLIENT_H
#define TCP_CLIENT_H

// PC 서버에서 펌웨어를 수신하고 SHA-256 검증 후 저장
// 검증 결과를 서버에 "OK" 또는 "FAIL"로 보고
// 성공하면 0, 실패하면 -1
int tcp_receive_firmware(void);

#endif
