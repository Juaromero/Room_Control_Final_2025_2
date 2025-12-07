#ifndef ROOM_CONTROL_H
#define ROOM_CONTROL_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define PASSWORD_LENGTH 4

// Estados del sistema (Lógica nueva)
typedef enum {
    STATE_IDLE,           // Esperando tarjeta
    STATE_CARD_OK,        // Tarjeta leída OK -> Accionar Servo/LED
    STATE_WAITING_KEY,    // Esperando clave del teclado
    STATE_SENDING_DATA,   // Enviando a ESP-01
    STATE_RESET           // Reiniciando
} system_state_t;

typedef struct {
    system_state_t current_state;
    
    // Datos
    uint8_t cardID[5];
    char input_buffer[PASSWORD_LENGTH + 1];
    uint8_t input_index;
    
    // Tiempos
    uint32_t state_enter_time;
    
} room_control_t;

// Funciones públicas
void room_control_init(room_control_t *room);
void room_control_update(room_control_t *room);
void room_control_set_servo(uint8_t angle);

#endif