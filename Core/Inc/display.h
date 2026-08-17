#ifndef INC_DISPLAY_H_
#define INC_DISPLAY_H_

#include <stdint.h>
#include "SSD1306.h"
#include "globalVariable.h"

void layoutDisplay_pad(uint8_t *buffer);
void layoutDisplay_drone(uint8_t *buffer);
void updateValue(uint8_t *buffer, uint8_t riga, uint8_t dato);
void updateAll(DualsenseData dsd, uint8_t *stato, uint8_t *buffer);
void clearAll(uint8_t *buffer);

#endif /* INC_DISPLAY_H_ */
