#ifndef LED_GPIO_H
#define LED_GPIO_H

#include "led_strip.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	SYSTEM_IDLE,
	SYSTEM_CONNECTING,
	SYSTEM_CONNECTED
} sys_state;

void configura_led(sys_state *);

#ifdef __cplusplus
}
#endif

#endif