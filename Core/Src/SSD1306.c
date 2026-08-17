#include "SSD1306.h"
#include "font.h"
#include <string.h>

static uint8_t reset_cursor[] = {
    0x00, 0x21, 0x00, 0x7F,
    0x22, 0x00, 0x07
};

void UpdateCursor_SSD1306(I2C_HandleTypeDef *hi2ci, uint8_t *buffer, uint8_t *dma_buffer, volatile uint8_t *sync_dma){
	*sync_dma = SSD_RESET;

	dma_buffer[0] = 0x40;
    memcpy(&dma_buffer[1],buffer,1024);

	HAL_I2C_Master_Transmit_DMA(hi2ci, SSD1306_I2C_ADDR, reset_cursor, sizeof(reset_cursor));
}

void Init_SSD1306(I2C_HandleTypeDef *hi2c1){
	uint8_t seq_init[] = {
			MUX_RATIO, 0x3F,
			DISPLAY_OFFSET, NO_OFFSET,
			MEMADD_MODE, MODE_HORIZZONTAL,
			START_LINE,
			SEGMENT_REMAP,
			COM,
			COM_PIN, 0x12,
			CONTRAST_CONTROL, 0x7F,
			OUT_FW_RAM,
			NORM_DISPLAY,
			CLK_DIVIDE, 0x80,
			CHARGE_PUMP, EN_PUMP,
			DISPLAY_ON};
	
	HAL_I2C_Mem_Write(hi2c1, SSD1306_I2C_ADDR, 0x00, 1, seq_init, sizeof(seq_init), HAL_MAX_DELAY);

}

void Char_SSD1306(uint8_t riga, uint8_t colonna, uint8_t *buffer, char lettera){
	uint8_t indice_font = lettera - 32;
	uint16_t posizione = (riga * 128) + colonna;

	if (lettera < 32 || lettera >127){
		lettera = '?';
	}

	if(riga > 7 || colonna > 122) return;


	for(short int i = 0; i < CHARS_COLS_LENGTH; i++){
		buffer[posizione + i] = FONTS[indice_font][i];
	}
	buffer[posizione + 5] = 0x00; // Ultima colonna lasciamo a 0
}

void String_SSD1306(uint8_t riga, uint8_t colonna, uint8_t *buffer, const char *stringa){
	short int i = 0;
	while (stringa[i] != '\0')
	{
		if((colonna + 6) > 128){
			break;
		}
		Char_SSD1306(riga,colonna + (i*6), buffer, stringa[i]);
		i++;
	}
	
}
void ClearArea_SSD1306(uint8_t *buffer, uint8_t riga, uint8_t colonna, uint8_t larghezza){
	uint16_t posizione;

	posizione = (riga * SSD1306_WIDTH) + colonna;

	for (uint8_t i = 0; i < larghezza; i++)
	{
		buffer[posizione + i] = 0x00;
	}
}

