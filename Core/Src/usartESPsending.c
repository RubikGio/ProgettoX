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
	uint8_t len = 0x00;

	if (dps == NULL || outBuff == NULL || dps->payload == NULL){
		return 0;
	}

	for (size_t i = 0; i < 3; i++){
		outBuff[index++] = id_send[i];
	}
	outBuff[index++] = dps->opcode;
	
	outBuff[index++] =  (uint8_t)((dps->lenght >> 8) & 0xFF);
	outBuff[index++] = 0x00; // --> sono 2 byte il campo lenght ma il secondo sempre 0
	len = (uint8_t)(dps->lenght >> 8);

	for(size_t i = 0; i < len; i++){
		outBuff[index++] = (uint8_t)(dps->payload[i]);
	}

	outBuff[index] = checksum(&outBuff[3], index - 3);
	index++;

	return index;
}

