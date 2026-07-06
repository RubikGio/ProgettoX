#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "LED_gpio.h"
#include "UDP_Connection.h"
#include "UART_drone.h"

#define EXAMPLE_ESP_MAXIMUM_RETRY  5

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "wifi station";
static int s_retry_num = 0;
static sys_state current_state = SYSTEM_IDLE; // Reso accessibile al task LED
static bool udp_tasks_created = false;
static QueueHandle_t queueRx;
static QueueHandle_t queueTx;
static QueueHandle_t queueHuart;

static void event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if(event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START){
        current_state = SYSTEM_CONNECTING;
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED){
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY){
            current_state = SYSTEM_IDLE;
            vTaskDelay(pdMS_TO_TICKS(100));
            current_state = SYSTEM_CONNECTING;
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Tentativo di riconessione all'AP");
        }else{
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"Connesione fallita all'AP");
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP ottenuto:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        current_state = SYSTEM_CONNECTED;
        
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_connection(void){
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    esp_netif_create_default_wifi_sta(); 

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "EccomiQua",
            .password = "giovanni",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}   

void app_main(void)
{
    configura_led(&current_state); 

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_connection();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT){
        ESP_LOGI(TAG, "Connessione stabilita. Avvio stack UDP...");
    		current_state = SYSTEM_CONNECTED;

        queueRx = xQueueCreate(70, sizeof(msg_t));
        queueTx = xQueueCreate(70, sizeof(msg_t));
		queueHuart = xQueueCreate(30, sizeof(msg_t));

        if (queueRx == NULL || queueTx == NULL) {
            ESP_LOGE(TAG, "Errore nella creazione delle code UART");
            return;
        }

        if (!udp_tasks_created) {
            int sock_send = 0;
            int sock_recv = 0;

            struct sockaddr_in dest_addr;
            dest_addr.sin_family = AF_INET;
            dest_addr.sin_port = htons(3456);
            dest_addr.sin_addr.s_addr = inet_addr(HOST_IP);     

            struct sockaddr_in local_addr;
            local_addr.sin_family = AF_INET;
            local_addr.sin_port = htons(3456);
            local_addr.sin_addr.s_addr = htonl(INADDR_ANY);

            setting_socket(&sock_send, SOCK_SENDER, dest_addr);
            setting_socket(&sock_recv, SOCK_RECEIVER, local_addr);

            // Allocazione memoria
            udp_datas_t *args_send = malloc(sizeof(udp_datas_t));
			udp_datas_t *args_recv = malloc(sizeof(udp_datas_t));
            if (args_send != NULL && args_recv != NULL) {
                args_send->dest = dest_addr;
                args_send->sock = sock_send;
				args_send->queue = queueRx;

				args_recv->sock = sock_recv;
				args_recv->queue = queueTx;

                udp_start_tasks(args_send, args_recv);
				vTaskDelay(pdMS_TO_TICKS(1000));
				setting_uart_trx(queueRx, queueTx, queueHuart);
                udp_tasks_created = true;
				while (1){
					invio_pacchetto_test(queueTx);
					vTaskDelay(pdMS_TO_TICKS(2000));
				}
            } else {
                ESP_LOGE(TAG, "Errore di allocazione memoria per UDP");
            }
        }
    } else if (bits & WIFI_FAIL_BIT){
        ESP_LOGE(TAG, "Connessione fallita. Impossibile avviare UDP.");
    }
}