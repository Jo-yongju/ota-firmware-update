#include <stdio.h>
#include <unistd.h>
#include "../include/tcp_client.h"
#include "../include/gpio_trigger.h"
#include "../include/spi_master.h"

#define FIRMWARE_PATH   "/home/ssafy/firmware/firmware.bin"

int main(void)
{
    printf("========================================\n");
    printf("   OTA 펌웨어 업데이트 시스템 시작\n");
    printf("========================================\n");

    while (1) {
        printf("\n--- [1단계] 펌웨어 수신 ---\n");
        if (tcp_receive_firmware() < 0) {
            printf("[메인] 펌웨어 수신 실패. 3초 후 재시도...\n");
            sleep(3);
            continue;
        }

        printf("\n--- [2단계] GPIO 초기화 ---\n");
        int gpio_h = gpio_init();
        if (gpio_h < 0) {
            printf("[메인] GPIO 초기화 실패. 3초 후 재시도...\n");
            sleep(3);
            continue;
        }

        printf("\n--- [3단계] 인터럽트 전송 ---\n");
        trigger_interrupt(gpio_h);

        printf("\n--- [4단계] STM32 준비 대기 ---\n");
        if (wait_for_ready(gpio_h) < 0) {
            printf("[메인] STM32 응답 없음. 3초 후 재시도...\n");
            gpio_cleanup(gpio_h);
            sleep(3);
            continue;
        }

        printf("\n--- [5단계] SPI 펌웨어 전송 ---\n");
        int spi_fd = spi_init();
        if (spi_fd < 0) {
            printf("[메인] SPI 초기화 실패. 3초 후 재시도...\n");
            gpio_cleanup(gpio_h);
            sleep(3);
            continue;
        }

        int spi_gpio_h = spi_gpio_init();
        if (spi_gpio_h < 0) {
            printf("[메인] SPI GPIO 초기화 실패. 3초 후 재시도...\n");
            close(spi_fd);
            gpio_cleanup(gpio_h);
            sleep(3);
            continue;
        }

        if (spi_send_firmware(spi_fd, spi_gpio_h, FIRMWARE_PATH) < 0) {
            printf("[메인] SPI 전송 실패. 3초 후 재시도...\n");
            spi_gpio_cleanup(spi_gpio_h);
            close(spi_fd);
            gpio_cleanup(gpio_h);
            sleep(3);
            continue;
        }

        spi_gpio_cleanup(spi_gpio_h);
        close(spi_fd);
        gpio_cleanup(gpio_h);

        printf("\n========================================\n");
        printf("   OTA 업데이트 완료!\n");
        printf("========================================\n");
    }

    return 0;
}
