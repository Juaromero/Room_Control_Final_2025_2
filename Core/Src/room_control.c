#include "room_control.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "rc522.h"
#include "servo_control.h"
#include <stdio.h> // Necesario para sprintf

// Variables externas del hardware
extern TIM_HandleTypeDef htim3; 
extern UART_HandleTypeDef huart2;

// --- AQUI PONDRAS TU CODIGO ---
// Por ahora déjalo así. En el monitor serial verás los números reales.
uint8_t VALID_CARD_UID[5] = {0x1A, 0x22, 0x73, 0x80, 0xCB};

// Instancia del Servo
servo_t door_servo;

// --- Prototipos Privados ---
static void room_control_change_state(room_control_t *room, room_state_t new_state);
static void room_control_update_display(room_control_t *room);
static void process_rfid_check(room_control_t *room);
static void control_servo_action(bool open);
static void send_log_to_esp(const char* message);
static void clear_input(room_control_t *room);

// --- Implementación Pública ---

void room_control_init(room_control_t *room) {

    strcpy(room->stored_password, "1234"); // Tu contraseña del teclado
    room->failed_attempts = 0;
    clear_input(room);
    
    // Inicializar Drivers
    MFRC522_Init();
    servo_init(&door_servo, &htim3, TIM_CHANNEL_1);
    
    // Estado inicial: Puerta cerrada
    // Usamos set_angle directo al inicio para asegurar posición cerrada rápido
    servo_set_angle(&door_servo, 0); 
    
    send_log_to_esp("SISTEMA: Listo. Acerque su tarjeta...");
    room_control_change_state(room, ROOM_STATE_WAITING_RFID);
}

void room_control_update(room_control_t *room) {
    uint32_t current_time = HAL_GetTick();

    switch (room->current_state) {
        case ROOM_STATE_WAITING_RFID:
            process_rfid_check(room);
            break;

        case ROOM_STATE_INPUT_PIN:
            // Timeout de 10s si no escribe nada
            if (current_time - room->last_activity_time > 10000) {
                send_log_to_esp("TIMEOUT: Volviendo a inicio");
                room_control_change_state(room, ROOM_STATE_WAITING_RFID);
            }
            break;

        case ROOM_STATE_UNLOCKED:
            // Mantener abierta 5 segundos (DOOR_OPEN_DURATION_MS)
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
        // Feedback en serial para ver qué tecla presionas
        char msg[20];
        sprintf(msg, "Tecla: %c", key);
        send_log_to_esp(msg);

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

// --- Funciones Privadas ---

static void room_control_change_state(room_control_t *room, room_state_t new_state) {
    room->current_state = new_state;
    room->state_enter_time = HAL_GetTick();
    room->last_activity_time = HAL_GetTick();
    room->display_update_needed = true;

    switch (new_state) {
        case ROOM_STATE_WAITING_RFID:
            control_servo_action(false); // Cierra la puerta
            clear_input(room);
            break;

        case ROOM_STATE_CHECK_PIN:
            if (strcmp(room->input_buffer, room->stored_password) == 0) {
                room_control_change_state(room, ROOM_STATE_UNLOCKED);
            } else {
                room->failed_attempts++;
                send_log_to_esp("ERROR: PIN Incorrecto");
                
                if (room->failed_attempts >= MAX_PIN_RETRIES) {
                    send_log_to_esp("ALERTA: Sistema Bloqueado");
                    room_control_change_state(room, ROOM_STATE_SYSTEM_LOCKOUT);
                } else {
                    room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
                }
            }
            break;

        case ROOM_STATE_UNLOCKED:
            send_log_to_esp("ACCESO CONCEDIDO: Abriendo puerta...");
            control_servo_action(true); // Abre suavemente
            room->failed_attempts = 0; 
            break;

        case ROOM_STATE_ACCESS_DENIED:
            clear_input(room);
            break;
            
        default: break;
    }
}

static void process_rfid_check(room_control_t *room) {
    uint8_t str[5]; 

    // Usamos 0x52 (REQALL) para mejor detección
    if (MFRC522_Request(0x52, str) == MI_OK) {
        if (MFRC522_Anticoll(str) == MI_OK) {
            
            // --- AQUI ESTA LA CLAVE: Imprime los 4 bytes ---
            char log_msg[64];
            sprintf(log_msg, "COPIA ESTO -> 0x%02X, 0x%02X, 0x%02X, 0x%02X", 
                    str[0], str[1], str[2], str[3]);
            send_log_to_esp(log_msg);
            // ------------------------------------------------
            
            if (MFRC522_Compare(str, VALID_CARD_UID) == MI_OK) {
                send_log_to_esp("TARJETA OK -> Ingrese PIN");
                room_control_change_state(room, ROOM_STATE_INPUT_PIN);
            } else {
                send_log_to_esp("TARJETA RECHAZADA (No está en la lista)");
                room_control_change_state(room, ROOM_STATE_ACCESS_DENIED);
            }
            
            HAL_Delay(1000); // Pausa para no leer mil veces la misma
        }
    }
}

static void control_servo_action(bool open) {
    if (open) {
        // MOVIMIENTO LENTO: Abre a 90 grados, 20ms de espera por grado
        // Si tu puerta abre a 180, cambia 90 por 180
        servo_move_slow(&door_servo, 90, 20); 
    } else {
        // Cierra suavemente a 0 grados
        servo_move_slow(&door_servo, 0, 20);
    }
}

static void send_log_to_esp(const char* message) {
    char buffer[128];
    sprintf(buffer, "[LOG] %s\r\n", message);
    HAL_UART_Transmit(&huart2, (uint8_t*)buffer, strlen(buffer), 100);
}

static void clear_input(room_control_t *room) {
    memset(room->input_buffer, 0, sizeof(room->input_buffer));
    room->input_index = 0;
}

static void room_control_update_display(room_control_t *room) {
    ssd1306_Fill(Black);
    
    // Coordenadas (0,0) para asegurar que se vea en pantallas partidas
    switch (room->current_state) {
        case ROOM_STATE_WAITING_RFID:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("BIENVENIDO", Font_7x10, White);
            ssd1306_SetCursor(0, 12);
            ssd1306_WriteString("Pase Tarjeta", Font_7x10, White);
            break;
        case ROOM_STATE_INPUT_PIN:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("PIN:", Font_7x10, White);
            ssd1306_SetCursor(35, 0);
            for(int i=0; i<room->input_index; i++) ssd1306_WriteChar('*', Font_7x10, White);
            break;
        case ROOM_STATE_UNLOCKED:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("ACCESO", Font_7x10, White);
            ssd1306_SetCursor(0, 12);
            ssd1306_WriteString("CONCEDIDO", Font_7x10, White);
            break;
        case ROOM_STATE_ACCESS_DENIED:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("DENEGADO", Font_7x10, White);
            break;
        case ROOM_STATE_SYSTEM_LOCKOUT:
            ssd1306_SetCursor(0, 0);
            ssd1306_WriteString("BLOQUEADO", Font_7x10, White);
            break;
        default: break;
    }
    ssd1306_UpdateScreen();
}