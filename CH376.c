#include "CH376.h"

static unsigned usb_state = 0;
static uint8_t cfg_buffer[255];
static DualSenseUSBInfo DS_info;
static uint8_t hid_report_descriptor[HID_REPORT_SIZE];

static inline void cs_low(){ 
    HAL_GPIO_WritePin(CH376_CS_PORT, CH376_CS_PIN, GPIO_PIN_RESET); 
}

static inline void cs_high() { 
    HAL_GPIO_WritePin(CH376_CS_PORT, CH376_CS_PIN, GPIO_PIN_SET); 
}

static void delay_us(uint16_t us) {
    volatile uint16_t count = us * (SystemCoreClock / 1000000) / 4;
    while(count--) {
        __NOP();
    }
}

static void ch376_endCommand(void) {
    cs_high();
    delay_us(4);
}

static void ch376_sendCommand(uint8_t cmd, SPI_HandleTypeDef *hspi1){
    cs_low();
    HAL_SPI_Transmit(hspi1, &cmd, 1, HAL_MAX_DELAY);
    delay_us(4); // Il CH376 richiede ~1.5us tra comando e dati
}

static void ch376_writeData(uint8_t data, SPI_HandleTypeDef *hspi1){
    HAL_SPI_Transmit(hspi1, &data, 1, HAL_MAX_DELAY);
    delay_us(4);
}

// Legge un dato a CS già abbassato
static uint8_t ch376_readData(SPI_HandleTypeDef *hspi1){
    uint8_t rx = 0xFF;
    HAL_SPI_Receive(hspi1, &rx, 1, HAL_MAX_DELAY);
    delay_us(4);
    return rx;
}

static uint8_t ch376_waitINTandGetStatus(SPI_HandleTypeDef *hspi1) {
    uint64_t starting = HAL_GetTick();
	uint8_t status = 0;

    while(HAL_GPIO_ReadPin(CH376_INT_PORT, CH376_INT_PIN) == GPIO_PIN_SET) {
        
        if ((HAL_GetTick() - starting) >= 1000)
        {
            return USB_ERR_TIMEOUT;
        }
    }

    ch376_sendCommand(GET_STATUS, hspi1); 
    status = ch376_readData(hspi1);
    ch376_endCommand();

    return status;
}

static uint8_t ch376_init(SPI_HandleTypeDef *hspi1){
	ch376_sendCommand(RESET_ALL, hspi1);     
	ch376_endCommand();    
	HAL_Delay(50);

	return USB_INT_SUCCESS;
}

static uint8_t ch376_reset_host(SPI_HandleTypeDef *hspi1){
	ch376_sendCommand(SET_USB_MODE, hspi1);
    ch376_writeData(RESET_BUS_USB, hspi1);
    ch376_endCommand();

    HAL_Delay(40);

    return USB_INT_SUCCESS;
}

static uint8_t ch376_connection(SPI_HandleTypeDef *hspi1){
	ch376_sendCommand(SET_USB_MODE, hspi1);
    ch376_writeData(HOST_MODE, hspi1);
    ch376_endCommand();

    HAL_Delay(100);

	uint8_t status = ch376_waitINTandGetStatus(hspi1);
	if (status != USB_INT_CONNECT){
		if (status == 0xFF) return USB_ERROR_CONNECTION;
			return status;
	}

    return status;
}

static uint8_t ch376_get_descriptor(SPI_HandleTypeDef *hspi1){
	uint8_t status = 0x00;
	
	uint8_t len = 0x00;
	uint8_t buffer[18];

	uint8_t setup[8] =
		{
			0x80,       // bmRequestType
			0x06,       // bRequest = GET_DESCRIPTOR
			0x00, 0x01, // wValue = DEVICE descriptor
			0x00, 0x00, // wIndex
			0x08, 0x00  // wLength = 18
		};

	// LOAD SETUP packet 
	HAL_Delay(50);

	ch376_sendCommand(WR_USB_DATA, hspi1);
    ch376_writeData(8, hspi1);

    for (uint8_t i = 0; i < 8; i++) ch376_writeData(setup[i], hspi1);

    ch376_endCommand();

	// SETUP TOKEN --> EP0 + TOKEN
	ch376_sendCommand(ISSUE_TKN_X, hspi1);

    ch376_writeData(0x00, hspi1);   // DATA PID / sync
    ch376_writeData(0x0D, hspi1);   // EP0 + SETUP

    ch376_endCommand();

    HAL_Delay(50);

	// Waiting operation
	status = ch376_waitINTandGetStatus(hspi1);
    if (status != USB_INT_SUCCESS)
		{
			if (status == 0xFF) return USB_ERROR_GET_DESCRIPTOR;
			else return status;
		}

	// DATA IN --> EP0 + IN
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
	ch376_writeData(0x80, hspi1);   // DATA1
    ch376_writeData(0x09, hspi1);   // EP0 + IN

    ch376_endCommand();

	// Wait DATA IN
	status = ch376_waitINTandGetStatus(hspi1);
    if (status != USB_INT_SUCCESS)
		{
			if (status == 0xFF) return USB_ERROR_GET_DESCRIPTOR;
			else return status;
		}

	ch376_sendCommand(RD_USB_DATA0, hspi1);

    len = ch376_readData(hspi1);
	for (uint8_t i = 0; i < len; i++) buffer[i] = ch376_readData(hspi1);
    ch376_endCommand();

	// STATUS OUT 
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x40, hspi1);   // DATA1
    ch376_writeData(0x01, hspi1);   // EP0 + OUT
    ch376_endCommand();

	// WAIT STATUS 
	status = ch376_waitINTandGetStatus(hspi1);
    if (status != USB_INT_SUCCESS)
		{
			if (status == 0xFF) return USB_ERROR_GET_DESCRIPTOR;
			else return status;
		}

	return USB_INT_SUCCESS;
}

static uint8_t ch376_setAddress(SPI_HandleTypeDef *hspi1){ 
	uint8_t status = 0x00;

	if (DUALSENSE_ADDRESS_USB > 0x7F) return USB_ERR_INVALID_ADDRESS;

	ch376_sendCommand(SET_ADDRESS, hspi1);
    ch376_writeData(DUALSENSE_ADDRESS_USB, hspi1);
    ch376_endCommand();

	status = ch376_waitINTandGetStatus(hspi1);
	if (status != USB_INT_SUCCESS)
		{
			if (status == 0xFF) return USB_ERROR_SET_ADDRESS;
			else return status;
		}
	
	ch376_sendCommand(SET_USB_ADDR, hspi1);
    ch376_writeData(DUALSENSE_ADDRESS_USB, hspi1);
    ch376_endCommand();

	HAL_Delay(20);
	
	return USB_INT_SUCCESS;
}

static uint8_t ch376_getConfig_Desc(SPI_HandleTypeDef *hspi1, uint16_t *total_length){
	uint8_t status = 0x00;
	uint8_t len = 0x00;
	uint8_t buffer[9];

	// SETUP DATA 
	ch376_sendCommand(WR_USB_DATA, hspi1);
    ch376_writeData(8, hspi1);
	uint8_t setup[8] = {
        0x80,
        0x06,
        0x00, 0x02,
        0x00, 0x00,
        0x09, 0x00 };

	for (uint8_t i = 0; i < 8; i++) ch376_writeData(setup[i], hspi1);
	ch376_endCommand();

	// SETUP TOKEN 
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x00, hspi1);
    ch376_writeData(0x0D, hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);

    if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_GET_CONFIG_DESC;
			else return status;
		}

	// DATA IN
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x80, hspi1);
    ch376_writeData(0x09, hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);
	if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_GET_CONFIG_DESC;
			else return status;
		}
	
	// READ DATA 
	ch376_sendCommand(RD_USB_DATA0, hspi1);
	len = ch376_readData(hspi1);

	if (len != 9) {
		ch376_endCommand();
		return USB_ERR_INVALID_LENGTH;
	}

	for (uint8_t i = 0; i < len; i++) buffer[i] = ch376_readData(hspi1);

	ch376_endCommand();

	// STATUS OUT
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x40, hspi1);
    ch376_writeData(0x01, hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);
	if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_GET_CONFIG_DESC;
			else return status;
		}

	// CHECK HEADER 
	if (buffer[0] != 9) return USB_INVALID_DESCRIPTOR;

	if (buffer[1] != 0x02) return USB_INVALID_DESCRIPTOR;

	// SETTING total_lenght
	if (*total_length == 0x0000) *total_length = buffer[2] | ((uint16_t)buffer[3] << 8);
 	
	return USB_INT_SUCCESS;      
}

static uint8_t ch376_GetFullDescriptor(SPI_HandleTypeDef *hspi1, uint16_t total_length, uint8_t *config_value){
	uint8_t status = 0x00;
	uint8_t len;
	memset(cfg_buffer, 0, sizeof(cfg_buffer));

    uint8_t setup[8] =
    {
        0x80,       // bmRequestType
        0x06,       // GET_DESCRIPTOR
        0x00, 0x02, // Configuration Descriptor
        0x00, 0x00, // wIndex
        (uint8_t)(total_length & 0xFF),
        (uint8_t)(total_length >> 8)
    };

	// SETUP DATA 
	ch376_sendCommand(WR_USB_DATA, hspi1);
    ch376_writeData(8, hspi1);

    for (uint8_t i = 0; i < 8; i++) ch376_writeData(setup[i], hspi1);

    ch376_endCommand();

	// SETUP TOKEN 
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x00, hspi1);
    ch376_writeData(0x0D, hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);

    if (status != USB_INT_SUCCESS) {
        if (status == 0xFF) return USB_ERROR_GET_CONFIG_DESC;
        return status;
    }

	// DATA IN TOKEN + READ DATA 
	uint8_t byte_read = 0;
	uint8_t sync = 0x80; // RX sync flag CH376

	while (byte_read < total_length)
	{
		ch376_sendCommand(ISSUE_TKN_X, hspi1);
		ch376_writeData(sync, hspi1);
		ch376_writeData(0x09, hspi1); 
		ch376_endCommand();
		status = ch376_waitINTandGetStatus(hspi1);
		
		if (status != USB_INT_SUCCESS){
        if (status == 0xFF) return USB_ERROR_GET_CONFIG_DESC;
        return status;
	}

		ch376_sendCommand(RD_USB_DATA0, hspi1);
		uint8_t chunk = ch376_readData(hspi1);
		for (int i = 0; i < chunk; i++){
			uint8_t b = ch376_readData(hspi1);
			if (byte_read < total_length){
				cfg_buffer[byte_read] = b;
			}
			byte_read++;
		}
		ch376_endCommand();

		// Alterna sync DATA0/DATA1 per i pacchetti successivi
		sync = (sync == 0x80) ? 0x00 : 0x80;

		// Se il pacchetto è corto (< 64 byte) siamo all'ultimo — esci
		if (chunk < 64) break;

	}
	// STATUS OUT 
	ch376_sendCommand(WR_USB_DATA, hspi1);
	ch376_writeData(0, hspi1);
	ch376_endCommand();

	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x40, hspi1);
    ch376_writeData(0x01, hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);

    if (status != USB_INT_SUCCESS) {
        if (status == 0xFF) return USB_ERROR_GET_CONFIG_DESC;
		return status;
    }

	*config_value = cfg_buffer[5];

	return USB_INT_SUCCESS;

}

static uint8_t ch376_setConfig(SPI_HandleTypeDef *hspi1, uint8_t config_value){
	uint8_t status = 0x00; 
	
	uint8_t setup[8] = { 
		0x00, // bmRequestType 
		0x09, // SET_CONFIGURATION 
		config_value, 0x00, // wValue 
		0x00, 0x00, // wIndex 
		0x00, 0x00 // wLength 
		};
	
	// SETUP DATA 
	ch376_sendCommand(WR_USB_DATA, hspi1);
	ch376_writeData(8, hspi1);
	for (uint8_t i = 0; i < 8; i++) ch376_writeData(setup[i], hspi1); 
	ch376_endCommand();

	// SETUP TOKEN 
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
	ch376_writeData(0x00, hspi1);
	ch376_writeData(0x0D, hspi1);
	ch376_endCommand();
	
	status = ch376_waitINTandGetStatus(hspi1);
	if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_SET_CONFIGURATION;
		return status;
	}

	// STATUS IN
	ch376_sendCommand(ISSUE_TKN_X, hspi1); 
	ch376_writeData(0x80, hspi1); 
	ch376_writeData(0x09, hspi1); 
	ch376_endCommand(); 
	
	status = ch376_waitINTandGetStatus(hspi1); 
	if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_SET_CONFIGURATION; 
		return status; 
	}

	return USB_INT_SUCCESS;
}

static uint8_t ch376_parseCfg(uint16_t total_length){

	uint16_t offset = 0;

    uint8_t current_interface = 0xFF;
    uint8_t found_interface = 0;

    memset(&DS_info, 0, sizeof(DS_info));

    while (offset + 2 <= total_length)
    {
        uint8_t length = cfg_buffer[offset];
        uint8_t type   = cfg_buffer[offset + 1];

        /* Descriptor non valido */
        if (length < 2)
            return USB_INVALID_PARSE_DESCRIPTOR;

        if ((offset + length) > total_length)
            return USB_INVALID_PARSE_DESCRIPTOR;


        // CONFIGURATION DESCRIPTOR
        if (type == 0x02)
        {
            if (length < 9)
                return USB_INVALID_PARSE_DESCRIPTOR;

            DS_info.configuration_value =
                cfg_buffer[offset + 5];
        }

        // INTERFACE DESCRIPTOR
        else if (type == 0x04)
        {
            if (length < 9)
                return USB_INVALID_PARSE_DESCRIPTOR;

            current_interface = cfg_buffer[offset + 2];

            // Per il DualSense ci interessa l'interfaccia HID. Class 0x03 = HID   
            if (cfg_buffer[offset + 5] == DUALSENSE_INTERFACE)
            {
                DS_info.interface_number =
                    cfg_buffer[offset + 2];

                DS_info.interface_class =
                    cfg_buffer[offset + 5];

                DS_info.interface_subclass =
                    cfg_buffer[offset + 6];

                DS_info.interface_protocol =
                    cfg_buffer[offset + 7];

                found_interface = 1;
            }
        }

        // HID DESCRIPTOR
        else if (type == 0x21)
        {
            if (found_interface)
            {
                if (length < 9)
                    return USB_INVALID_PARSE_DESCRIPTOR;

                DS_info.hid_report_descriptor_length =
                    cfg_buffer[offset + 7] |
                    ((uint16_t)cfg_buffer[offset + 8] << 8);
            }
        }

        // ENDPOINT DESCRIPTOR
        else if (type == 0x05)
        {
            if (length < 7)
                return USB_INVALID_PARSE_DESCRIPTOR;

            if (found_interface)
            {
                uint8_t address =
                    cfg_buffer[offset + 2];

                uint16_t max_packet =
                    cfg_buffer[offset + 4] |
                    ((uint16_t)cfg_buffer[offset + 5] << 8);

                uint8_t interval =
                    cfg_buffer[offset + 6];


                if (address & 0x80)
                {
                    /*
                     * Endpoint IN
                     */
                    DS_info.endpoint_in = address;
                    DS_info.endpoint_in_max_packet = max_packet;
                    DS_info.endpoint_in_interval = interval;
                }
                else
                {
                    /*
                     * Endpoint OUT
                     */
                    DS_info.endpoint_out = address;
                    DS_info.endpoint_out_max_packet = max_packet;
                    DS_info.endpoint_out_interval = interval;
                }
            }
        }
        /* Passa al descriptor successivo */
        offset += length;
    }

    if (!found_interface)
        return USB_INVALID_PARSE_DESCRIPTOR;

    return USB_INT_SUCCESS;
}

static uint8_t ch376_getHIDreport(SPI_HandleTypeDef *hspi1){
	uint8_t status;
    uint16_t byte_read = 0;

	memset(hid_report_descriptor, 0, sizeof(hid_report_descriptor));

    uint8_t setup[8] =
    {
        0x81,       // IN | Standard | Interface
        0x06,       // GET_DESCRIPTOR
        0x00, 0x22, // HID Report Descriptor
        0x03, 0x00, // Interface 3
        0x21, 0x01  // 289 bytes
    };

	if (DS_info.hid_report_descriptor_length != HID_REPORT_SIZE){
		return USB_INVALID_SIZE_HID;
	}

	// SETUP
	ch376_sendCommand(WR_USB_DATA, hspi1);
    ch376_writeData(8, hspi1);

    for (uint8_t i = 0; i < 8; i++) ch376_writeData(setup[i], hspi1);
    ch376_endCommand();

	// SETUP TOKEN
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x00, hspi1);
    ch376_writeData(0x0D, hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);

    if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_GET_HID; 
		return status; 
	}

	uint8_t sync = 0x80;

    while (byte_read < HID_REPORT_SIZE)
    {
        ch376_sendCommand(ISSUE_TKN_X, hspi1);
        ch376_writeData(sync, hspi1);
		ch376_writeData(0x09, hspi1); 
        ch376_endCommand();

        status = ch376_waitINTandGetStatus(hspi1);

    	if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_GET_HID; 
		return status; 
		}

        ch376_sendCommand(RD_USB_DATA0, hspi1);

        uint8_t chunk = ch376_readData(hspi1);

        for (uint8_t i = 0; i < chunk; i++)
        {
            uint8_t b = ch376_readData(hspi1);
            if (byte_read < HID_REPORT_SIZE) hid_report_descriptor[byte_read] = b;

            byte_read++;
        }
        ch376_endCommand();

        // Alterna DATA0 / DATA1
        sync = (sync == 0x80) ? 0x00 : 0x80;

		if (chunk < 64) break;
    }
	// STATUS OUT
	ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x00,hspi1);
    ch376_writeData(0x01,hspi1);
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);

    if (status != USB_INT_SUCCESS) {
		if (status == 0xFF) return USB_ERROR_GET_HID; 
		return status; 
	}

	return USB_INT_SUCCESS;
}

uint8_t ch376_enumerateDevice(SPI_HandleTypeDef *hspi1) {
	uint8_t status = 0x00;
	uint16_t total_lenght = 0x0000;
	uint8_t config_value = 0x00;
	usb_state = 0;
	
	// FASE 1 --> RESET 
	if (usb_state == RESET){
		status = ch376_init(hspi1);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_RESET_HOST;
	}

	// FASE 2 --> RESET_HOST 
	if (usb_state == USB_RESET_HOST){
		status = ch376_reset_host(hspi1);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_CONNECTION;
	}

	// FASE 3 --> CONNECTION 
	if (usb_state == USB_CONNECTION){
		status = ch376_connection(hspi1);
		if (status != USB_INT_CONNECT) return status;
		usb_state = USB_GET_DEVICE_DESCRIPTOR;
	}

	// FASE 4 --> GET_DEVICE_DESCRIPTOR
	if (usb_state == USB_GET_DEVICE_DESCRIPTOR){
		status = ch376_get_descriptor(hspi1);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_SET_ADDRESS;
	}

	// FAES 5 -->  USB_SET_ADDRESS
	if (usb_state == USB_SET_ADDRESS){
		status = ch376_setAddress(hspi1);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_GET_CONFIG_DESCRIPTOR;
	}

	// FASE 6 --> USB_GET_CONFIG_DESCRIPTOR
	if (usb_state == USB_GET_CONFIG_DESCRIPTOR){
		status = ch376_getConfig_Desc(hspi1,&total_lenght);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_GET_FULL_DESCRIPTOR;
	}

	// FASE 7 --> USB_GET_FULL_DESCRIPTOR
	if (usb_state == USB_GET_FULL_DESCRIPTOR){
		status = ch376_GetFullDescriptor(hspi1, total_lenght, &config_value);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_PARSING_CFG;
	}
	// FASE 8 --> USB_SET_CONFIGURATION
	if (usb_state == USB_PARSING_CFG){
		status = ch376_parseCfg(total_lenght);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_SET_CONFIGURATION;
	}
	// FASE 9 --> USB_PARSING_CFG
	if (usb_state == USB_SET_CONFIGURATION){
		status = ch376_setConfig(hspi1, config_value);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_HID_GET_REPORT;
	}

	// FASE 10 --> USB_HID_GET_REPORT
	if (usb_state == USB_HID_GET_REPORT){
		status = ch376_getHIDreport(hspi1);
		if (status != USB_INT_SUCCESS) return status;
		usb_state = USB_DONE;
	}

	if(usb_state == USB_DONE){
		usb_state = 0;
		return USB_INT_SUCCESS;
	}

	return USB_INT_SUCCESS;
}

// Avvia la transazione in modo non bloccante. Non aspetta l'interrupt.
void ch376_startINTransaction(SPI_HandleTypeDef *hspi, uint8_t sync_bit) {
    ch376_sendCommand(ISSUE_TKN_X, hspi);
    ch376_writeData(sync_bit, hspi); 
    ch376_writeData(((DS_info.endpoint_in & 0x0F) << 4) | USB_PID_IN, hspi); // IN token per l'endpoint 0x84 ossia EP4
    ch376_endCommand();
}

// Funzione chiamata dentro takeUSBdata() quando arriva l'interrupt
uint8_t ch376_readInterruptDataAsync(SPI_HandleTypeDef *hspi, uint8_t *buffer, uint8_t *len) {
    uint8_t status;
    
    // Leggiamo lo stato dell'interrupt appena arrivato (questo pulisce anche il pin INT del CH376)
    ch376_sendCommand(GET_STATUS, hspi); 
    status = ch376_readData(hspi);
    ch376_endCommand();

    if (status == USB_INT_SUCCESS) {
        ch376_sendCommand(RD_USB_DATA0, hspi);
        *len = ch376_readData(hspi);
		if (*len > 64){
			*len = 64;
		}
        for (int i = 0; i < (*len); i++) {
            buffer[i] = ch376_readData(hspi);
        }
        ch376_endCommand();
    }
    
    return status;
}

uint8_t ch376_checkExist(SPI_HandleTypeDef *hspi, uint8_t test_val) {
    uint8_t res;
    ch376_sendCommand(CHECK_EXIST, hspi);
    ch376_writeData(test_val, hspi);
    res = ch376_readData(hspi);
    ch376_endCommand();
    return res;
}

uint8_t ch376_setDualsenseLED(SPI_HandleTypeDef *hspi1){
	uint8_t status;

    uint8_t report[DS_REPORT_OUTPUT_SIZE];

	memset(report,0,sizeof(report));


	/*FF F7 00 00 00 00 00 00 00 10
	00 00 00 00 00 00 00 00
	00 00 00 00 00 00 00 00
	00 00 00 00 00 00 00 00 00 00
	02 00 02 00 00
	FF FF FF*/

    report[0]  = DS_OUTPUT_REPORT;
    report[1]  = 0xFF;       
	report[2]  = 0xF7;

	report[10] = 0x10;

	report[40] = 0x02;
	report[42] = 0x02;

    report[45] = 0xFF;
    report[46] = 0xFF;
    report[47] = 0xFF;


    // Carica il report nel buffer USB del CH376
    ch376_sendCommand(WR_USB_DATA, hspi1);
    ch376_writeData(DS_REPORT_OUTPUT_SIZE, hspi1);

    for (uint8_t i = 0; i < DS_REPORT_OUTPUT_SIZE; i++) ch376_writeData(report[i], hspi1);

    ch376_endCommand();

    // OUT token verso endpoint 3
    ch376_sendCommand(ISSUE_TKN_X, hspi1);
    ch376_writeData(0x00, hspi1);  // DATA0
    ch376_writeData(((DS_info.endpoint_out & 0x0F) << 4) | USB_PID_OUT, hspi1);  // EP3 OUT
    ch376_endCommand();

    status = ch376_waitINTandGetStatus(hspi1);

    return status;
}
