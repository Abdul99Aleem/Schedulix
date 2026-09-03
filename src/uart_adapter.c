#include "uart_adapter.h"
#include "trace_collector.h"
#include "trace_schema.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#if !defined(_WIN32)
#include <termios.h>
#endif

static int g_fd = -1;
static bool g_simulated = true;
static char g_status[128] = "uninitialized";
static char g_dev_path[256] = "";

/* Simple circular queue for simulation loopback */
#define SIM_QUEUE_SZ 256
static uint8_t g_sim_queue[SIM_QUEUE_SZ];
static size_t g_sim_head = 0;
static size_t g_sim_tail = 0;

int uart_adapter_init(const char *device_path) {
    g_fd = -1;
    g_simulated = true;
    g_sim_head = 0;
    g_sim_tail = 0;

    if (device_path && strlen(device_path) > 0) {
        strncpy(g_dev_path, device_path, sizeof(g_dev_path) - 1);
    } else {
        strcpy(g_dev_path, "/dev/ser1");
    }

#if !defined(_WIN32)
    /* QNX UART Initialization */
    g_fd = open(g_dev_path, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (g_fd >= 0) {
        struct termios tty;
        memset(&tty, 0, sizeof(tty));
        if (tcgetattr(g_fd, &tty) == 0) {
            cfsetospeed(&tty, B115200);
            cfsetispeed(&tty, B115200);
            tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; // 8-bit chars
            tty.c_iflag &= ~IGNBRK;                     // disable break processing
            tty.c_lflag = 0;                            // no signaling chars, no echo,
                                                        // no canonical processing
            tty.c_oflag = 0;                            // no remapping, no delays
            tty.c_cc[VMIN]  = 0;                        // read doesn't block
            tty.c_cc[VTIME] = 5;                        // 0.5 seconds read timeout

            tty.c_cflag |= (CLOCAL | CREAD);            // ignore modem controls, enable reading
            tty.c_cflag &= ~(PARENB | PARODD);          // shut off parity
            tty.c_cflag &= ~CSTOPB;                     // 1 stop bit
#ifdef CRTSCTS
            tty.c_cflag &= ~CRTSCTS;                    // no flow control
#elif defined(IHFLOW) && defined(OHFLOW)
            tty.c_cflag &= ~(IHFLOW | OHFLOW);
#endif

            if (tcsetattr(g_fd, TCSANOW, &tty) == 0) {
                g_simulated = false;
                snprintf(g_status, sizeof(g_status), "physical device %s configured at 115200 baud", g_dev_path);
                return 0;
            }
        }
        close(g_fd);
        g_fd = -1;
    }
#endif

    /* Fallback to Simulated UART */
    g_simulated = true;
    snprintf(g_status, sizeof(g_status), "simulated loopback mode (dev %s offline)", g_dev_path);
    return 0;
}

void uart_adapter_shutdown(void) {
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
    g_simulated = true;
    strcpy(g_status, "shutdown");
}

int uart_adapter_send(const uint8_t *data, size_t len) {
    if (!data || len == 0) return 0;

    if (!g_simulated && g_fd >= 0) {
        ssize_t written = write(g_fd, data, len);
        if (written < 0) {
            return -1;
        }
        return (int)written;
    } else {
        /* Push into loopback buffer */
        size_t pushed = 0;
        for (size_t i = 0; i < len; i++) {
            size_t next = (g_sim_head + 1) % SIM_QUEUE_SZ;
            if (next != g_sim_tail) {
                g_sim_queue[g_sim_head] = data[i];
                g_sim_head = next;
                pushed++;
            } else {
                break; // queue full
            }
        }
        return (int)pushed;
    }
}

int uart_adapter_receive(uint8_t *buf, size_t max_len) {
    if (!buf || max_len == 0) return 0;

    if (!g_simulated && g_fd >= 0) {
        ssize_t read_bytes = read(g_fd, buf, max_len);
        if (read_bytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            return -1;
        }
        return (int)read_bytes;
    } else {
        /* Read from loopback buffer */
        size_t read_bytes = 0;
        while (g_sim_tail != g_sim_head && read_bytes < max_len) {
            buf[read_bytes++] = g_sim_queue[g_sim_tail];
            g_sim_tail = (g_sim_tail + 1) % SIM_QUEUE_SZ;
        }
        return (int)read_bytes;
    }
}

const char* uart_adapter_status(void) {
    return g_status;
}

bool uart_adapter_is_simulated(void) {
    return g_simulated;
}

int uart_adapter_trace_rx(uint32_t act_id, uint32_t corr_id, uint8_t byte) {
    return trace_collector_record_simple(TRACE_EXTERNAL_EVENT_RX, 0xFFFFu, act_id, corr_id, byte, 1 /* UART marker */);
}

int uart_adapter_trace_tx(uint32_t act_id, uint32_t corr_id, uint8_t byte) {
    return trace_collector_record_simple(TRACE_EXTERNAL_EVENT_RX, 0xFFFFu, act_id, corr_id, byte, 2 /* UART TX marker */);
}
