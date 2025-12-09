#include "servo_control.h"

// Mapeo simple de Arduino (long map)
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void servo_init(servo_t *servo, TIM_HandleTypeDef *htim, uint32_t channel) {
    servo->htim = htim;
    servo->channel = channel;
    HAL_TIM_PWM_Start(servo->htim, servo->channel);
}

void servo_set_angle(servo_t *servo, uint8_t angle) {
    if (angle > 180) angle = 180;
    
    // Calcular el valor del pulso (CCR)
    uint32_t pulse = map(angle, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);
    
    __HAL_TIM_SET_COMPARE(servo->htim, servo->channel, pulse);
}

void servo_open_door(servo_t *servo) {
    servo_set_angle(servo, 90); // O 180, depende de tu instalación física
}

void servo_close_door(servo_t *servo) {
    servo_set_angle(servo, 0);
}

// speed_delay: milisegundos entre cada grado (ej. 20ms)
void servo_move_slow(servo_t *servo, uint8_t target_angle, uint8_t speed_delay) {
    if (target_angle > 180) target_angle = 180;

    // Si estamos en 0 y queremos ir a 90
    if (servo->current_angle < target_angle) {
        for (int i = servo->current_angle; i <= target_angle; i++) {
            servo_set_angle(servo, i);
            HAL_Delay(speed_delay);
        }
    } 
    // Si estamos en 90 y queremos ir a 0
    else {
        for (int i = servo->current_angle; i >= target_angle; i--) {
            servo_set_angle(servo, i);
            HAL_Delay(speed_delay);
        }
    }
}