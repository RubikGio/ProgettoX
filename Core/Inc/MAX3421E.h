#ifndef INC_MAX3421E_H_
#define INC_MAX3421E_H_

#include <stdint.h>

// -- REGISTRI DEL CHIP MAX3421E -- // 
#define RCVFIFO 1
#define SNDFIFO 2
#define SUDFIFO 4
#define RCVBC 6
#define SNDBC 7
#define USBIRQ 13
#define USBIEN 14
#define USBCTL 15
#define CPUCTL 16
#define PINCTL 17
#define REVISION 18
#define HIRQ 25
#define HIEN 26
#define MODE 27
#define PERADDR 28
#define HCTL 29
#define HXFR 30
#define HRSL 31
// -- COMANDI DEL CHIP MAX3421E -- // 

#define ALL_BIT_0 0x00

// USBIRQ
#define bmOSCOKIRQ 0x01

// USBCTL
#define bmCHIPRES 0x20

// MODE
#define bmDPPULLDN 0x80
#define bmDMPULLDN 0x40
#define bmHOST 0x01
#define bmSOFKAENAB 0x08
#define bmLOWSPEED 0x02

// CPUCTL
#define bmIE 0x01

// HIRQ/HIEN 
#define bmCONDETIRQ 0x02
#define bmFRAMEIRQ 0x40
#define bmHXFRDNIRQ 0x80

// HCTL
#define bmBUSRST 0x01
#define bmSAMPLEBUS 0x04
#define bmSIGRSM 0x40

// HRSL
#define bmJSTATUS 0x80
#define bmKSTATUS 0x40
#define SUCCESS 0x00 // stauts exit HSRL register 

// HXFR
#define bmSETUP 0x10
#define bmHS 0x80
#define EP_DUALSENSE 0x01

// HIEN
#define bmHIEN 0x04
#define bmCONDETIE 0x20


// Pin che cambia lo stato del chip select o anche meglio quello collegato al pin SS
#define MAX3421_CS_PORT GPIOC 
#define MAX3421_CS_PIN GPIO_PIN_13 

uint8_t max3421_readReg(uint8_t, SPI_HandleTypeDef *);
void max3421_writeReg(uint8_t,uint8_t, SPI_HandleTypeDef *);
void max3421_readFIFO(uint8_t, uint8_t *, uint8_t, SPI_HandleTypeDef *);
void max3421_writeFIFO(uint8_t, const uint8_t *, uint8_t, SPI_HandleTypeDef *);
void max3421_init(SPI_HandleTypeDef *);
void detectDevice(SPI_HandleTypeDef *);
void enumerateDevice(SPI_HandleTypeDef *);
void startUSB_request(SPI_HandleTypeDef *);

/* Operazioni da fare 
1. Impostare il bit chipset da 15(register) pari a 1 e poi riporti a 0 per resettare il chip
2. Leggere da (registro 13) OSCOKIRQ bit fin quando non è pari a 1
3. Cambia la mode di attività nel registro MODE (register 27) e nel bit di posizione 0 mettici 1 
4. Cambiare i bit DMPULLDN e DPPULLDN e metterli pari a 1 dal registro MODE 
*/

#endif
