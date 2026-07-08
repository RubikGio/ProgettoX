#include "usartESPreceiving.h"
#include <string.h>

uint8_t deserializePacket(uint8_t *dataRaw, DronePackRecieve *dpr){
	
	uint8_t id [] = { dataRaw[0], dataRaw[1], dataRaw[2]};

	if (memcmp(id,id_recv,3) != 0){
		return 1;
	}

	dpr->opcode = dataRaw[3];

	for (size_t i = 0; i < 2; i++)
		{
			dpr->lenght[i] = dataRaw[4 + i];
			dpr->rolls[i] = dataRaw[6 + i];
			dpr->pitches[i] = dataRaw[8 + i];
			dpr->yaw[i] = dataRaw[10 + i];
			dpr->speed[i] = dataRaw[12 + i];
		}
		
	dpr->mode = (dataRaw[14] >> 4) & 0x0F;
	dpr->engine = dataRaw[14] & 0x0F;

	//GPS serialization miss

	return 0;

}
