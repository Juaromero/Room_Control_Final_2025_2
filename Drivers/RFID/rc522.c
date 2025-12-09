#include "rc522.h"
#include <string.h>

// Variable externa del SPI (Definida en main.c)
extern SPI_HandleTypeDef hspi2;

// Mapeo de registros RC522
#define MFRC522_REG_COMMAND           0x01
#define MFRC522_REG_COMM_I_EN         0x02
#define MFRC522_REG_DIV_I_EN          0x03
#define MFRC522_REG_COMM_IRQ          0x04
#define MFRC522_REG_DIV_IRQ           0x05
#define MFRC522_REG_ERROR             0x06
#define MFRC522_REG_STATUS1           0x07
#define MFRC522_REG_STATUS2           0x08
#define MFRC522_REG_FIFO_DATA         0x09
#define MFRC522_REG_FIFO_LEVEL        0x0A
#define MFRC522_REG_WATER_LEVEL       0x0B
#define MFRC522_REG_CONTROL           0x0C
#define MFRC522_REG_BIT_FRAMING       0x0D
#define MFRC522_REG_COLL              0x0E
#define MFRC522_REG_MODE              0x11
#define MFRC522_REG_TX_MODE           0x12
#define MFRC522_REG_RX_MODE           0x13
#define MFRC522_REG_TX_CONTROL        0x14
#define MFRC522_REG_TX_ASK            0x15
#define MFRC522_REG_TX_SEL            0x16
#define MFRC522_REG_RX_SEL            0x17
#define MFRC522_REG_RX_THRESHOLD      0x18
#define MFRC522_REG_DEMOD             0x19
#define MFRC522_REG_CRC_RESULT_H      0x21
#define MFRC522_REG_CRC_RESULT_L      0x22
#define MFRC522_REG_RF_CFG            0x26
#define MFRC522_REG_GS_N              0x27
#define MFRC522_REG_CW_GS_P           0x28
#define MFRC522_REG_MOD_GS_P          0x29
#define MFRC522_REG_T_MODE            0x2A
#define MFRC522_REG_T_PRESCALER       0x2B
#define MFRC522_REG_T_RELOAD_H        0x2C
#define MFRC522_REG_T_RELOAD_L        0x2D
#define MFRC522_REG_T_COUNTER_VALUE_H 0x2E
#define MFRC522_REG_T_COUNTER_VALUE_L 0x2F

#define MFRC522_MAX_LEN 16

void MFRC522_WriteRegister(uint8_t addr, uint8_t val) {
	addr = (addr << 1) & 0x7E; // Address format: 0XXXXXX0
	HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_RESET); // CS LOW
	HAL_SPI_Transmit(&hspi2, &addr, 1, HAL_MAX_DELAY);
	HAL_SPI_Transmit(&hspi2, &val, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_SET); // CS HIGH
}

uint8_t MFRC522_ReadRegister(uint8_t addr) {
	uint8_t val;
	addr = ((addr << 1) & 0x7E) | 0x80; // Address format: 1XXXXXX0
	HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(&hspi2, &addr, 1, HAL_MAX_DELAY);
	HAL_SPI_Receive(&hspi2, &val, 1, HAL_MAX_DELAY);
	HAL_GPIO_WritePin(RFID_CS_GPIO_Port, RFID_CS_Pin, GPIO_PIN_SET);
	return val;
}

void MFRC522_SetBitMask(uint8_t reg, uint8_t mask) {
	MFRC522_WriteRegister(reg, MFRC522_ReadRegister(reg) | mask);
}

void MFRC522_ClearBitMask(uint8_t reg, uint8_t mask) {
	MFRC522_WriteRegister(reg, MFRC522_ReadRegister(reg) & (~mask));
}

void MFRC522_Init(void) {
	HAL_GPIO_WritePin(RFID_RST_GPIO_Port, RFID_RST_Pin, GPIO_PIN_SET); // Reset Release
	MFRC522_Reset();
	MFRC522_WriteRegister(MFRC522_REG_T_MODE, 0x8D);
	MFRC522_WriteRegister(MFRC522_REG_T_PRESCALER, 0x3E);
	MFRC522_WriteRegister(MFRC522_REG_T_RELOAD_L, 30);
	MFRC522_WriteRegister(MFRC522_REG_T_RELOAD_H, 0);
	MFRC522_WriteRegister(MFRC522_REG_TX_ASK, 0x40);
	MFRC522_WriteRegister(MFRC522_REG_MODE, 0x3D);
	MFRC522_AntennaOn();
}

void MFRC522_Reset(void) {
	MFRC522_WriteRegister(MFRC522_REG_COMMAND, PCD_RESETPHASE);
}

void MFRC522_AntennaOn(void) {
	uint8_t temp = MFRC522_ReadRegister(MFRC522_REG_TX_CONTROL);
	if (!(temp & 0x03)) MFRC522_SetBitMask(MFRC522_REG_TX_CONTROL, 0x03);
}

void MFRC522_AntennaOff(void) {
	MFRC522_ClearBitMask(MFRC522_REG_TX_CONTROL, 0x03);
}

uint8_t MFRC522_Request(uint8_t reqMode, uint8_t *TagType) {
	uint8_t status;
	uint16_t backBits;
	MFRC522_WriteRegister(MFRC522_REG_BIT_FRAMING, 0x07);
	TagType[0] = reqMode;
	status = MFRC522_ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);
	if ((status != MI_OK) || (backBits != 0x10)) status = MI_ERR;
	return status;
}

uint8_t MFRC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen, uint8_t *backData, uint16_t *backLen) {
	uint8_t status = MI_ERR;
	uint8_t irqEn = 0x00;
	uint8_t waitIRq = 0x00;
	uint8_t lastBits;
	uint8_t n;
	uint16_t i;

	switch (command) {
		case PCD_AUTHENT:
			irqEn = 0x12;
			waitIRq = 0x10;
			break;
		case PCD_TRANSCEIVE:
			irqEn = 0x77;
			waitIRq = 0x30;
			break;
		default:
			break;
	}

	MFRC522_WriteRegister(MFRC522_REG_COMM_I_EN, irqEn | 0x80);
	MFRC522_ClearBitMask(MFRC522_REG_COMM_IRQ, 0x80);
	MFRC522_SetBitMask(MFRC522_REG_FIFO_LEVEL, 0x80);
	MFRC522_WriteRegister(MFRC522_REG_COMMAND, PCD_IDLE);

	for (i = 0; i < sendLen; i++) MFRC522_WriteRegister(MFRC522_REG_FIFO_DATA, sendData[i]);

	MFRC522_WriteRegister(MFRC522_REG_COMMAND, command);
	if (command == PCD_TRANSCEIVE) MFRC522_SetBitMask(MFRC522_REG_BIT_FRAMING, 0x80);

	i = 2000;
	do {
		n = MFRC522_ReadRegister(MFRC522_REG_COMM_IRQ);
		i--;
	} while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

	MFRC522_ClearBitMask(MFRC522_REG_BIT_FRAMING, 0x80);

	if (i != 0) {
		if (!(MFRC522_ReadRegister(MFRC522_REG_ERROR) & 0x1B)) {
			status = MI_OK;
			if (n & irqEn & 0x01) status = MI_NOTAGERR;
			if (command == PCD_TRANSCEIVE) {
				n = MFRC522_ReadRegister(MFRC522_REG_FIFO_LEVEL);
				uint8_t lastBits = MFRC522_ReadRegister(MFRC522_REG_CONTROL) & 0x07;
				if (lastBits) *backLen = (n - 1) * 8 + lastBits;
				else *backLen = n * 8;
				if (n == 0) n = 1;
				if (n > MFRC522_MAX_LEN) n = MFRC522_MAX_LEN;
				for (i = 0; i < n; i++) backData[i] = MFRC522_ReadRegister(MFRC522_REG_FIFO_DATA);
			}
		} else status = MI_ERR;
	}
	return status;
}

uint8_t MFRC522_Anticoll(uint8_t *serNum) {
	uint8_t status;
	uint8_t i;
	uint8_t serNumCheck = 0;
	uint16_t unLen;

	MFRC522_WriteRegister(MFRC522_REG_BIT_FRAMING, 0x00);
	serNum[0] = PICC_ANTICOLL;
	serNum[1] = 0x20;
	status = MFRC522_ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

	if (status == MI_OK) {
		for (i = 0; i < 4; i++) serNumCheck ^= serNum[i];
		if (serNumCheck != serNum[4]) status = MI_ERR;
	}
	return status;
}

uint8_t MFRC522_Check(uint8_t *id) {
	uint8_t status;
	status = MFRC522_Request(PICC_REQIDL, id);
	if (status == MI_OK) status = MFRC522_Anticoll(id);
	MFRC522_Halt();
	return status;
}

uint8_t MFRC522_Compare(uint8_t *CardID, uint8_t *CompareID) {
	uint8_t i;
	for (i = 0; i < 5; i++) {
		if (CardID[i] != CompareID[i]) return MI_ERR;
	}
	return MI_OK;
}

void MFRC522_Halt(void) {
	uint16_t unLen;
	uint8_t buff[4];
	buff[0] = PICC_HALT;
	buff[1] = 0;
	MFRC522_CalulateCRC(buff, 2, &buff[2]);
	MFRC522_ToCard(PCD_TRANSCEIVE, buff, 4, buff, &unLen);
}

void MFRC522_CalulateCRC(uint8_t *pIndata, uint8_t len, uint8_t *pOutData) {
	uint8_t i, n;
	MFRC522_ClearBitMask(MFRC522_REG_DIV_IRQ, 0x04);
	MFRC522_SetBitMask(MFRC522_REG_FIFO_LEVEL, 0x80);
	for (i = 0; i < len; i++) MFRC522_WriteRegister(MFRC522_REG_FIFO_DATA, *(pIndata + i));
	MFRC522_WriteRegister(MFRC522_REG_COMMAND, PCD_CALCCRC);
	i = 0xFF;
	do {
		n = MFRC522_ReadRegister(MFRC522_REG_DIV_IRQ);
		i--;
	} while ((i != 0) && !(n & 0x04));
	pOutData[0] = MFRC522_ReadRegister(MFRC522_REG_CRC_RESULT_L);
	pOutData[1] = MFRC522_ReadRegister(MFRC522_REG_CRC_RESULT_H);
}