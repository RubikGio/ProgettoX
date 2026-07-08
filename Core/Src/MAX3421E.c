#include "MAX3421E.h"

static void max3421_cs_low() { 
	HAL_GPIO_WritePin(MAX3421_CS_PORT, MAX3421_CS_PIN, GPIO_PIN_RESET); 
}

static void max3421_cs_high() { 
	HAL_GPIO_WritePin(MAX3421_CS_PORT, MAX3421_CS_PIN, GPIO_PIN_SET); 
}

uint8_t max3421_readReg(uint8_t reg, SPI_HandleTypeDef *hspi1){
	uint8_t cmd = (reg << 3);
    uint8_t rx = 0;

    max3421_cs_low();
    HAL_SPI_Transmit(hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(hspi1, &rx, 1, HAL_MAX_DELAY);
    max3421_cs_high();

    return rx;
}

void max3421_writeReg(uint8_t reg, uint8_t data, SPI_HandleTypeDef *hspi1){
	uint8_t cmd = (reg << 3) | 0x02;   // direzione 1 = write

    max3421_cs_low();
    HAL_SPI_Transmit(hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi1, &data, 1, HAL_MAX_DELAY);
    max3421_cs_high();
}

void max3421_readFIFO(uint8_t reg, uint8_t *buff, uint8_t len, SPI_HandleTypeDef *hspi1){
	uint8_t cmd = (reg << 3);

    max3421_cs_low();
    HAL_SPI_Transmit(hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(hspi1, buff, len, HAL_MAX_DELAY);
    max3421_cs_high();
}
void max3421_writeFIFO(uint8_t reg, const uint8_t *buff, uint8_t len, SPI_HandleTypeDef *hspi1){
	uint8_t cmd = (reg << 3) | 0x02;

    max3421_cs_low();
    HAL_SPI_Transmit(hspi1, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(hspi1, (uint8_t*)buff, len, HAL_MAX_DELAY);
    max3421_cs_high();
}

void max3421_init(SPI_HandleTypeDef *hspi1){
	max3421_writeReg(USBCTL,bmCHIPRES,hspi1);
	HAL_Delay(10);
	max3421_writeReg(USBCTL,ALL_BIT_0,hspi1);

	uint8_t reg = 0x00;
	while (!(reg & bmOSCOKIRQ)){
		reg = max3421_readReg(USBIRQ,hspi1);
	}

	// MOD comportamento da host 
	max3421_writeReg(MODE,(bmHOST | bmDMPULLDN | bmDPPULLDN),hspi1);

	// INT attiva
	max3421_writeReg(HIEN, (bmRCVDAVIE | bmCONDETIE), hspi1);

	max3421_writeReg(CPUCTL, bmIE, hspi1);

	detectDevice(&hspi1);
	enumerateDevice(&hspi1);

}

void detectDevice(SPI_HandleTypeDef *hspi1){
	int busstate;

	max3421_writeReg(HIRQ, bmCONDETIRQ, hspi1);

	do
	{
		max3421_writeReg(HCTL, bmSAMPLEBUS, hspi1);
		busstate = max3421_readReg(HRSL,hspi1);
		busstate &= (bmJSTATUS | bmKSTATUS);
	} while (busstate == 0);
	switch (busstate)
	{
	case bmJSTATUS:
		max3421_writeReg(MODE, (bmDMPULLDN | bmDPPULLDN | bmHOST | bmSOFKAENAB), hspi1);
		break;
	case bmKSTATUS:
		max3421_writeReg(MODE, (bmDPPULLDN | bmDMPULLDN | bmHOST | bmLOWSPEED), hspi1);
	default:
		break;
	}

}

void enumerateDevice(SPI_HandleTypeDef *hspi1){

	// Mancherebbe il get_descriptor ma sapendo di avere un dulasense come periferica non mi serve

	static uint8_t set_address_1[8] = {0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
	static uint8_t set_config_1[8] = {0x00, 0x09, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

	max3421_writeReg(HCTL, bmBUSRST, hspi1);
	while (max3421_readReg(HCTL, hspi1) & bmBUSRST);
	HAL_Delay(300);

	max3421_writeReg(PERADDR, 0, hspi1);
	
	
	max3421_writeFIFO(SUDFIFO, set_address_1, 8, hspi1); // riempio la coda con 8 byte 
	max3421_writeReg(HXFR, bmSETUP, hspi1); // Lancio il comando di setup sull'endpoint 0
	while ((max3421_readReg(HIRQ, hspi1) & bmHXFRDNIRQ) == 0 ); // in pratica attendo che finisco di inviare fisicamente i dati sul cavo
	max3421_writeReg(HIRQ, bmHXFRDNIRQ, hspi1); // lo sto rimettendo ad 1 immagino
	max3421_writeReg(HXFR, bmHS, hspi1); // in pratica sto inviando un pacchetto di tipo IN con handshake
	while((max3421_readReg(HIRQ, hspi1) & bmHXFRDNIRQ) == 0); // Attendo come prima che finsice di inviare 
	max3421_writeReg(HIRQ, bmHXFRDNIRQ, hspi1);

	
	max3421_writeReg(PERADDR, 1, hspi1); // Sto mettendo come "obbligo" di parlare con l'indirizzo 1

	max3421_writeFIFO(SUDFIFO, set_config_1, 8, hspi1); // riempio la coda con 8 byte 
	max3421_writeReg(HXFR, bmSETUP, hspi1); // Lancio il comando di setup sull'endpoint 0
	while ((max3421_readReg(HIRQ, hspi1) & bmHXFRDNIRQ) == 0 ); // in pratica attendo che finisco di inviare fisicamente i dati sul cavo
	max3421_writeReg(HIRQ, bmHXFRDNIRQ, hspi1); // lo sto rimettendo ad 1 immagino
	max3421_writeReg(HXFR, bmHS, hspi1); // in pratica sto inviando un pacchetto di tipo IN con handshake
	while((max3421_readReg(HIRQ, hspi1) & bmHXFRDNIRQ) == 0); // Attendo come prima che finsice di inviare 
	max3421_writeReg(HIRQ, bmHXFRDNIRQ, hspi1);

}

void startUSB_request(SPI_HandleTypeDef *hspi1){
	max3421_writeReg(HXFR, (ALL_BIT_0 | EP_DUALSENSE), hspi1);
}
