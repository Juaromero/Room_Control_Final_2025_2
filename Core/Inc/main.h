/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define RFID_MISO_Pin GPIO_PIN_2
#define RFID_MISO_GPIO_Port GPIOC
#define RFID_MOSI_Pin GPIO_PIN_3
#define RFID_MOSI_GPIO_Port GPIOC
#define LED_ROJO_Pin GPIO_PIN_1
#define LED_ROJO_GPIO_Port GPIOA
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define LED_VERDE_Pin GPIO_PIN_4
#define LED_VERDE_GPIO_Port GPIOA
#define LD2_Pin GPIO_PIN_5
#define LD2_GPIO_Port GPIOA
#define SERVO_PWM_Pin GPIO_PIN_6
#define SERVO_PWM_GPIO_Port GPIOA
#define RFID_CS_Pin GPIO_PIN_4
#define RFID_CS_GPIO_Port GPIOC
#define RFID_RST_Pin GPIO_PIN_5
#define RFID_RST_GPIO_Port GPIOC
#define LED_AZUL_Pin GPIO_PIN_0
#define LED_AZUL_GPIO_Port GPIOB
#define KEYPAD_C1_Pin GPIO_PIN_10
#define KEYPAD_C1_GPIO_Port GPIOB
#define KEYPAD_C1_EXTI_IRQn EXTI15_10_IRQn
#define RFID_SCK_Pin GPIO_PIN_13
#define RFID_SCK_GPIO_Port GPIOB
#define KEYPAD_C4_Pin GPIO_PIN_7
#define KEYPAD_C4_GPIO_Port GPIOC
#define KEYPAD_C4_EXTI_IRQn EXTI9_5_IRQn
#define KEYPAD_C2_Pin GPIO_PIN_8
#define KEYPAD_C2_GPIO_Port GPIOA
#define KEYPAD_C2_EXTI_IRQn EXTI9_5_IRQn
#define KEYPAD_C3_Pin GPIO_PIN_9
#define KEYPAD_C3_GPIO_Port GPIOA
#define KEYPAD_C3_EXTI_IRQn EXTI9_5_IRQn
#define KEYPAD_R1_Pin GPIO_PIN_10
#define KEYPAD_R1_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define KEYPAD_R2_Pin GPIO_PIN_3
#define KEYPAD_R2_GPIO_Port GPIOB
#define KEYPAD_R4_Pin GPIO_PIN_4
#define KEYPAD_R4_GPIO_Port GPIOB
#define KEYPAD_R3_Pin GPIO_PIN_5
#define KEYPAD_R3_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
