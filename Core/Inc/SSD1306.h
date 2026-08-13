#ifndef INC_SSD1306_H_
#define INC_SSD1306_H_

#include <stdint.h>
#include "stm32f3xx_hal.h"

// -- COMANDI --
#define SSD1306_I2C_ADDR   0x3C << 1   // indirizzo tipico 0x3C, shiftato per la HAL
#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     64
#define SSD1306_PAGES      (SSD1306_HEIGHT / 8)

#define DISPLAY_OFF 0xAE
#define MEMADD_MODE 0x20
#define MODE_HORIZZONTAL 0x00
#define STARTPAGE 0xB0
#define COM 0xC8
#define COM_PIN 0xDA
#define LOW_COLUMN 0x00
#define HIGH_COLUMN 0x10
#define START_LINE 0x40
#define CONTRAST_CONTROL 0x81
#define SEGMENT_REMAP 0xA1
#define NORM_DISPLAY 0xA6
#define MUX_RATIO 0xA8
#define DUTY_CYCLE 0x3F
#define OUT_FW_RAM 0xA4
#define DISPLAY_OFFSET 0xD3
#define NO_OFFSET 0x00
#define CLK_DIVIDE 0xD5
#define DIVIDE_RATIO 0xF0
#define CHARGE_PUMP 0x8D
#define EN_PUMP 0x14
#define DISPLAY_ON 0xAF

void Init_SSD1306(I2C_HandleTypeDef *);
void WriteReg_SSD1306(I2C_HandleTypeDef *, uint8_t);
void UpdateCursor_SSD1306(I2C_HandleTypeDef *, uint8_t *);
void ClearArea_SSD1306(uint8_t *, uint8_t, uint8_t, uint8_t);
void String_SSD1306(uint8_t, uint8_t, uint8_t *,const char *);
void Char_SSD1306(uint8_t, uint8_t, uint8_t *,char);

// È diviso in colonne la scrittura e in righe
// Verticale ne ha di 8 pixel ognuna
// Orizzontale ne ha di 8 righe totali 

#endif /* INC_SSD1306_H_ */
