#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "rc522.h"
#include "servo_control.h"
#include <stdio.h>
#include <string.h>
#include "locks_bitmaps.h"

extern TIM_HandleTypeDef htim3; 
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

// --- CONFIGURACIÓN ---
uint8_t VALID_CARD_UID[5] = {0x1A, 0x22, 0x73, 0x80, 0xCB}; 
servo_t door_servo;

// --- PROTOTIPOS ---
static void room_control_change_state(room_control_t *room, room_state_t new_state);
static void room_control_update_display(room_control_t *room);
static void process_rfid_check(room_control_t *room);
static void control_servo_action(bool open);
static void send_log_to_esp(const char* message);
static void clear_input(room_control_t *room);
static void set_rgb_color(bool r, bool g, bool b);

// --- IMPLEMENTACIÓN ---

void room_control_init(room_control_t *room) {
    strcpy(room->stored_password, "1234");
    room->failed_attempts = 0;
    clear_input(room);
    
    MFRC522_Init();
    servo_init(&door_servo, &htim3, TIM_CHANNEL_1);
    
    send_log_to_esp("INIT: Test RGB...");
    
    // Iniciar en 5 grados para evitar el tope mecanico
    servo_set_angle(&door_servo, 5); 
    
    // Test rápido de colores
    set_rgb_color(1, 0, 0); HAL_Delay(200); // Rojo
    set_rgb_color(0, 1, 0); HAL_Delay(200); // Verde
    set_rgb_color(0, 0, 1); HAL_Delay(200); // Azul
    set_rgb_color(0, 0, 0); // Off

    HAL_Delay(500); 
    send_log_to_esp("SISTEMA: Listo.");
    room_control_change_state(room, ROOM_STATE_WAITING_RFID);
}

void room_control_update(room_control_t *room) {
    uint32_t current_time = HAL_GetTick();
    static uint32_t last_debug_print = 0;

    switch (room->current_state) {
        case ROOM_STATE_WAITING_RFID:
            process_rfid_check(room);
            break;

        case ROOM_STATE_INPUT_PIN:
            if (current_time - room->last_activity_time > 15000) {
                send_log_to_esp("TIMEOUT PIN");
                room_control_change_state(room, ROOM_STATE_WAITING_RFID);
            }
            break;

        case ROOM_STATE_UNLOCKED:
            if (current_time - last_debug_print > 1000) { 
                char msg[64];
                uint32_t elapsed = current_time - room->state_enter_time;
                sprintf(msg, "ABIERTA: %lu/%d s", elapsed/1000, DOOR_OPEN_DURATION_MS/1000);
                send_log_to_esp(msg);
                last_debug_print = current_time;
            }
            if (current_time - room->state_enter_time > DOOR_OPEN_DURATION_MS) {
                room_control_change_state(room, ROOM_STATE_WAITING_RFID);
            }
            break;

        case ROOM_STATE_ACCESS_DENIED:
            if (current_time - room->state_enter_time > 3000) {
                room_control_change_state(room, ROOM_STATE_WAITING_RFID);
            }
            break;
            
        case ROOM_STATE_SYSTEM_LOCKOUT:
            if (current_time - room->state_enter_time > LOCKOUT_DURATION_MS) {
                room->failed_attempts = 0; 
                room_control_change_state(room, ROOM_STATE_WAITING_RFID);
            }
            break;

        default: break;
    }

    if (room->display_update_needed) {
        room_control_update_display(room);
        room->display_update_needed = false;
    }
}

void room_control_process_key(room_control_t *room, char key) {
    room->last_activity_time = HAL_GetTick();

    if (room->current_state == ROOM_STATE_INPUT_PIN) {
        char msg[20]; sprintf(msg, "Tecla: %c", key); send_log_to_esp(msg);

        if (room->input_index < PASSWORD_LENGTH) {
            room->input_buffer[room->input_index++] = key;
            room->input_buffer[room->input_index] = '\0'; 
            room->display_update_needed = true;

            if (room->input_index == PASSWORD_LENGTH) {
                room_control_change_state(room, ROOM_STATE_CHECK_PIN);
            }
        }
    }
}

static void room_control_change_state(room_control_t *room, room_state_t new_state) {
    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->last_activity_time = HAL_GetTick();
    room->display_update_needed = true;

    switch (new_state) {
        case ROOM_STATE_WAITING_RFID:
            send_log_to_esp("CERRANDO...");
            set_rgb_color(0, 0, 0); // APAGADO
            
            // Cierra la puerta ANTES de borrar variables
            control_servo_action(false); 
            
            clear_input(room);
            MFRC522_Init(); 
            HAL_Delay(500); // Reducido para que no se sienta lento
            send_log_to_esp("--- LISTO PARA LEER ---");
            break;

        case ROOM_STATE_INPUT_PIN:
            set_rgb_color(0, 0, 1); // AZUL
            break;

        case ROOM_STATE_CHECK_PIN:
            if (strcmp(room->input_buffer, room->stored_password) == 0) {
                room_control_change_state(room, ROOM_STATE_UNLOCKED);
            } else {
                room->failed_attempts++;
                send_log_to_esp("ERROR: PIN Incorrecto");
                if (room->failed_attempts >= MAX_PIN_RETRIES) {
                    room_control_change_state(room, ROOM_STATE_SYSTEM_LOCKOUT);
                } else {
                    room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                }
            }
            break;

        case ROOM_STATE_UNLOCKED:
            send_log_to_esp("ACCESO CONCEDIDO");
            set_rgb_color(0, 1, 0); // VERDE
            control_servo_action(true); 
            room->failed_attempts = 0; 
            break;

        case ROOM_STATE_ACCESS_DENIED:
            send_log_to_esp("ACCESO DENEGADO");
            set_rgb_color(1, 0, 0); // ROJO
            clear_input(room);
            break;
            
        case ROOM_STATE_SYSTEM_LOCKOUT:
            set_rgb_color(1, 0, 0); // ROJO FIJO
            break;
            
        default: break;
    }
}

static void process_rfid_check(room_control_t *room) {
    uint8_t str[5]; 
    if (MFRC522_Request(0x52, str) == MI_OK) {
        if (MFRC522_Anticoll(str) == MI_OK) {
            char log_msg[64];
            sprintf(log_msg, "RFID: %02X %02X %02X %02X %02X", str[0], str[1], str[2], str[3], str[4]);
            send_log_to_esp(log_msg);
            
            if (MFRC522_Compare(str, VALID_CARD_UID) == MI_OK) {
                room_control_change_state(room, ROOM_STATE_INPUT_PIN);
            } else {
                room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
            }
            HAL_Delay(1000);
        }
    }
}

// --- CONTROLADOR LED RGB (ÁNODO COMÚN / CABLEADO PA1, PA4, PB0) ---
static void set_rgb_color(bool r, bool g, bool b) {
    // ROJO -> PA1
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, r ? GPIO_PIN_RESET : GPIO_PIN_SET);
    // VERDE -> PB0 (Intercambiado por tu cableado)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, g ? GPIO_PIN_RESET : GPIO_PIN_SET);
    // AZUL -> PA4 (Intercambiado por tu cableado)
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, b ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

// --- AJUSTE DE MOVIMIENTO DEL SERVO ---
static void control_servo_action(bool open) {
    if (open) {
        // Abrir a 90 grados, velocidad normal (10ms por grado = suave)
        servo_move_slow(&door_servo, 90, 10); 
    } else {
        // Cerrar a 5 grados (NO a 0) para evitar que se trabe mecánicamente
        // Velocidad 10ms para que no se vea a saltos
        servo_move_slow(&door_servo, 5, 10);
    }
}

static void send_log_to_esp(const char* message) {
    char buffer[128];
    int len = snprintf(buffer, sizeof(buffer), "[LOG] %s\r\n", message);
    if (len < 0) return;
    if (len > (int)sizeof(buffer)) len = sizeof(buffer);

    // Consola local (ST-Link, USART2)
    HAL_UART_Transmit(&huart2, (uint8_t*)buffer, len, 100);

    // Consola remota (ESP-01, USART3)
    HAL_UART_Transmit(&huart3, (uint8_t*)buffer, len, 100);
}

static void clear_input(room_control_t *room) {
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    room->input_index = 0;
}

room_state_t room_control_get_state(room_control_t *room) {
    return room->current_state;
}

// --- PANTALLA (DISEÑO ORIGINAL MANTENIDO) ---
static void room_control_update_display(room_control_t *room) {
    ssd1306_Fill(Black);
    
    switch (room->current_state) {
        case ROOM_STATE_WAITING_RFID:
            ssd1306_SetCursor(10, 5);
            ssd1306_WriteString("BIENVENIDO", Font_11x18, White);
            ssd1306_SetCursor(10, 25);
            ssd1306_WriteString("Acerque su", Font_11x18, White);
            ssd1306_SetCursor(25, 40);
            ssd1306_WriteString("Tarjeta", Font_11x18, White);
            break;
            
        case ROOM_STATE_INPUT_PIN:
            ssd1306_SetCursor(5, 5);
            ssd1306_WriteString("INGRESE PIN:", Font_7x10, White);
            ssd1306_SetCursor(30, 30);
            for(int i=0; i<room->input_index; i++) {
                ssd1306_WriteChar('*', Font_11x18, White);
            }
            break;
            
        case ROOM_STATE_UNLOCKED:
            ssd1306_DrawBitmap(48, 0, lock_open_bitmap, LOCK_WIDTH, LOCK_HEIGHT, White);
            ssd1306_SetCursor(43, 36); 
            ssd1306_WriteString("ACCESO", Font_7x10, White);
            ssd1306_SetCursor(32, 48); 
            ssd1306_WriteString("PERMITIDO", Font_7x10, White);
            break;
            
        case ROOM_STATE_ACCESS_DENIED:
            ssd1306_DrawBitmap(48, 0, lock_closed_bitmap, LOCK_WIDTH, LOCK_HEIGHT, White);
            ssd1306_SetCursor(43, 36);
            ssd1306_WriteString("ACCESO", Font_7x10, White);
            ssd1306_SetCursor(36, 48);
            ssd1306_WriteString("DENEGADO", Font_7x10, White);
            break;
            
        case ROOM_STATE_SYSTEM_LOCKOUT:
            ssd1306_SetCursor(20, 10);
            ssd1306_WriteString("SISTEMA", Font_11x18, White);
            ssd1306_SetCursor(15, 35);
            ssd1306_WriteString("BLOQUEADO", Font_11x18, White);
            break;

        default: break;
    }
    ssd1306_UpdateScreen();
}

void room_control_process_remote(room_control_t *room, const char *cmd)
{
    if (strcmp(cmd, "OPEN") == 0) {
        room_control_change_state(room, ROOM_STATE_UNLOCKED);
        send_log_to_esp("REMOTE: OPEN");
        return;
    }

    if (strcmp(cmd, "CLOSE") == 0) {
        room_control_change_state(room, ROOM_STATE_WAITING_RFID);
        send_log_to_esp("REMOTE: CLOSE");
        return;
    }

    if (strcmp(cmd, "LOCKOUT") == 0) {
        room_control_change_state(room, ROOM_STATE_SYSTEM_LOCKOUT);
        send_log_to_esp("REMOTE: LOCKOUT");
        return;
    }

    if (strcmp(cmd, "STATUS") == 0) {
        char msg[32];
        sprintf(msg, "STATUS:%d", room->current_state);
        send_log_to_esp(msg);
        return;
    }

    // Cambiar contraseña
    if (strncmp(cmd, "SET_PASS:", 9) == 0) {
        const char *nueva = cmd + 9;

        if (strlen(nueva) == 4) {
            strcpy(room->stored_password, nueva);
            send_log_to_esp("CLAVE ACTUALIZADA");
        } else {
            send_log_to_esp("ERROR: PIN INVALIDO");
        }
    }
}