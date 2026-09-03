#ifndef UART_ADAPTER_H
#define UART_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 2.12 UART Device Interface */
int  uart_adapter_init(const char *device_path);
void uart_adapter_shutdown(void);

int  uart_adapter_send(const uint8_t *data, size_t len);
int  uart_adapter_receive(uint8_t *buf, size_t max_len);

const char* uart_adapter_status(void);
bool uart_adapter_is_simulated(void);

/* Instrumentation helpers */
int  uart_adapter_trace_rx(uint32_t act_id, uint32_t corr_id, uint8_t byte);
int  uart_adapter_trace_tx(uint32_t act_id, uint32_t corr_id, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* UART_ADAPTER_H */
