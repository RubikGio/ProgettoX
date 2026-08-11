#include "MAX4231E.h"
#include "stm32f3xx_hal.h"
#include <string.h>

#define BYTE uint8_t
#define WORD uint16_t

// Set transfer bounds
#define NAK_LIMIT 200
#define RETRY_LIMIT 3

// Global variables
static BYTE XfrData[2000];      // Big array to handle max size descriptor data
static BYTE maxPacketSize;      // discovered and filled in by Get_Descriptor-device request in enumerate_device()
static WORD VID,PID,nak_count,IN_nak_count,HS_nak_count;
static unsigned int last_transfer_size;
unsigned volatile long timeval;         // incremented by timer0 ISR
WORD inhibit_send;

uint8_t dataOut[80];
uint8_t dataIn[5];

void Hwreg(uint8_t reg, uint8_t val, SPI_HandleTypeDef *hspi5);
uint8_t Hrreg(uint8_t reg, SPI_HandleTypeDef *hspi5);
void MAX_Reset(SPI_HandleTypeDef *hspi5);
void detect_device(SPI_HandleTypeDef *hspi5);
void waitframes(uint8_t num, SPI_HandleTypeDef *hspi5);
void enumerate_device(SPI_HandleTypeDef *hspi5);
BYTE CTL_Write_ND(BYTE *pSUD, SPI_HandleTypeDef *hspi5);
BYTE CTL_Read(BYTE *pSUD, SPI_HandleTypeDef *hspi5);
BYTE Send_Packet(BYTE token,BYTE endpoint, SPI_HandleTypeDef *hspi5);
BYTE IN_Transfer(BYTE endpoint,WORD INbytes, SPI_HandleTypeDef *hspi5);

// Firme aggiornate come richiesto
void MAX_Init(SPI_HandleTypeDef *hspi5);
void MAX_Process(SPI_HandleTypeDef *hspi5);

void Hwreg(uint8_t reg, uint8_t val, SPI_HandleTypeDef *hspi5){
    int delay = 50;
    dataOut[0] = reg | 2;
    dataOut[1] = val;
    HAL_GPIO_WritePin(MAX_CS_PORT, MAX_CS_PIN, GPIO_PIN_RESET); // Abbassa CS
    HAL_SPI_TransmitReceive(hspi5, dataOut, dataIn, 2, 1000);
    HAL_GPIO_WritePin(MAX_CS_PORT, MAX_CS_PIN, GPIO_PIN_SET);   // Alza CS
    while(delay--);
}

uint8_t Hrreg(uint8_t reg, SPI_HandleTypeDef *hspi5){
    int delay = 50;
    dataOut[0] = reg;
    dataOut[1] = 0;
    HAL_GPIO_WritePin(MAX_CS_PORT, MAX_CS_PIN, GPIO_PIN_RESET); // Abbassa CS
    HAL_SPI_TransmitReceive(hspi5, dataOut, dataIn, 2, 1000);
    HAL_GPIO_WritePin(MAX_CS_PORT, MAX_CS_PIN, GPIO_PIN_SET);   // Alza CS
    while(delay--);
    return dataIn[1];
}

void Hwritebytes(BYTE reg, BYTE N, BYTE *p, SPI_HandleTypeDef *hspi5)
{
    int delay = 50;
    dataOut[0] = reg+0x02;          // command byte into the FIFO. 0x0002 is the write bit
    for(int j = 0; j < N; j++)
    {
        dataOut[j+1] = *p++;                // send the next data byte
    }
    MAX_CS_PORT->BSRR = (uint32_t)MAX_CS_PIN << 16;
    HAL_SPI_Transmit(hspi5, dataOut, N+1, 1000); // Utilizza il puntatore
    MAX_CS_PORT->BSRR = MAX_CS_PIN;
    while(delay--);
}

void MAX_Init(SPI_HandleTypeDef *hspi5){

    dataOut[0] = (17<<3) | 2;
    dataOut[1] = 0x10;
    GPIOF->BSRR = (uint32_t)GPIO_PIN_6 << 16;
    HAL_SPI_Transmit(hspi5, dataOut, 2, 1000);
    GPIOF->BSRR = GPIO_PIN_6;
    Hwreg(rUSBIEN, bmOSCOKIE, hspi5);
    Hrreg(rUSBIEN,hspi5);
    Hrreg(18<<3,hspi5);
    MAX_Reset(hspi5);
    Hwreg(rIOPINS1,0x00,hspi5);       // seven-segs off
    Hwreg(rIOPINS2,0x00,hspi5);       // and Vbus OFF (in case something already plugged in)
    HAL_Delay(200);
    Hwreg(rIOPINS2,(Hrreg(rIOPINS2,hspi5)|0x08),hspi5); //VBUS-ON
}

void MAX_Process(SPI_HandleTypeDef *hspi5){
    detect_device(hspi5);
    waitframes(200,hspi5);            // Some devices require this
    enumerate_device(hspi5);
}

void detect_device(SPI_HandleTypeDef *hspi5)
{
    int busstate;
    // Activate HOST mode & turn on the 15K pulldown resistors on D+ and D-
    Hwreg(rMODE,(bmDPPULLDN|bmDMPULLDN|bmHOST), hspi5); // Note--initially set up as a FS host (LOWSPEED=0)
    Hwreg(rHIRQ,bmCONDETIRQ, hspi5);  // clear the connection detect IRQ

    do      // See if anything is plugged in. If not, hang until something plugs in
    {
        Hwreg(rHCTL,bmSAMPLEBUS, hspi5);           // update the JSTATUS and KSTATUS bits
        busstate = Hrreg(rHRSL, hspi5);            // read them
        busstate &= (bmJSTATUS|bmKSTATUS);  // check for either of them high
    }
    while (busstate==0);
    if (busstate==bmJSTATUS)    // since we're set to FS, J-state means D+ high
    {
        Hwreg(rMODE,(bmDPPULLDN|bmDMPULLDN|bmHOST|bmSOFKAENAB), hspi5);  // make the MAX3421E a full speed host
    }
    if (busstate==bmKSTATUS)  // K-state means D- high
    {
        Hwreg(rMODE,(bmDPPULLDN|bmDMPULLDN|bmHOST|bmLOWSPEED|bmSOFKAENAB), hspi5);  // make the MAX3421E a low speed host
    }
}

void enumerate_device(SPI_HandleTypeDef *hspi5)
{
    static BYTE HR,iCONFIG,iMFG,iPROD,iSERIAL;
    static WORD TotalLen,ix;
    static BYTE len,type,adr,pktsize;
    // SETUP bytes for the requests we'll send to the device
    static BYTE Set_Address_to_7[8]             = {0x00,0x05,0x07,0x00,0x00,0x00,0x00,0x00};
    static BYTE Get_Descriptor_Device[8]        = {0x80,0x06,0x00,0x01,0x00,0x00,0x00,0x00}; // code fills in length field
    static BYTE Get_Descriptor_Config[8]        = {0x80,0x06,0x00,0x02,0x00,0x00,0x00,0x00};
    static BYTE str[8] = {0x80,0x06,0x00,0x03,0x00,0x00,0x40,0x00}; // Get_Descriptor-String template. Code fills in idx at str[2].

    // Issue a USB bus reset
    Hwreg(rHCTL,bmBUSRST,hspi5);           // initiate the 50 msec bus reset
    while(Hrreg(rHCTL,hspi5) & bmBUSRST);  // Wait for the bus reset to complete

    // Wait some frames before programming any transfers. This allows the device to recover from
    // the bus reset.
    waitframes(200,hspi5);

    // Get the device descriptor.
    maxPacketSize = 8;              // only safe value until we find out
    Hwreg(rPERADDR,0,hspi5);              // First request goes to address 0
    Get_Descriptor_Device[6]=8;     // wLengthL
    Get_Descriptor_Device[7]=0;     // wLengthH
	HR = CTL_Read(Get_Descriptor_Device,hspi5);   // Get device descriptor into XfrData[]
    if((HR)) return; // () does nothing if HRSL=0, returns the 4-bit HRSL.

    // Show NAK count for data stage/status stage (Rimossi argomenti extra dal printfUART per sicurezza se non usi sprintf)
    maxPacketSize = XfrData[7];
    for (ix=0; ix<last_transfer_size;ix++){
        // (Iterazione array disabilitata)
    }

    // Issue another USB bus reset
    Hwreg(rHCTL,bmBUSRST,hspi5);           // initiate the 50 msec bus reset
    while(Hrreg(rHCTL,hspi5) & bmBUSRST);  // Wait for the bus reset to complete
    waitframes(200,hspi5);

    // Set_Address to 7 (Note: this request goes to address 0, already set in PERADDR register).
    HR = CTL_Write_ND(Set_Address_to_7,hspi5);   // CTL-Write, no data stage
    if((HR)) return;

    waitframes(30,hspi5);           // Device gets 2 msec recovery time
    Hwreg(rPERADDR,7,hspi5);       // now all transfers go to addr 7

    // Get the device descriptor at the assigned address.
    Get_Descriptor_Device[6]=0x12;          // fill in the real device descriptor length
    HR = CTL_Read(Get_Descriptor_Device, hspi5);  // Get device descriptor into XfrData[]
    if((HR)) return;
    
    VID     = XfrData[8] + 256*XfrData[9];
    PID     = XfrData[10]+ 256*XfrData[11];
    iMFG    = XfrData[14];
    iPROD   = XfrData[15];
    iSERIAL = XfrData[16];

    for (ix=0; ix<last_transfer_size;ix++){
        // (Iterazione array disabilitata)
    }
    
    //
    str[2]=0;   // index 0 is language ID string
    str[4]=0;   // lang ID is 0
    str[5]=0;
    str[6]=4;   // wLengthL
    str[7]=0;   // wLengthH

    HR = CTL_Read(str,hspi5);     // Get lang ID string
    if (!HR)                // Check for ACK (could be a STALL if the device has no strings)
        {
		for (ix=0; ix<last_transfer_size;ix++){
           // (Iterazione)
        }
        str[4]=XfrData[2];  // LangID-L
        str[5]=XfrData[3];  // LangID-H
        str[6]=255;         // now request a really big string
        }
    if(iMFG)
        {
        str[2]=iMFG;            // fill in the string index from the device descriptor
        HR = CTL_Read(str,hspi5);     // Get Manufacturer ID string
        }

    if(iPROD)
        {
        str[2]=iPROD;
        HR = CTL_Read(str,hspi5);  // Get Product ID string
        }

    if(iSERIAL)
        {
        str[2]=iSERIAL;
        HR = CTL_Read(str,hspi5);  // Get Serial Number ID string
        }

    // Get the 9-byte configuration descriptor

    Get_Descriptor_Config[6]=9; // fill in the wLengthL field
    Get_Descriptor_Config[7]=0; // fill in the wLengthH field

    HR = CTL_Read(Get_Descriptor_Config,hspi5);  // Get config descriptor into XfrData[]
    if((HR)) return;

    for (ix=0; ix<last_transfer_size;ix++){
        // (Iterazione)
    }

    // Now that the full length of all descriptors (Config, Interface, Endpoint, maybe Class)
    // is known we can fill in the correct length and ask for the full boat.

    Get_Descriptor_Config[6]=XfrData[2];    // LengthL
    Get_Descriptor_Config[7]=XfrData[3];    // LengthH
    HR = CTL_Read(Get_Descriptor_Config,hspi5);  // Get config descriptor into XfrData[]

    iCONFIG = XfrData[6];   // optional configuration string

    //
    // Parse the config+ data for interfaces and endpoints. Skip over everything but
    // interface and endpoint descriptors.
    //
    TotalLen=last_transfer_size;
    ix=0;
        do
        {
        len=XfrData[ix];        // length of first descriptor (the CONFIG descriptor)
        type=XfrData[ix+1];
        adr=XfrData[ix+2];
        pktsize=XfrData[ix+4];
        ix += len;              // point to next descriptor
        }
        while (ix<TotalLen);
    //
    if(iCONFIG)
        {
        str[2]=iCONFIG;
        HR = CTL_Read(str,hspi5);  // Get Config string
        }
}

// ----------------------------------------------------
// CONTROL-Read Transfer. Get the length from SUD[7:6].
// ----------------------------------------------------
BYTE CTL_Read(BYTE *pSUD,SPI_HandleTypeDef *hspi5)
{
  BYTE  resultcode;
  WORD  bytes_to_read;
  bytes_to_read = pSUD[6] + 256*pSUD[7];

// SETUP packet
  Hwritebytes(rSUDFIFO,8,pSUD,hspi5);             // Load the Setup data FIFO
  resultcode=Send_Packet(tokSETUP,0,hspi5);       // SETUP packet to EP0
  if (resultcode) return (resultcode);      // should be 0, indicating ACK. Else return error code.
// One or more IN packets (may be a multi-packet transfer)
  Hwreg(rHCTL,bmRCVTOG1,hspi5);                   // FIRST Data packet in a CTL transfer uses DATA1 toggle.
  resultcode = IN_Transfer(0,bytes_to_read,hspi5);
  if(resultcode) return (resultcode);

  IN_nak_count=nak_count;
// The OUT status stage
  resultcode=Send_Packet(tokOUTHS,0,hspi5);
  if (resultcode) return (resultcode);   // should be 0, indicating ACK. Else return error code.
  return(0);    // success!
}

// -----------------------------------------------------------------------------------
// IN Transfer to arbitrary endpoint.
// -----------------------------------------------------------------------------------
BYTE IN_Transfer(BYTE endpoint,WORD INbytes, SPI_HandleTypeDef *hspi5)
{
BYTE resultcode,j;
BYTE pktsize;
unsigned int xfrlen,xfrsize;

xfrsize = INbytes;
xfrlen = 0;

while(1) // use a 'return' to exit this loop.
  {
  resultcode=Send_Packet(tokIN,endpoint,hspi5);       // IN packet to EP-'endpoint'. Function takes care of NAKS.
  if (resultcode) return (resultcode);          // should be 0, indicating ACK. Else return error code.
  pktsize=Hrreg(rRCVBC,hspi5);                        // number of received bytes
  for(j=0; j<pktsize; j++)                      // add this packet's data to XfrData array
      XfrData[j+xfrlen] = Hrreg(rRCVFIFO,hspi5);
  Hwreg(rHIRQ,bmRCVDAVIRQ,hspi5);                     // Clear the IRQ & free the buffer
  xfrlen += pktsize;                            // add this packet's byte count to total transfer length

  if ((pktsize < maxPacketSize) || (xfrlen >= xfrsize))    // have we transferred 'length' bytes?
        {
        last_transfer_size = xfrlen;
        return(resultcode);
        }
  }
}

BYTE CTL_Write_ND(BYTE *pSUD, SPI_HandleTypeDef *hspi5)
{
  BYTE resultcode;
  Hwritebytes(rSUDFIFO,8,pSUD,hspi5);
// 1. Send the SETUP token and 8 setup bytes. Device should immediately ACK.
  resultcode=Send_Packet(tokSETUP,0,hspi5);    // SETUP packet to EP0
  if (resultcode) return (resultcode);   // should be 0, indicating ACK.

// 2. No data stage, so the last operation is to send an IN token
  resultcode=Send_Packet(tokINHS,0,hspi5);   // This function takes care of NAK retries.
  if(resultcode) return (resultcode);  // should be 0, indicating ACK.
  else  return(0);
}

void MAX_Reset(SPI_HandleTypeDef *hspi5)
{
    Hwreg(rUSBCTL,bmCHIPRES, hspi5);  // chip reset This stops the oscillator
    Hwreg(rUSBCTL,0x00,hspi5);       // remove the reset
    while(!(Hrreg(rUSBIRQ,hspi5) & bmOSCOKIRQ)) ;  // hang until the PLL stabilizes
}

void waitframes(BYTE num, SPI_HandleTypeDef *hspi5)
{
    BYTE k;
    Hwreg(rHIRQ,bmFRAMEIRQ,hspi5);     // clear any pending
    k=0;
    while(k!=num)               // do this at least once
    {
        while(!(Hrreg(rHIRQ,hspi5)& bmFRAMEIRQ));
        Hwreg(rHIRQ,bmFRAMEIRQ,hspi5); // clear the IRQ
        k++;
    }
}

// -----------------------------------------------------------------------------------
// Send a packet.
// -----------------------------------------------------------------------------------
BYTE Send_Packet(BYTE token,BYTE endpoint, SPI_HandleTypeDef *hspi5)
{
BYTE resultcode,retry_count;
retry_count = 0;
nak_count = 0;

while(1)    
  {
  Hwreg(rHXFR,(token|endpoint), hspi5);         // launch the transfer
  while(!(Hrreg(rHIRQ, hspi5) & bmHXFRDNIRQ));  // wait for the completion IRQ
  Hwreg(rHIRQ,bmHXFRDNIRQ, hspi5);              // clear the IRQ
  resultcode = (Hrreg(rHRSL,hspi5) & 0x0F);    // get the result
  if (resultcode==hrNAK)
    {
    nak_count++;
    if(nak_count==NAK_LIMIT) break;
    else continue;
    }

  if (resultcode==hrTIMEOUT)
    {
    retry_count++;
    if (retry_count==RETRY_LIMIT) break;    // hit the max allowed retries. Exit and return result code
    else continue;
    }
  else break;                               // all other cases, just return the success or error code
  }
return(resultcode);
}