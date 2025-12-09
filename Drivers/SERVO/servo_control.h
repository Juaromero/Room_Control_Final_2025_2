#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include "main.h"

// Definiciones para SG90
#define SERVO_MIN_PULSE 1000  // 0 grados
#define SERVO_MAX_PULSE 2000  // 180 grados

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t current_angle; // <--- ¡ESTA ES LA LÍNEA QUE FALTABA!
} servo_t;

void servo_init(servo_t *servo, TIM_HandleTypeDef *htim, uint32_t channel);
void servo_set_angle(servo_t *servo, uint8_t angle);
// Prototipo de la función lenta
void servo_move_slow(servo_t *servo, uint8_t target_angle, uint8_t speed_delay);

#endif