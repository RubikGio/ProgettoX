#include "UDP_Connection.h"
#include "UART_drone.h"

void setting_socket(int *sock, sock_type type, struct sockaddr_in dest_addr,struct sockaddr_in local_addr){	

	*sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (*sock < 0) {
		ESP_LOGE("Socket setting", "Unable to create socket: errno %d", errno);
		return;
	}

	 // Abilita riuso della porta su entrambe
    int reuse = 1;
    setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(*sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));

    struct timeval timeout = { .tv_sec = 10, .tv_usec = 0 };
    setsockopt(*sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // Bind su local_addr per ENTRAMBE le socket
    int err = bind(*sock, (struct sockaddr *)&local_addr, sizeof(local_addr));
    if (err < 0) {
        ESP_LOGE("Socket setting", "Socket unable to bind: errno %d", errno);
        close(*sock);
        *sock = -1;
        return;
    }

	if (type == SOCK_SENDER) {
        ESP_LOGI("Socket setting", "TX pronta — src: 55555 → dst: %s:%d",
                 HOST_IP, ntohs(dest_addr.sin_port));
    } else {
        ESP_LOGI("Socket setting", "RX pronta — in ascolto su porta: %d",
                 ntohs(local_addr.sin_port));
    }
}

static msg_t take_data(QueueHandle_t queue){ //RX
	msg_t msg;
	msg.len = 0;
	if(xQueueReceive(queue,&msg,0) == pdPASS){
		if (msg.buffer[3] == ENGINE || msg.buffer[3] == MODE){
		ESP_LOGW("MESSAGGIO SEND", "messaggio in arrivo: %X", msg.buffer[3]); 
		pck_lock = 0;
		}
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
			if (msg.len > 0){
				/* for (size_t i = 0; i < msg.len; i++) {
				ESP_LOGI("SOCK_SEND: ","%02X ", msg.buffer[i]); 
			}	*/
			int err = sendto(sock, msg.buffer, msg.len, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err < 0) {
				ESP_LOGE("Sock send", "Error occurred during sending: errno %d", errno);
			} else {
				//ESP_LOGI("Sock send", "Message sent");
			}
		} else {
			ESP_LOGE("Sock send","Dato corrotto o coda vuota");
		}

		vTaskDelay(45);
	}
	
	if (sock != -1) {
		ESP_LOGE("Sock send", "Shutting down socket and restarting...");
		shutdown(sock, 0);
		close(sock);
	}

	vTaskDelete(NULL);

}

static void udp_recv_task(void *pvParameters){
	struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    socklen_t socklen = sizeof(source_addr);

	udp_datas_t *data = (udp_datas_t *)pvParameters;
	int sock = data->sock;
	QueueHandle_t queue = data->queue;

	while (1)
	{
		ESP_LOGI("Sock recv", "Waiting for data");

		uint8_t rx_buffer[MAX_UDP_RX_BUFFER];

		int len = recvfrom(sock, rx_buffer, MAX_UDP_RX_BUFFER, 0, (struct sockaddr *)&source_addr, &socklen);

		if (len < 0) {
			ESP_LOGE("Sock recv ", "recvfrom failed: errno %d", errno);
			continue;
			
		}else if (len > 0){
			msg_t msg;
			memcpy(msg.buffer, rx_buffer, len);
			msg.len = len;
			if (xQueueSend(queue, &msg, 0) == pdPASS){
				/* for (size_t i = 0; i < msg.len; i++ ){
				ESP_LOGI("Sock recv: ","%X",msg.buffer[i]); }*/
			}
			}
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
            udp_send_task,      // La funzione static
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
            udp_recv_task,      // La funzione static
            "udp_recv",         // Nome per il debug
            2048,               // Stack in byte
			(void*)dati_per_rcv, // puntatore ai dati di ricezione
            4,                  // Priorità
            NULL                // Handle
        );
        ESP_LOGI("UDP Init", "Task di ricezione avviato.");
    }
}