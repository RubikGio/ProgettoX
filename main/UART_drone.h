#ifndef UART_DRONE_H
#define UART_DRONE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef struct {
	size_t lenght;
	uint8_t *data;
} msg_t;

void setting_uart_trx(QueueHandle_t, QueueHandle_t);
void invio_pacchetto_test(QueueHandle_t);

#ifdef __cplusplus
}
#endif

#endif