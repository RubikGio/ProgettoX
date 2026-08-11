#ifndef INC_CH376_H_
#define INC_CH376_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "stm32f3xx_hal.h"

enum USB_STATE{
	USB_RESET = 0,
	USB_RESET_HOST = 1,
	USB_CONNECTION = 2,
	USB_GET_DEVICE_DESCRIPTOR = 3,
	USB_SET_ADDRESS = 4,
	USB_GET_CONFIG_DESCRIPTOR = 5,
	USB_GET_FULL_DESCRIPTOR = 6,
	USB_PARSING_CFG = 7,
	USB_SET_CONFIGURATION = 8,
	USB_HID_GET_REPORT = 9,
	USB_DONE = 10
};

typedef struct __attribute__((packed))
{
    uint8_t  bLength;
    uint8_t  bDescriptorType;

    uint16_t bcdUSB;

    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;

    uint8_t  bMaxPacketSize0;

    uint16_t idVendor;
    uint16_t idProduct;

    uint16_t bcdDevice;

    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;

    uint8_t  bNumConfigurations;

} USB_DeviceDescriptor;

typedef struct
{
    uint8_t  configuration_value;

    uint8_t  interface_number;
    uint8_t  interface_class;
    uint8_t  interface_subclass;
    uint8_t  interface_protocol;

    uint16_t hid_report_descriptor_length;

    uint8_t  endpoint_in;
    uint16_t endpoint_in_max_packet;
    uint8_t  endpoint_in_interval;

    uint8_t  endpoint_out;
    uint16_t endpoint_out_max_packet;
    uint8_t  endpoint_out_interval;

} DualSenseUSBInfo;

// USB ADDRESS 
#define DUALSENSE_ADDRESS_USB 0x01
#define DUALSENSE_INTERFACE 0x03
#define HID_REPORT_SIZE 289
#define USB_PID_IN 0x09
#define USB_PID_OUT 0x01

// CUSTOM EXIT_CODE 
#define USB_ERR_TIMEOUT 0xFF
#define USB_ERROR_GET_DESCRIPTOR 0xFE
#define USB_ERR_INVALID_LENGTH 0xFD
#define USB_ERR_INVALID_DESCRIPTOR 0xFC
#define USB_ERR_INVALID_ADDRESS 0xFB
#define USB_ERROR_SET_ADDRESS 0xFA
#define USB_ERROR_GET_CONFIG_DESC 0xF9
#define USB_INVALID_DESCRIPTOR 0xF8
#define USB_ERROR_SET_CONFIGURATION 0xF7
#define USB_INVALID_PARSE_DESCRIPTOR 0xF6
#define USB_INVALID_SIZE_HID 0xF5
#define USB_ERROR_GET_HID 0xF4

// OUTPUT COMMAND DUALSENSE 
#define DS_REPORT_OUTPUT_SIZE 63
#define DS_OUTPUT_REPORT 0x02

// COMMAND 
#define GET_IC_VER      0x01
#define SET_BAUD_RATE   0x02 // useless for this case 
#define ENTER_SLEEP     0x03 // go to low power and suspending 
#define RESET_ALL       0x05 // Execute hardware reset 
#define CHECK_EXIST     0x06 // Test communication interface and working status 
#define RESET_BUS_USB	0x07 // Go for reset usb bus (essential)
#define SET_SD0_INT     0x0B // Set interrupt mode of SD0 in SPI
#define SET_USB_MODE    0x15 // Configure work mode of USB 
#define GET_STATUS      0x22 // Get interruption status and cancel requirements 
#define RD_USB_DATA0    0x27 // Read data from current INT port buffer USB or receive buffer of host port 
#define WR_USB_DATA     0x2C // Write data to transfer buffer of USB host 
#define WR_REQ_DATA     0x2D // Write reqeusted data block to internal appointed buffer 
#define SET_RETRY		0x0B // Set the number of retries for USB transaction operations

// USB MODE
#define HOST_MODE 0x06 // Host USB mode 

// COMMAND FOR USB
#define SET_USB_SPEED   0x04 // Set USB_speed 
#define GET_DEV_RATE    0x0A // Get data rate type of the USB device 
#define SET_USB_ADDR    0x13 // Set USB address
#define TEST_CONNECT    0x16 // Check the connection status of USB device
#define SET_ENDP6       0x1C // Set the receiver for USB host endpoint 
#define SET_ENDP7       0x1D // Set the transmitter for USB host endpoint 
#define SET_ADDRESS     0x45 // Set USB Address 
#define GET_DESCR       0x46 // Get the descriptor 
#define SET_CONFIG      0x49 // Set USB configuration
#define AUTO_SETUP      0x4D // Automatically configure USB device 
#define ISSUE_TKN_X     0x4E // Send Synch token and execute transactions 
#define COMMAND_RETRY	0x25 // Command for CH376 that help to interrogate at the right moment

// CONFIGURATION BIT FOR USB COMMAND 
#define FULL_SPEED      0x00 // full speed mode sending 
#define LOW_SPEED       0x02 // low speed mode sending 

// STATUS CODE 
#define CMD_RET_SUCCESS 0x51 // Operation Successfull
#define CMD_RET_ABORT   0x5F // Operation Failure 

// STATUS CODE USB
#define USB_INT_SUCCESS 0x14 // Success of transfer operation 
#define USB_INT_CONNECT 0x15 // Detection of USB device and connection 
#define USB_INT_DISCONNECT 0x16 // Disconnection of USB device 
#define USB_INT_BUF_OVER 0x17 // Error buffer overflow or data error 
#define USB_INT_NAK     0x2A // Device returned NAK
#define USB_INT_STALL   0x2E // Device returned STALL

// Pin definitions
#define CH376_CS_PORT   GPIOC 
#define CH376_CS_PIN    GPIO_PIN_13 

#define CH376_INT_PORT  GPIOF
#define CH376_INT_PIN   GPIO_PIN_6

uint8_t ch376_enumerateDevice(SPI_HandleTypeDef *hspi);
uint8_t ch376_readInterruptData(SPI_HandleTypeDef *hspi, uint8_t *buffer, uint8_t *len);
uint8_t ch376_checkExist(SPI_HandleTypeDef *hspi, uint8_t test_val);
uint8_t ch376_readInterruptDataAsync(SPI_HandleTypeDef *hspi, uint8_t *buffer, uint8_t *len);
uint8_t ch376_unlockDualSense(SPI_HandleTypeDef *hspi1);

void ch376_startINTransaction(SPI_HandleTypeDef *hspi, uint8_t sync_bit);
uint8_t ch376_setDualsenseLED(SPI_HandleTypeDef *hspi);

#endif /* INC_CH376_H_ */
