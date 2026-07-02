#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

#include "UART_drone.h"
#include "UDP_Connection.h"

#define MIO_UART UART_NUM_1
#define TX_TO_ST 5
#define ST_TO_RX 4

#define BUFF_SIZE 1024

const char* TAG_R = "UART RX:";
const char* TAG_T = "UART TX:";
static const char *TAG_TEST = "UART TEST";


static void uart_tx_task(void *pvParameters){
	QueueHandle_t queueTx = (QueueHandle_t) pvParameters;
	msg_t msg;
	while (1)
	{	
		if(xQueueReceive(queueTx,&msg,portMAX_DELAY) == pdPASS){
			ESP_LOGI(TAG_T, "Prelevato pacchetto dalla coda! Lunghezza: %u byte", (unsigned) msg.lenght);
			
			int bytes_sended = uart_write_bytes(MIO_UART, (const char *) msg.data, msg.lenght);

			if(bytes_sended != msg.lenght){
				ESP_LOGW(TAG_T, "Attenzione: inviati solo %d byte su %u", bytes_sended, (unsigned) msg.lenght);
			}

			free(msg.data);
		}
			
	}
}

void invio_pacchetto_test(QueueHandle_t queueTx){
    // Pacchetto di test: header (id_send 2 byte) + opcode + length(2) + payload + checksum
    uint8_t opcode = 0x01;
    uint8_t payload_bytes[3] = {0x02, 0x02, 0x02};
    uint16_t payload_len = sizeof(payload_bytes);

    size_t tt_pack_size = 6 + payload_len + 1;  // stesso schema che usi in uart_rx_task

    uint8_t *buff = malloc(tt_pack_size);
    if (buff == NULL) {
        ESP_LOGE(TAG_TEST, "Malloc fallita per pacchetto di test");
        return;
    }

    size_t idx = 0;
    buff[idx++] = 0x00;   // id_send[0]
    buff[idx++] = 0x00;   // id_send[1]
    buff[idx++] = opcode;
    buff[idx++] = (uint8_t)(payload_len & 0xFF);        // LSB
    buff[idx++] = (uint8_t)(payload_len >> 8);           // MSB
    memcpy(&buff[idx], payload_bytes, payload_len);
    idx += payload_len;

    // checksum XOR sul payload (coerente con checkSum lato STM32)
    uint8_t check = 0;
    for (size_t i = 0; i < payload_len; i++) {
        check ^= payload_bytes[i];
    }
    buff[idx++] = check;

    msg_t msg;
    msg.data = buff;
    msg.lenght = tt_pack_size;

    if (xQueueSend(queueTx, &msg, pdMS_TO_TICKS(100)) != pdPASS) {
        ESP_LOGW(TAG_TEST, "Coda TX piena, pacchetto di test scartato");
        free(buff);
    } else {
        ESP_LOGI(TAG_TEST, "Pacchetto di test inviato in coda, lunghezza: %u", (unsigned)tt_pack_size);
    }
}

static void uart_rx_task(void *pvParameters){
	uint8_t header[6];
	QueueHandle_t queueRx = (QueueHandle_t) pvParameters;

	while (1)
	{
		int header_len = uart_read_bytes(MIO_UART, header, 6, portMAX_DELAY);

		if (header_len == 6){
			uint8_t payload = header[4];

			if (payload > 0){
				size_t tt_pack_size = 6 + payload + 1;

				uint8_t *full_pack = malloc(tt_pack_size);

				if(full_pack != NULL){
					memcpy(full_pack,header,6);

					int bytes_read = uart_read_bytes(MIO_UART,full_pack + 6, payload, pdMS_TO_TICKS(50));

					if (bytes_read == payload){
						msg_t msg;
						msg.data = full_pack;
						msg.lenght = tt_pack_size;

						if (xQueueSend(queueRx, &msg, 0) == pdPASS)
						{
							ESP_LOGI(TAG_R,"Invio dati per coda UDP: %p", (void*)full_pack);
						}
						else{
							ESP_LOGW(TAG_R,"Coda UDP piena!!");
							free(full_pack);
						}
					}else{
						ESP_LOGE(TAG_R,"Errore dato da buttare (interruzione comunicazione)");
						free(full_pack);
						uart_flush_input(MIO_UART);
					}
				}
			}
		}

	}
	
	
}

void setting_uart_trx(QueueHandle_t queueRx, QueueHandle_t queueTx){
	
	uart_config_t uart_config = {
        .baud_rate = 96000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

	uart_set_pin(UART_NUM_1,TX_TO_ST,ST_TO_RX,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);

	uart_driver_install(MIO_UART, BUFF_SIZE, BUFF_SIZE, 20, &queueRx, 0);
	uart_param_config(MIO_UART, &uart_config);

	uart_pattern_queue_reset(MIO_UART, 20);

	xTaskCreate(uart_rx_task,"UART_RX", 2048, (void*)queueRx, 10, NULL);
	xTaskCreate(uart_tx_task,"UART_TX", 2048, (void*)queueTx, 9, NULL);
}