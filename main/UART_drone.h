#ifndef UART_DRONE_H
#define UART_DRONE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#define MAX_BUFF 40

#define MIO_UART UART_NUM_1
#define TX_TO_ST 5
#define ST_TO_RX 4

#define ENGINE 0x67
#define MODE 0x69
#define FLY_ON 0x6A

#define BUFF_SIZE 1024

extern volatile int pck_lock;

typedef struct {
	size_t len;
	uint8_t buffer[MAX_BUFF];
} msg_t;

void setting_uart_trx(QueueHandle_t, QueueHandle_t, QueueHandle_t);
void invio_pacchetto_test(QueueHandle_t);

#ifdef __cplusplus
}
#endif

#endif