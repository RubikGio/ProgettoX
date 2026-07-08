#include "analyzerDualsense.h"
#include <string.h>
#include <stdlib.h>

static uint8_t flymode = 0;
static uint8_t engine = 0x00;

uint8_t packDualsenseData(DualsenseData *dualData, size_t data_len, uint8_t *dataRaw){

	if (data_len != 64 || dataRaw[0] != 1){
		return 1;
	}

	dualData->X_axis_left = dataRaw[1];
	dualData->Y_axis_left = dataRaw[2];
	dualData->X_axis_right = dataRaw[3];
	dualData->Y_axis_right = dataRaw[4];
	
	dualData->L2_trigger = dataRaw[5];
	dualData->R2_trigger = dataRaw[6];

	dualData->button_cross    = (dataRaw[8] >> 4) & 0x01;
	dualData->button_square   = (dataRaw[8] >> 5) & 0x01;
	dualData->button_circle   = (dataRaw[8] >> 6) & 0x01;
	dualData->button_triangle = (dataRaw[8] >> 7) & 0x01;

	dualData->L1_button = (dataRaw[9] >> 0) & 0x01;
	dualData->R1_button = (dataRaw[9] >> 1) & 0x01;
	dualData->L2_button = (dataRaw[9] >> 2) & 0x01;
	dualData->R2_button = (dataRaw[9] >> 3) & 0x01;

	dualData->battery = (dataRaw[53]) & 0x0F;

	return 0;
}

uint8_t composePacket(DualsenseData *dualData, DronePackSending *droneData){
	uint8_t comm[] = {dualData->button_triangle, dualData->button_square, dualData->button_circle, dualData->R1_button, 
					  dualData->L1_button};
	uint8_t opcode = KEEP_ALIVE;
	uint32_t payload = 0x01;

	uint8_t X_left = 0x00;
	uint8_t Y_left = 0x00;

	uint8_t X_right = 0x00;
	uint8_t Y_right = 0x00;

	uint16_t len = 0x0100;

	if (comm[0] == 0xFF){ //OP MODE
		opcode = USE_MODE;

		payload = 0x02;
		len = 0x0100;
		
	}
	else if(comm[1] == 0xFF){ // GPS MODE
		opcode = USE_MODE;

		payload = 0x01;
		len = 0x0100;
	}
	else if(comm[2] == 0xFF){ //ENGINE ON
		opcode = ENGINE_ON;
		len = 0x0100;
		if (flymode == 0){
			if (engine == 0x00){
				engine = 0x01;
			}
			else {
				engine = 0x00;
			}
		}
		payload = engine;
	}
	else if(comm[3] == 0xFF){ //FLY MODE ON
		opcode = FLY_ON;
		
		flymode = 1;

	}
	else if(comm[4] == 0xFF){ //FLY MODE OFF
		opcode = KEEP_ALIVE;
		flymode = 0;
	}
	else {
		return 0;
	}

	if ((flymode) == 1){
		X_left = translateAxisX(dualData->X_axis_left); // Destra / Sinistra movimento 
		X_right = translateAxisX(dualData->X_axis_right); // rotazione destra o sinistra 
		Y_left = translateAxisY(dualData->Y_axis_left);  // Avanti / Indietro  
		Y_right = translateAxisY(dualData->Y_axis_right); // DAI GASSS --> altitudine 

		opcode = FLY_ON;

		len = 0x0400;
		
		payload = (Y_left << 24) | (X_left << 16) | (Y_right << 8) | X_right;
	}

	droneData->lenght = len;
	droneData->opcode = opcode;
	droneData->payload = payload;

	return 1;

}	
static uint8_t translateAxisX(uint8_t X){
	if(X > 0x7A && X < 0x86){ //confronto tra 122 e 134
		return 0x80;
	}
	return X;
}

static uint8_t translateAxisY(uint8_t Y){
	if(Y > 0x7A && Y < 0x86){ //confronto tra 122 e 134
		return 0x80;
	}
	return Y;
}

