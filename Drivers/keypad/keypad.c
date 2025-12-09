#include "keypad.h"

// Mapa del teclado 4x4
static const char map4x4[KEYPAD_ROWS][KEYPAD_COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

/*
  set_rows me simplifica el código, me evita repetir for-loops y me ayuda
  a mantener el reposo del teclado (todas LOW) o el modo de escaneo (todas HIGH).
*/
static inline void set_rows(keypad_handle_t* k, GPIO_PinState st){
  for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
    HAL_GPIO_WritePin(k->row_ports[r], k->row_pins[r], st);
  }
}

// Pequeño retraso para asentamiento de señales dentro de la ISR (sin HAL_Delay)
static inline void tiny_delay(volatile uint32_t n){
  while (n--) { __NOP(); }
}

void keypad_init(keypad_handle_t* k){
  // Dejo el teclado en reposo: todas las filas en LOW.
  // Como las columnas tienen pull-up y EXTI por flanco de bajada,
  // cuando presiono una tecla la columna cae y se genera la interrupción.
  set_rows(k, GPIO_PIN_RESET);
}

char keypad_scan(keypad_handle_t* k, uint16_t col_pin_irq){
  // Primero deshabilito EXTI mientras detecto la tecla.
  // Así evito repeticiones por rebotes o por cambios durante el escaneo.
  HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
  HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

  // Intento identificar qué columna quedó realmente en LOW.
  int8_t c = -1;
  for (uint8_t i = 0; i < KEYPAD_COLS; i++) {
    if (k->col_pins[i] == col_pin_irq) { c = i; break; }
  }
  // Si el pin que vino no es fiable, busco leyendo todas las columnas.
  if (c < 0 || HAL_GPIO_ReadPin(k->col_ports[c], k->col_pins[c]) == GPIO_PIN_SET) {
    c = -1;
    for (uint8_t i = 0; i < KEYPAD_COLS; i++) {
      if (HAL_GPIO_ReadPin(k->col_ports[i], k->col_pins[i]) == GPIO_PIN_RESET) { c = i; break; }
    }
    // Si ninguna está en LOW, fue un falso disparo: limpio y re-habilito EXTI.
    if (c < 0) {
      __HAL_GPIO_EXTI_CLEAR_IT(col_pin_irq);
      HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
      HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
      return '\0';
    }
    col_pin_irq = k->col_pins[c];
  }

  GPIO_TypeDef* cport = k->col_ports[c];
  uint16_t      cpin  = k->col_pins[c];

  // Hago un debounce rápido: leo dos veces con un mini delay en medio.
  if (HAL_GPIO_ReadPin(cport, cpin) == GPIO_PIN_SET) {
    __HAL_GPIO_EXTI_CLEAR_IT(col_pin_irq);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    return '\0';
  }
  tiny_delay(800);
  if (HAL_GPIO_ReadPin(cport, cpin) == GPIO_PIN_SET) {
    __HAL_GPIO_EXTI_CLEAR_IT(col_pin_irq);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
    return '\0';
  }

  // Ahora escaneo filas: pongo todas HIGH y voy bajando una por una.
  // Si la columna detectada sigue en LOW con esa fila, ya sé qué tecla es.
  char key = '\0';
  set_rows(k, GPIO_PIN_SET);
  for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
    HAL_GPIO_WritePin(k->row_ports[r], k->row_pins[r], GPIO_PIN_RESET);
    tiny_delay(300);
    if (HAL_GPIO_ReadPin(cport, cpin) == GPIO_PIN_RESET) {
      key = map4x4[r][c];
    }
    HAL_GPIO_WritePin(k->row_ports[r], k->row_pins[r], GPIO_PIN_SET);
    if (key) break;
  }
  // Vuelvo a reposo: todas las filas LOW.
  set_rows(k, GPIO_PIN_RESET);

  // Si se detecto una tecla, espero a que la suelten (columna vuelva a HIGH),
  // limpio el pending de EXTI y re-habilito las interrupciones.
  if (key) {
    while (HAL_GPIO_ReadPin(cport, cpin) == GPIO_PIN_RESET) { __NOP(); }
  }
  __HAL_GPIO_EXTI_CLEAR_IT(col_pin_irq);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  return key;
}