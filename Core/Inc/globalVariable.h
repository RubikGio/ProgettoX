#ifndef INC_GLOBALVARIABLE_H_
#define INC_GLOBALVARIABLE_H_

#include <stdio.h> 
#include <stdint.h>

static const unsigned char id_send[] = {0x46, 0x48, 0x3C};
static const unsigned char id_recv[] = {0x46, 0x48, 0x3E};

typedef struct{
	// sticks 
	uint8_t X_axis_left; 
	uint8_t X_axis_right;
	/*
		0x00 full left
		0xff full right 
		~ 0x80 neutral position
	*/

	uint8_t Y_axis_left;
	uint8_t Y_axis_right;
	/*
		0x00 full up
		0xff full down 
		~ 0x80 neutral position
	*/

	// triggers L2 and R2 
	uint8_t L2_trigger;
	uint8_t R2_trigger;
	/*
		0x00 neutral
		0xFF pressed position
	*/

	// Buttons 
	uint8_t button_cross;
	uint8_t button_square;
	uint8_t button_circle;
	uint8_t button_triangle;

	// Buttons trigger
	uint8_t L1_button;
	uint8_t R1_button;
	uint8_t L2_button; // check pressed or not pressed
	uint8_t R2_button; // check pressed or not pressed

	// Battery state 
	uint8_t battery; //moltiplicare valore estratto per 10

}DualsenseData;

typedef struct {
	// operation code
	uint8_t opcode;

	// payload lenght
	uint16_t lenght;

	// payload
	uint8_t *payload;

	// checksum
	uint8_t checksum;

}DronePackSending;

typedef struct {
	// operation code
	uint8_t opcode;

	// payload lenght
	uint8_t lenght[2];

	// roll left and roll right 
	uint8_t rolls[2];

	// pitch forward and pitch backward
	uint8_t pitches[2];

	// yaw or rotation
	uint8_t yaw[2];

	// speed
	uint8_t speed[2];

	// use mode
	uint8_t mode; //primi 4 bit 
	
	// engine armed or not
	uint8_t engine; //ultimi 4 bit

	//GPS information
	uint8_t *GPS; // Da testare ancora sul campo per capire come funziona

	// battery state
	uint8_t battery; // Da testare ancora sul campo per capire come funziona

}DronePackRecieve;



#endif /* INC_GLOBALVARIABLE_H_ */
