#ifndef INC_ANALYZERDUALSENSE_H_
#define INC_ANALYZERDUALSENSE_H_

#include <stdint.h>
#include "globalVariable.h"

// -- Comandi drone opcode --
#define KEEP_ALIVE 100
#define ACK 101
#define ENGINE_ON 103
#define USE_MODE 105
#define FLY_ON 106

uint8_t packDualsenseData(DualsenseData *, size_t, uint8_t *); //Impacchetta il dato nella struct DualsenseData
uint8_t composePacket(DualsenseData *, DronePackSending *); //Traduce i comandi dal dualsense al formato pacchetto

#endif /* INC_ANALYZERDUALSENSE_H_ */
