#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

#include "UART_drone.h"
#include "UDP_Connection.h"

const char* TAG_R = "UART RX:";
const char* TAG_T = "UART TX:";
const char *TAG_TEST = "UART TEST";

volatile int pck_lock = 0;

static void uart_tx_task(void *pvParameters){
	QueueHandle_t queueTx = (QueueHandle_t) pvParameters;
	msg_t msg;
	while (1)
	{	
		if(xQueueReceive(queueTx,&msg,portMAX_DELAY) == pdPASS){
			ESP_LOGI(TAG_T, "Prelevato pacchetto dalla coda! Lunghezza: %u byte", (unsigned) msg.len);
			
			int bytes_sended = uart_write_bytes(MIO_UART, (const char *) msg.buffer, msg.len);

			if(bytes_sended != msg.len){
				ESP_LOGW(TAG_T, "Attenzione: inviati solo %d byte su %u", bytes_sended, (unsigned) msg.len);
			}
		}
			
	}
}

void invio_pacchetto_test(QueueHandle_t queueTx) {
    uint8_t opcode = 0x64;
    uint8_t payload_bytes[1] = {0x01};
    uint16_t payload_len = sizeof(payload_bytes);
    size_t tt_pack_size = 6 + payload_len + 1;

    if (tt_pack_size > sizeof(((msg_t*)0)->buffer)) {
        ESP_LOGE(TAG_TEST, "Pacchetto troppo grande per il buffer inline");
        return;
    }

    msg_t msg = {0};
    msg.len = tt_pack_size;

    size_t idx = 0;
    msg.buffer[idx++] = 0x46;
    msg.buffer[idx++] = 0x48;
    msg.buffer[idx++] = 0x3E;
    msg.buffer[idx++] = opcode;
    msg.buffer[idx++] = (uint8_t)(payload_len & 0xFF);
    msg.buffer[idx++] = (uint8_t)(payload_len >> 8);
    memcpy(&msg.buffer[idx], payload_bytes, payload_len);
    idx += payload_len;

    uint8_t check = 0;
    for (size_t i = 0; i < payload_len; i++) {
        check ^= payload_bytes[i];
    }
    msg.buffer[idx] = check;  // ← rimosso il idx++ spurio che avevi

    if (xQueueSend(queueTx, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG_TEST, "Coda TX piena, pacchetto scartato");
    } else {
        ESP_LOGI(TAG_TEST, "Pacchetto inviato, lunghezza: %u", (unsigned)tt_pack_size);
    }

}

static void uart_rx_task(void *pvParameters) {
    uint8_t header[6];
    QueueHandle_t queueRx = (QueueHandle_t) pvParameters;

    while (1) {
        int header_len = uart_read_bytes(MIO_UART, header, 6, portMAX_DELAY);

		if (header[3] != 0x65)
		ESP_LOGI(TAG_R, "Header ricevuto (%d byte): %02X %02X %02X %02X %02X %02X",
					header_len, header[0], header[1], header[2], header[3], header[4], header[5]);

        if (header_len == 6) {
            uint8_t payload_len = header[4];

            if (payload_len > MAX_BUFF) {
                ESP_LOGE(TAG_R, "Payload troppo grande (%u), scarto", payload_len);
                uart_flush_input(MIO_UART);
                continue;
            }

            if (payload_len > 0) {
                size_t tt_pack_size = 6 + payload_len + 1;

                msg_t msg = {0};
                msg.len = tt_pack_size;
                memcpy(msg.buffer, header, 6);

                int bytes_read = uart_read_bytes(MIO_UART, msg.buffer + 6,
                                                 payload_len + 1,
                                                 pdMS_TO_TICKS(50));

                if (bytes_read == payload_len + 1) {

					if(header[3] == ENGINE || header[3] == MODE || header[3] == FLY_ON){
						xQueueOverwrite(queueRx, &msg);
						pck_lock = 1;
					}

					if(pck_lock == 0){
                    	xQueueOverwrite(queueRx, &msg);
					}
                } else {
                    ESP_LOGE(TAG_R, "Errore ricezione, pacchetto scartato");
                    uart_flush_input(MIO_UART);
                }
            }
        }
    }
}

void setting_uart_trx(QueueHandle_t queueRx, QueueHandle_t queueTx, QueueHandle_t queueHuart){
	
	uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

	uart_set_pin(UART_NUM_1,TX_TO_ST,ST_TO_RX,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);

	uart_driver_install(MIO_UART, BUFF_SIZE, BUFF_SIZE, 20, &queueHuart, 0);
	uart_param_config(MIO_UART, &uart_config);

	uart_pattern_queue_reset(MIO_UART, 20);

	xTaskCreate(uart_rx_task,"UART_RX", 2048, (void*)queueRx, 10, NULL);
	xTaskCreate(uart_tx_task,"UART_TX", 2048, (void*)queueTx, 9, NULL);
}