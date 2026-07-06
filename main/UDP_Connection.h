#ifndef UDP_CONNECTION_H
#define UDP_CONNECTION_H

#include "lwip/sockets.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_IP "10.253.101.147"
#define PORT 3456
#define MAX_UDP_RX_BUFFER 64

typedef enum {
    SOCK_RECEIVER,
    SOCK_SENDER
} sock_type;

typedef struct {
    struct sockaddr_in dest;
    int sock;
	QueueHandle_t queue;
} udp_datas_t;

void setting_socket(int *sock, sock_type type, struct sockaddr_in dest_addr);
void udp_start_tasks(udp_datas_t *send_data, udp_datas_t *recv_data);

#ifdef __cplusplus
}
#endif

#endif // UDP_CONNECTION_H