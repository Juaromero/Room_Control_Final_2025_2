/*
 * room_control.h
 * ACTUALIZADO
 */
#ifndef ROOM_CONTROL_H
#define ROOM_CONTROL_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

// Configuración del Sistema
#define PASSWORD_LENGTH 4
#define MAX_PIN_RETRIES 3
#define DOOR_OPEN_DURATION_MS 5000  // 5 segundos abierta
#define LOCKOUT_DURATION_MS 10000   // 10 segundos bloqueo tras error

// Configuración Servo (Ajustar según tu motor SG90 y Timer)
#define SERVO_CLOSED_CCR 1000 // 1ms (0 grados)
#define SERVO_OPEN_CCR   2000 // 2ms (180 grados)

// Estados del sistema
typedef enum {
    ROOM_STATE_WAITING_RFID,    // Estado inicial: Esperando tarjeta
    ROOM_STATE_INPUT_PIN,       // Tarjeta OK: Esperando PIN
    ROOM_STATE_CHECK_PIN,       // Validando PIN
    ROOM_STATE_UNLOCKED,        // Abriendo puerta
    ROOM_STATE_ACCESS_DENIED,   // Error (PIN incorrecto o Tarjeta inválida)
    ROOM_STATE_SYSTEM_LOCKOUT   // Bloqueo por múltiples intentos fallidos
} room_state_t;

// Estructura de control principal
typedef struct {
    room_state_t current_state;
    
    // Autenticación
    char stored_password[PASSWORD_LENGTH + 1];
    char input_buffer[PASSWORD_LENGTH + 1];
    uint8_t input_index;
    uint8_t failed_attempts;
    
    // Tiempos
    uint32_t last_activity_time;
    uint32_t state_enter_time;
    
    // Hardware Flags
    bool display_update_needed;
    
} room_control_t;

// --- Funciones Públicas ---
void room_control_init(room_control_t *room);
void room_control_update(room_control_t *room);
void room_control_process_key(room_control_t *room, char key);
void room_control_remote_byte(room_control_t *room, uint8_t byte);
void room_control_process_remote(room_control_t *room, const char *cmd);

// Esta función es necesaria si quieres consultar el estado desde main.c
room_state_t room_control_get_state(room_control_t *room);

#endif /* ROOM_CONTROL_H */