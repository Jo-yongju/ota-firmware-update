#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <lgpio.h>
#include <stdint.h>
#include "../include/spi_master.h"

#define SPI_DEVICE      "/dev/spidev0.0"
#define SPI_SPEED       500000
#define SPI_MODE_VAL    (SPI_MODE_0 | SPI_NO_CS)
#define CHUNK_SIZE      256
#define PACKET_SIZE     257

#define HEADER_BIN      0x00
#define HEADER_CRC      0x01

#define GPIO_CHIP       0

// ===== CRC32 =====
static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc = crc >> 1;
        }
        crc32_table[i] = crc;
    }
    crc32_table_ready = 1;
}

static uint32_t compute_crc32(const uint8_t *data, size_t len)
{
    if (!crc32_table_ready) crc32_init_table();

    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

static void wait_busy(int gpio_h)
{
    while (lgGpioRead(gpio_h, GPIO_BUSY_PIN) == 1) {
        usleep(1000);
    }
}

int spi_init(void)
{
    int fd = open(SPI_DEVICE, O_RDWR);
    if (fd < 0) {
        perror("[SPI] 디바이스 열기 실패");
        return -1;
    }

    uint8_t mode = SPI_MODE_VAL;
    uint8_t bits = 8;
    uint32_t speed = SPI_SPEED;

    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    printf("[SPI] 초기화 완료 (속도: %d Hz, Mode 0, NO_CS, MSB first)\n", speed);
    return fd;
}

int spi_gpio_init(void)
{
    int h = lgGpiochipOpen(GPIO_CHIP);
    if (h < 0) {
        printf("[SPI] GPIO 칩 열기 실패\n");
        return -1;
    }

    if (lgGpioClaimOutput(h, 0, GPIO_CS_PIN, 1) < 0) {
        printf("[SPI] CS 핀(GPIO %d) 설정 실패\n", GPIO_CS_PIN);
        lgGpiochipClose(h);
        return -1;
    }

    if (lgGpioClaimInput(h, LG_SET_PULL_DOWN, GPIO_BUSY_PIN) < 0) {
        printf("[SPI] BUSY 핀(GPIO %d) 설정 실패\n", GPIO_BUSY_PIN);
        lgGpiochipClose(h);
        return -1;
    }

    printf("[SPI] GPIO 초기화 완료 (CS: GPIO %d, BUSY: GPIO %d)\n", GPIO_CS_PIN, GPIO_BUSY_PIN);
    return h;
}

void spi_gpio_cleanup(int h)
{
    if (h >= 0) {
        lgGpioWrite(h, GPIO_CS_PIN, 1);
        lgGpiochipClose(h);
    }
}

static int spi_send_packet(int fd, int gpio_h, uint8_t header, uint8_t *data)
{
    uint8_t tx[PACKET_SIZE];

    tx[0] = header;
    memcpy(tx + 1, data, CHUNK_SIZE);

    struct spi_ioc_transfer tr = {
        .tx_buf = (unsigned long)tx,
        .rx_buf = 0,
        .len = PACKET_SIZE,
        .speed_hz = SPI_SPEED,
        .bits_per_word = 8,
    };

    lgGpioWrite(gpio_h, GPIO_CS_PIN, 0);
    int ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
    lgGpioWrite(gpio_h, GPIO_CS_PIN, 1);

    if (ret < 0) {
        perror("[SPI] 전송 실패");
        return -1;
    }

    return 0;
}

int spi_send_firmware(int fd, int gpio_h, const char *filepath)
{
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("[SPI] 펌웨어 파일 열기 실패");
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t *file_buf = (uint8_t *)malloc(file_size);
    if (!file_buf) {
        printf("[SPI] 메모리 할당 실패\n");
        fclose(fp);
        return -1;
    }
    fread(file_buf, 1, file_size, fp);
    fclose(fp);

    uint32_t crc = compute_crc32(file_buf, file_size);

    int total_packets = (file_size + CHUNK_SIZE - 1) / CHUNK_SIZE;
    printf("[SPI] 펌웨어 전송 시작 (크기: %ld bytes, 패킷 수: %d, CRC32: 0x%08X)\n",
           file_size, total_packets, crc);

    uint8_t data[CHUNK_SIZE];
    size_t bytes_sent = 0;
    int packet_num = 0;

    while (bytes_sent < (size_t)file_size) {
        wait_busy(gpio_h);

        memset(data, 0xFF, CHUNK_SIZE);
        size_t remaining = file_size - bytes_sent;
        size_t copy_len = (remaining > CHUNK_SIZE) ? CHUNK_SIZE : remaining;
        memcpy(data, file_buf + bytes_sent, copy_len);

        if (spi_send_packet(fd, gpio_h, HEADER_BIN, data) < 0) {
            printf("[SPI] 패킷 %d 전송 실패\n", packet_num);
            free(file_buf);
            return -1;
        }

        bytes_sent += copy_len;
        packet_num++;

        int percent = (int)((uint64_t)bytes_sent * 100 / file_size);
        printf("\r[SPI] 전송 중... 패킷 %d/%d (%d%%)", packet_num, total_packets, percent);
        fflush(stdout);
    }
    printf("\n");

    free(file_buf);

    wait_busy(gpio_h);

    printf("[SPI] CRC32 패킷 전송 (0x%08X)\n", crc);
    memset(data, 0xFF, CHUNK_SIZE);
    data[0] = (uint8_t)(crc & 0xFF);
    data[1] = (uint8_t)((crc >> 8) & 0xFF);
    data[2] = (uint8_t)((crc >> 16) & 0xFF);
    data[3] = (uint8_t)((crc >> 24) & 0xFF);

    if (spi_send_packet(fd, gpio_h, HEADER_CRC, data) < 0) {
        printf("[SPI] CRC 패킷 전송 실패\n");
        return -1;
    }

    printf("[SPI] 펌웨어 전송 완료! (%zu bytes, %d 패킷)\n", bytes_sent, packet_num);
    return 0;
}
