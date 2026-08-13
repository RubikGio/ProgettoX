#include "display.h"
#include <string.h>

static uint8_t op_mode = 0;
static uint8_t engine = 0;

static const char *pushButtonState(uint8_t dato, uint8_t *state){
	if (*state == 0){
		if (dato == 0xFF){
			*state = 1;
			return "ON";
		}
		return "OFF";
	}

	if (dato == 0xFF){
		*state = 0;
		return "OFF";
	}

	return "ON";
}

void layoutDisplay_pad(uint8_t *buffer){
	String_SSD1306(0,0,buffer,"MOVE A/I:");
	String_SSD1306(1,0,buffer,"MOVE D/S:");
	String_SSD1306(2,0,buffer,"ALTEZZA:");
	String_SSD1306(3,0,buffer,"ROTATION:");
	String_SSD1306(4,0,buffer,"ENGINE:");
	String_SSD1306(5,0,buffer,"FLY BTN:");
	String_SSD1306(6,0,buffer,"OP MODE:");
}

void updateValue(uint8_t *buffer, uint8_t riga, uint8_t dato){
	uint8_t colonna = 80;
	uint8_t larghezza = SSD1306_WIDTH - colonna;
	char stringa[8];
	const char *stato;
	
	ClearArea_SSD1306(buffer,riga,colonna,larghezza);

	if (riga == 4){
		stato = pushButtonState(dato, &engine);
		String_SSD1306(riga,colonna,buffer,stato);
		return;
	}
	if (riga == 6){
		stato = pushButtonState(dato, &op_mode);
		String_SSD1306(riga,colonna,buffer,stato);
		return;
	}

	snprintf(stringa,sizeof(stringa),"%u",dato);
	String_SSD1306(riga,colonna,buffer,stringa);
}

void updateAll(DualsenseData dsd, uint8_t *stato, uint8_t *buffer){
	if (*stato == 0){
			if (dsd.R1_button == 0xFF){
				*stato = 0xFF;
			}
		}
		if (*stato == 0xFF){
			if (dsd.L1_button == 0xFF){
				*stato = 0x00;
			}
		}

	updateValue(buffer,0,dsd.Y_axis_left);
	updateValue(buffer,1,dsd.X_axis_left);
	updateValue(buffer,2,dsd.Y_axis_right);
	updateValue(buffer,3,dsd.X_axis_right);
	updateValue(buffer,4,dsd.button_circle);
	updateValue(buffer,5,*stato);
	updateValue(buffer,6,dsd.button_triangle);
}