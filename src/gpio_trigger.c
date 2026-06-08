#include <stdio.h>
#include <lgpio.h>
#include <unistd.h>
#include <time.h>
#include "../include/gpio_trigger.h"

#define GPIO_CHIP       0

// ===== 핀 매핑 =====
// RPi GPIO 17 (출력) ---> STM32 PD0 (입력): 인터럽트 전송
// RPi GPIO 27 (입력) <--- STM32 PD1 (출력): 준비 완료 신호
#define GPIO_OUT_PIN    17
#define GPIO_IN_PIN     27
#define TIMEOUT_SEC     10

int gpio_init(void)
{
    int h = lgGpiochipOpen(GPIO_CHIP);
    if (h < 0) {
        printf("[GPIO] 칩 열기 실패\n");
        return -1;
    }

    if (lgGpioClaimOutput(h, LG_SET_PULL_UP, GPIO_OUT_PIN, 1) < 0) {
        printf("[GPIO] 출력 핀(GPIO %d) 설정 실패\n", GPIO_OUT_PIN);
        lgGpiochipClose(h);
        return -1;
    }

    if (lgGpioClaimInput(h, LG_SET_PULL_DOWN, GPIO_IN_PIN) < 0) {
        printf("[GPIO] 입력 핀(GPIO %d) 설정 실패\n", GPIO_IN_PIN);
        lgGpiochipClose(h);
        return -1;
    }

    printf("[GPIO] 초기화 완료 (출력: GPIO %d, 입력: GPIO %d)\n", GPIO_OUT_PIN, GPIO_IN_PIN);
    return h;
}

int trigger_interrupt(int h)
{
    printf("[GPIO] 인터럽트 전송 (RPi GPIO %d -> STM32 PD0, Falling edge)\n", GPIO_OUT_PIN);
    lgGpioWrite(h, GPIO_OUT_PIN, 0);
    usleep(10000);
    lgGpioWrite(h, GPIO_OUT_PIN, 1);
    printf("[GPIO] 인터럽트 전송 완료 (복귀: HIGH)\n");
    return 0;
}

int wait_for_ready(int h)
{
    printf("[GPIO] STM32 준비 신호 대기 중... (STM32 PD1 -> RPi GPIO %d, 타임아웃: %d초)\n",
           GPIO_IN_PIN, TIMEOUT_SEC);

    time_t start = time(NULL);

    while (1) {
        int val = lgGpioRead(h, GPIO_IN_PIN);
        if (val == 1) {
            printf("[GPIO] STM32 준비 완료 신호 감지! (PD1 = HIGH)\n");
            return 0;
        }

        if (time(NULL) - start >= TIMEOUT_SEC) {
            printf("[GPIO] 타임아웃! STM32 응답 없음\n");
            return -1;
        }

        usleep(10000);
    }
}

void gpio_cleanup(int h)
{
    if (h >= 0) {
        lgGpiochipClose(h);
    }
}
