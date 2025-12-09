#include "ring_buffer.h"

static inline uint16_t advance_index(uint16_t idx, uint16_t capacity) {
    return (uint16_t)((idx + 1) % capacity);
}

void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, uint16_t capacity) 
{
    rb->buffer   = buffer;
    rb->capacity = capacity ? capacity : 1; // evitar div/0 en %
    rb->head     = 0;
    rb->tail     = 0;
}

bool ring_buffer_write(ring_buffer_t *rb, uint8_t data)
{
    uint16_t next_head = advance_index(rb->head, rb->capacity);

    // Lleno si el próximo head alcanzaría al tail
    if (next_head == rb->tail) {
        return false;
    }

    rb->buffer[rb->head] = data;
    rb->head = next_head;
    return true;
}

bool ring_buffer_read(ring_buffer_t *rb, uint8_t *data)
{
    // Vacío si head == tail
    if (rb->head == rb->tail) {
        return false;
    }

    *data = rb->buffer[rb->tail];
    rb->tail = advance_index(rb->tail, rb->capacity);
    return true;
}

uint16_t ring_buffer_count(ring_buffer_t *rb)
{
    // Si head >= tail: elementos = head - tail
    // Si head < tail : elementos = capacity - (tail - head)
    if (rb->head >= rb->tail) {
        return (uint16_t)(rb->head - rb->tail);
    } else {
        return (uint16_t)(rb->capacity - (rb->tail - rb->head));
    }
}

bool ring_buffer_is_empty(ring_buffer_t *rb)
{
    return (rb->head == rb->tail);
}

bool ring_buffer_is_full(ring_buffer_t *rb)
{
    // Lleno si avanzar head alcanzaría al tail
    return (advance_index(rb->head, rb->capacity) == rb->tail);
}

void ring_buffer_flush(ring_buffer_t *rb)
{
    // Vaciar: mover tail a head (conserva el búfer)
    rb->tail = rb->head;
}