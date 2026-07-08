#include "usartESPsending.h"

static uint8_t checksum(const uint8_t *payload, uint16_t lenght){
	uint8_t check = 0;

	for(size_t i = 0; i < lenght; i++){
		check ^= payload[i];
	}

	return check;
}

uint16_t serializePacket(const DronePackSending *dps, uint8_t *outBuff){
	uint16_t index = 0;

	if (dps == NULL || outBuff == NULL || dps->payload == NULL){
		return 0;
	}

	for (size_t i = 0; i < 3; i++){
		outBuff[index++] = id_send[i];
	}
	outBuff[index++] = dps->opcode;
	
	outBuff[index++] =  (uint8_t)(dps->lenght & 0xFF);
	outBuff[index++] =  (uint8_t)(dps->lenght >> 8);

	for(size_t i = 0; i < dps->lenght; i++){
		outBuff[index++] = (uint8_t)(dps->payload[i]);
	}

	outBuff[index] = checksum(&outBuff[6], index - 6);
	index++;

	return index;
}

