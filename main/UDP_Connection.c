#include "UDP_Connection.h"
#include "UART_drone.h"

void setting_socket(int *sock, sock_type type, struct sockaddr_in dest_addr){	

	*sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (*sock < 0) {
		ESP_LOGE("Socket setting", "Unable to create socket: errno %d", errno);
		return;
	}

	// Set timeout
	struct timeval timeout;
	timeout.tv_sec = 10;
	timeout.tv_usec = 0;
	setsockopt (*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

	if (type == SOCK_RECEIVER){
		int err = bind(*sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err < 0) {
					ESP_LOGE("Socket setting", "Socket unable to bind: errno %d", errno);
				}
			ESP_LOGI("Socekt setting","Binding succesfull: %d", *sock);
		}
	
	ESP_LOGI("Sock setting", "Socket created, sending to %s:%d", HOST_IP, PORT);

}

static msg_t take_data(QueueHandle_t queue){ //RX
	msg_t msg;
	msg.data = NULL;
	msg.lenght = 0;
	if(xQueueReceive(queue,&msg,portMAX_DELAY) == pdPASS){
		return msg;
	}
	return msg;
}

static void udp_send_task(void *pvParameters){
	udp_datas_t * data = (udp_datas_t *)pvParameters;
	int sock = data->sock;
	struct sockaddr_in dest_addr = data->dest; 
	QueueHandle_t queue = data->queue;

	while (1) {
		msg_t msg = take_data(queue);
		if (msg.data != NULL && msg.lenght > 0){
			int err = sendto(sock, msg.data, msg.lenght, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err < 0) {
				ESP_LOGE("Sock send", "Error occurred during sending: errno %d", errno);
			} else {
				ESP_LOGI("Sock send", "Message sent");
			}

			free(msg.data);
		} else {
			ESP_LOGE("Sock send","Dato corrotto o coda vuota");
		}

		vTaskDelay(pdMS_TO_TICKS(400));
	}
	
	if (sock != -1) {
		ESP_LOGE("Sock send", "Shutting down socket and restarting...");
		shutdown(sock, 0);
		close(sock);
	}

	free(data);
	vTaskDelete(NULL);

}

static void udp_recv_task(void *pvParameters){
	struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t socklen = sizeof(source_addr);

	udp_datas_t *data = (udp_datas_t *)pvParameters;
	int sock = data->sock;
	QueueHandle_t queue = data->queue;

	msg_t msg;

	while (1)
	{
		ESP_LOGI("Sock recv", "Waiting for data");

		int len = recvfrom(sock, msg.data, msg.lenght - 1, 0, (struct sockaddr *)&source_addr, &socklen);

		if (len < 0) {
			ESP_LOGE("Sock recv ", "recvfrom failed: errno %d", errno);
			continue;
			
		}else if (xQueueSend(queue,&msg,0) == pdPASS){
			ESP_LOGI("Sock recv","Data arrived con pacchetto: %d",msg.data);
		}
		
		free(msg.data);

		if (source_addr.ss_family == AF_INET) {
			struct sockaddr_in *source = (struct sockaddr_in *)&source_addr;
			ESP_LOGI("Sock recv", "Source %s:%d",
					inet_ntoa(source->sin_addr), ntohs(source->sin_port));
		}

	}

	if (sock != -1) {
            ESP_LOGE("Sock recv", "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
	
    vTaskDelete(NULL);
	
}

void udp_start_tasks(udp_datas_t *dati_per_send, udp_datas_t *dati_per_rcv) {

	if (dati_per_send != NULL) {
        xTaskCreate(
            udp_send_task,      // La tua funzione static
            "udp_send",         // Nome per il debug
            2048,               // Stack in byte
            (void*)dati_per_send, // Parametri passati al task
            5,                  // Priorità
            NULL                // Handle
        );
        ESP_LOGI("UDP Init", "Task di invio avviato.");
    }

    if (dati_per_rcv != NULL) {
        xTaskCreate(
            udp_recv_task,      // La tua funzione static
            "udp_recv",         // Nome per il debug
            2048,               // Stack in byte
			(void*)dati_per_rcv, // Passiamo il puntatore ai dati di ricezione
            4,                  // Priorità
            NULL                // Handle
        );
        ESP_LOGI("UDP Init", "Task di ricezione avviato.");
    }
}