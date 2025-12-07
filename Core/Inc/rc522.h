#ifndef RC522_H
#define RC522_H

#include "main.h"

// Comandos y registros del RC522
#define PCD_IDLE       0x00
#define PCD_AUTHENT    0x0E
#define PCD_RECEIVE    0x08
#define PCD_TRANSMIT   0x04
#define PCD_TRANSCEIVE 0x0C
#define PICC_REQIDL    0x26
#define PICC_ANTICOLL  0x93

// Funciones públicas
void MFRC522_Init(void);
uint8_t MFRC522_Check(uint8_t *id);
uint8_t MFRC522_Compare(uint8_t *CardID, uint8_t *CompareID);
void MFRC522_WriteRegister(uint8_t addr, uint8_t val);
uint8_t MFRC522_ReadRegister(uint8_t addr);
void MFRC522_SetBitMask(uint8_t reg, uint8_t mask);
void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask);
void MFRC522_AntennaOn(void);
void MFRC522_Reset(void);
uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen);
uint8_t MFRC522_Anticoll(uint8_t *serNum);

#endif