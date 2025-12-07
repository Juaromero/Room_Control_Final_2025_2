#include "rc522.h"
#include <stdio.h>
#include <string.h>

extern SPI_HandleTypeDef hspi2; // Usamos SPI2 según tu configuración

// Macros para controlar el Chip Select (CS) y Reset
// Si CubeMX generó los defines en main.h, esto funcionará directo.
#define RC522_CS_LOW  HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_RESET)
#define RC522_CS_HIGH HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_SET)

void MFRC522_WriteRegister(uint8_t addr, uint8_t val) {
    uint8_t addr_bits = (addr << 1) & 0x7E;
    RC522_CS_LOW;
    HAL_SPI_Transmit(&hspi2, &addr_bits, 1, 100);
    HAL_SPI_Transmit(&hspi2, &val, 1, 100);
    RC522_CS_HIGH;
}

uint8_t MFRC522_ReadRegister(uint8_t addr) {
    uint8_t val;
    uint8_t addr_bits = ((addr << 1) & 0x7E) | 0x80;
    RC522_CS_LOW;
    HAL_SPI_Transmit(&hspi2, &addr_bits, 1, 100);
    HAL_SPI_Receive(&hspi2, &val, 1, 100);
    RC522_CS_HIGH;
    return val;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
    MFRC522_WriteRegister(reg, MFRC522_ReadRegister(reg) | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
    MFRC522_WriteRegister(reg, MFRC522_ReadRegister(reg) & (~mask));
}

void MFRC522_AntennaOn(void) {
    uint8_t temp = MFRC522_ReadRegister(0x14);
    if (!(temp & 0x03)) MFRC522_SetBitMask(0x14, 0x03);
}

void MFRC522_Init(void) {
    // Reset Hardware
    HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_SET); 
    MFRC522_Reset();
    
    MFRC522_WriteRegister(0x2A, 0x8D);
    MFRC522_WriteRegister(0x2B, 0x3E);
    MFRC522_WriteRegister(0x2D, 30);
    MFRC522_WriteRegister(0x2C, 0);
    MFRC522_WriteRegister(0x15, 0x40);
    MFRC522_WriteRegister(0x11, 0x3D);
    MFRC522_AntennaOn();
}

void MFRC522_Reset(void) {
    MFRC522_WriteRegister(0x01, 0x0F);
}

uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen) {
    uint8_t status = 0;
    uint8_t irqEn = 0x00;
    uint8_t waitIRq = 0x00;
    uint8_t lastBits, n;
    uint16_t i;

    if (command == PCD_AUTHENT) { irqEn = 0x12; waitIRq = 0x10; }
    else if (command == PCD_TRANSCEIVE) { irqEn = 0x77; waitIRq = 0x30; }

    MFRC522_WriteRegister(0x02, irqEn | 0x80);
    MFRC522_ClearBitMask(0x0D, 0x80);
    MFRC522_SetBitMask(0x0A, 0x80);
    MFRC522_WriteRegister(0x01, 0x00);

    for (i = 0; i < sendLen; i++) MFRC522_WriteRegister(0x09, sendData[i]);

    MFRC522_WriteRegister(0x01, command);
    if (command == PCD_TRANSCEIVE) MFRC522_SetBitMask(0x0D, 0x80);

    i = 2000;
    do {
        n = MFRC522_ReadRegister(0x04);
        i--;
    } while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

    MFRC522_ClearBitMask(0x0D, 0x80);

    if (i != 0) {
        if (!(MFRC522_ReadRegister(0x06) & 0x1B)) {
            status = 0;
            if (n & irqEn & 0x01) status = 1;
            if (command == PCD_TRANSCEIVE) {
                n = MFRC522_ReadRegister(0x09);
                lastBits = MFRC522_ReadRegister(0x0C) & 0x07;
                if (lastBits) *backLen = (n - 1) * 8 + lastBits;
                else *backLen = n * 8;
                if (n == 0) n = 1;
                if (n > 16) n = 16;
                for (i = 0; i < n; i++) backData[i] = MFRC522_ReadRegister(0x09);
            }
        } else status = 1;
    } else status = 1;

    return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum) {
    uint8_t status;
    uint16_t unLen;
    MFRC522_WriteRegister(0x0D, 0x00);
    serNum[0] = PICC_ANTICOLL;
    serNum[1] = 0x20;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);
    return status;
}

uint8_t MFRC522_Check(uint8_t *id) {
    uint8_t status;
    status = MFRC522_Request(PICC_REQIDL, id);
    if (status == 0) status = MFRC522_Anticoll(id);
    MFRC522_WriteRegister(0x0D, 0x00);
    return status;
}

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType) {
    uint8_t status;
    uint16_t backBits;
    MFRC522_WriteRegister(0x0D, 0x07);
    TagType[0] = reqMode;
    status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);
    if ((status != 0) || (backBits != 0x10)) status = 1;
    return status;
}

uint8_t MFRC522_Compare(uint8_t *CardID, uint8_t *CompareID) {
    return memcmp(CardID, CompareID, 5);
}