#include "LED_gpio.h"

#define LED_STRIP_GPIO 27
#define LED_NUMBERS 1 

static led_strip_handle_t led_strip;

static void gestione_led(void *pvParameters){
	sys_state *state = (sys_state *)pvParameters;
	bool led_attivo = false;
	while (1)
	{	
		switch (*state)
		{
			case SYSTEM_IDLE:
				led_strip_set_pixel(led_strip, 0, 255, 0, 0);
				led_strip_refresh(led_strip);
				break;
			case SYSTEM_CONNECTING:
				if (led_attivo) led_strip_set_pixel(led_strip, 0, 0, 127, 255);
				else led_strip_clear(led_strip);
				led_attivo = !led_attivo;
				led_strip_refresh(led_strip);
				break;
			case SYSTEM_CONNECTED:
				led_strip_set_pixel(led_strip, 0, 0, 255, 0);
				led_strip_refresh(led_strip);
				break;
		}
	vTaskDelay(pdMS_TO_TICKS(500));
	}

}

void configura_led(sys_state *current_state){

	ESP_LOGI("LED","Configurazione dell'unico led...");

	led_strip_config_t led_conf = {
		.strip_gpio_num = LED_STRIP_GPIO,
		.max_leds = LED_NUMBERS,
		.led_pixel_format = LED_PIXEL_FORMAT_GRB,
		.led_model = LED_MODEL_WS2812
	};

	led_strip_rmt_config_t rmt_conf = {
		.resolution_hz = 10 * 1000 * 1000, // --> risoluzione a 10MHz
		.flags = { .with_dma = false } // --> identifica l'uso del DMA
	};

	ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_conf, &rmt_conf, &led_strip));

	led_strip_clear(led_strip);

	xTaskCreate(gestione_led, "Gestione LED", 1024, current_state, 3, NULL);
}