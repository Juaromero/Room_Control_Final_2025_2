#include "room_control.h"
#include "rc522.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "ring_buffer.h"  
#include <stdio.h>
#include <string.h>

// --- VARIABLES EXTERNAS (Vienen del main.c) ---
extern TIM_HandleTypeDef htim3;   // Servo en TIM3 Canal 1
extern UART_HandleTypeDef huart1; // ESP-01 en UART1
extern ring_buffer_t keypad_rb;   // Buffer del teclado (YA DEBE EXISTIR EN TU MAIN)

// --- CONFIGURACIÓN ---
static const char VALID_PASSWORD[] = "1234"; // Clave correcta
// UID de tarjeta Maestra (Cámbialo si tu tarjeta no abre)
static const uint8_t MY_VALID_CARD[5] = {0x33, 0x65, 0x08, 0x12, 0x90}; 

// --- FUNCIÓN SERVO (Grados a PWM) ---
void room_control_set_servo(uint8_t angle) {
    if (angle > 180) angle = 180;
    // Cálculo para 50Hz (ARR=19999, PSC=79)
    // 0 grados = 500us, 180 grados = 2500us
    uint32_t pulse = 500 + (angle * 2000 / 180);
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, pulse);
}

// --- INICIALIZACIÓN ---
void room_control_init(room_control_t *room) {
    room->current_state = STATE_IDLE;
    room->input_index = 0;
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    memset(room->cardID, 0, 5);
    
    // Hardware Estado Inicial (Puerta Cerrada)
    room_control_set_servo(0); 
    HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_RESET); // LED OFF
    
    // OLED: Mensaje Inicial
    ssd1306_Fill(Black);
    ssd1306_SetCursor(10, 10);
    ssd1306_WriteString("BIENVENIDO", Font_7x10, White);
    ssd1306_SetCursor(10, 30);
    ssd1306_WriteString("Acerque Tarjeta", Font_6x8, White);
    ssd1306_UpdateScreen();
    
    // Limpiar buffer del teclado para evitar pulsaciones viejas
    uint8_t dummy;
    while(ring_buffer_get(&keypad_rb, &dummy) == 0); 
}

// --- UPDATE LOOP ---
void room_control_update(room_control_t *room) {
    char msg_buffer[64];
    uint8_t rb_val = 0;
    char key_char = 0;

    switch (room->current_state) {
        
        // 1. ESPERANDO TARJETA
        case STATE_IDLE:
            if (MFRC522_Check(room->cardID) == 0) { 
                // Tarjeta detectada -> Pasamos al siguiente estado
                room->current_state = STATE_CARD_OK;
                room->state_enter_time = HAL_GetTick();
            }
            break;

        // 2. TARJETA OK: ABRIR PUERTA Y PEDIR CLAVE
        case STATE_CARD_OK:
            // Acción Física: Servo 90 grados, LED ON
            room_control_set_servo(90);
            HAL_GPIO_WritePin(DOOR_STATUS_GPIO_Port, DOOR_STATUS_Pin, GPIO_PIN_SET);
            
            // Acción Visual: Pedir Clave
            ssd1306_Fill(Black);
            ssd1306_SetCursor(10, 10);
            ssd1306_WriteString("ACCESO OK", Font_11x18, White);
            ssd1306_SetCursor(0, 40);
            ssd1306_WriteString("Clave:", Font_7x10, White);
            ssd1306_UpdateScreen();
            
            room->input_index = 0;
            memset(room->input_buffer, 0, sizeof(room->input_buffer));
            room->current_state = STATE_WAITING_KEY;
            room->state_enter_time = HAL_GetTick();
            break;

        // 3. ESPERANDO TECLADO (Leyendo Buffer)
        case STATE_WAITING_KEY:
            // Leemos del Ring Buffer (NO BLOQUEANTE)
            if (ring_buffer_get(&keypad_rb, &rb_val) == 0) {
                key_char = (char)rb_val;
                
                if (key_char == '#') { // Tecla Enter
                    if (strcmp(room->input_buffer, VALID_PASSWORD) == 0) {
                        room->current_state = STATE_SENDING_DATA; // Clave Correcta
                    } else {
                        // Clave Incorrecta
                        ssd1306_Fill(Black);
                        ssd1306_WriteString("CLAVE MAL", Font_11x18, White);
                        ssd1306_UpdateScreen();
                        HAL_Delay(1500);
                        room->current_state = STATE_RESET;
                    }
                } 
                else if (key_char == '*') { // Tecla Borrar
                    room->input_index = 0;
                    memset(room->input_buffer, 0, sizeof(room->input_buffer));
                }
                else if (room->input_index < PASSWORD_LENGTH) { // Digitar número
                    room->input_buffer[room->input_index++] = key_char;
                    // Mostrar asterisco
                    ssd1306_SetCursor(10 + (room->input_index * 10), 50);
                    ssd1306_WriteString("*", Font_11x18, White);
                    ssd1306_UpdateScreen();
                }
            }
            // Timeout de 15 segundos si no escribe
            if (HAL_GetTick() - room->state_enter_time > 15000) {
                room->current_state = STATE_RESET;
            }
            break;

        // 4. ENVIAR A ESP-01
        case STATE_SENDING_DATA:
            ssd1306_Fill(Black);
            ssd1306_WriteString("ENVIANDO...", Font_7x10, White);
            ssd1306_UpdateScreen();
            
            // Construir mensaje UART: "UID:XXXX PIN:1234"
            sprintf(msg_buffer, "UID:%02X%02X PIN:%s\r\n", 
                    room->cardID[0], room->cardID[1], room->input_buffer);
            
            // Enviar mensaje
            HAL_UART_Transmit(&huart1, (uint8_t*)msg_buffer, strlen(msg_buffer), 500);
            
            HAL_Delay(1000);
            room->current_state = STATE_RESET;
            break;

        // 5. RESETEAR SISTEMA
        case STATE_RESET:
            room_control_init(room); // Cierra puerta, apaga LED, vuelve a inicio
            break;
    }
}