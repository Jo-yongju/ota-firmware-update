#ifndef GPIO_TRIGGER_H
#define GPIO_TRIGGER_H

// GPIO 초기화 (출력: GPIO 17 풀업, 입력: GPIO 27 풀다운)
// 성공하면 핸들 반환, 실패하면 -1
int gpio_init(void);

// STM32에 Falling edge 인터럽트 전송 (GPIO 17 → STM32 PD0)
// 성공하면 0, 실패하면 -1
int trigger_interrupt(int h);

// STM32 준비 완료 신호 대기 (STM32 PD1 → GPIO 27)
// 성공하면 0, 타임아웃이면 -1
int wait_for_ready(int h);

// GPIO 자원 해제
void gpio_cleanup(int h);

#endif
