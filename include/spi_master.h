#ifndef SPI_MASTER_H
#define SPI_MASTER_H

// ===== SPI 및 GPIO 핀 정의 =====
// RPi GPIO 25 (출력) ---> STM32 PA4 (NSS): CS 수동 제어
// RPi GPIO 22 (입력) <--- STM32 PB3 (출력): BUSY 신호
// RPi GPIO 10 (MOSI) ---> STM32 PB5 (MOSI)
// RPi GPIO 11 (SCLK) ---> STM32 PA5 (SCK)
#define GPIO_CS_PIN     25
#define GPIO_BUSY_PIN   22

// SPI 디바이스 초기화 (500KHz, Mode 0, NO_CS)
// 성공하면 fd 반환, 실패하면 -1
int spi_init(void);

// SPI 전송용 GPIO 초기화 (CS: GPIO 25, BUSY: GPIO 22)
// 성공하면 핸들 반환, 실패하면 -1
int spi_gpio_init(void);

// SPI GPIO 자원 해제
void spi_gpio_cleanup(int h);

// 저장된 펌웨어 파일을 SPI로 STM32에 전송
// CS 수동 제어 + BUSY 흐름제어 + CRC32
// 성공하면 0, 실패하면 -1
int spi_send_firmware(int fd, int gpio_h, const char *filepath);

#endif
