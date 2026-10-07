/*
 * Copyright (c) 2025, BlackBerry Limited. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/mman.h>

#include <mcp2515/logging.h>

#include "drv_state.h"
#include "rpi4.h"

// These #defines are copied from the BSP's bcm2711.h
#define BCM2711_GPIO_BASE                   0xfe200000
#define BCM2711_GPIO_SIZE                   0x100
#define BCM2711_GPIO_FSEL0                  (0x00)
#define BCM2711_GPIO_FSEL1                  (0x04)
#define BCM2711_GPIO_FSEL2                  (0x08)
#define BCM2711_GPIO_FSEL3                  (0x0c)
#define BCM2711_GPIO_FSEL4                  (0x10)
#define BCM2711_GPIO_FSEL5                  (0x14)
#define BCM2711_GPIO_SET0                   (0x1c)
#define BCM2711_GPIO_SET1                   (0x20)
#define BCM2711_GPIO_CLR0                   (0x28)
#define BCM2711_GPIO_CLR1                   (0x2c)
#define BCM2711_GPIO_LEV0                   (0x34)
#define BCM2711_GPIO_LEV1                   (0x38)
#define BCM2711_GPIO_EDS0                   (0x40)
#define BCM2711_GPIO_EDS1                   (0x44)
#define BCM2711_GPIO_REN0                   (0x4c)
#define BCM2711_GPIO_REN1                   (0x50)
#define BCM2711_GPIO_FEN0                   (0x58)
#define BCM2711_GPIO_FEN1                   (0x5c)
#define BCM2711_GPIO_HEN0                   (0x64)
#define BCM2711_GPIO_HEN1                   (0x68)
#define BCM2711_GPIO_LEN0                   (0x70)
#define BCM2711_GPIO_LEN1                   (0x74)
#define BCM2711_GPIO_AREN0                  (0x7c)
#define BCM2711_GPIO_AREN1                  (0x80)
#define BCM2711_GPIO_AFEN0                  (0x88)
#define BCM2711_GPIO_AFEN1                  (0x8c)
#define BCM2711_GPIO_PULL0                  (0xe4)
#define BCM2711_GPIO_PULL1                  (0xe8)
#define BCM2711_GPIO_PULL2                  (0xec)
#define BCM2711_GPIO_PULL3                  (0xf0)
#define BCM2711_GPIO_IRQ0                   (96+49)
#define BCM2711_GPIO_IRQ1                   (96+50)
#define BCM2711_GPIO_IRQ2                   (96+51)
#define BCM2711_GPIO_IRQ3                   (96+52)

// These #define's come from BSP's gpio-bcm2711 code
#define BCM2711_GPIO_MIN        0
#define BCM2711_GPIO_MAX        53

struct platform_rpi4_data {
    volatile uint32_t *regs;
};
typedef struct platform_rpi4_data platform_rpi4_data_t;

int platform_rpi4_initialize(drv_state_t* state) {
    platform_rpi4_data_t* data = NULL;
    char errStr[ERR_STR_SIZE];
    int rc;
    int shift;
    int reg;

    // Before I do anything, make sure the gpio I want is valid for this platform.
    if (state->gpio < BCM2711_GPIO_MIN || state->gpio > BCM2711_GPIO_MAX) {
        LOGE("GPIO %d is out of range for RPI4. Must be between %d and %d.",
             state->gpio, BCM2711_GPIO_MIN, BCM2711_GPIO_MAX);
        return EINVAL;
    }

    data = calloc(1, sizeof(*data));
    if (!data) {
        LOGE("Unable to allocate platform data.\n");
        return ENOMEM;
    }
    data->regs = mmap(0, __PAGESIZE, PROT_NOCACHE|PROT_READ|PROT_WRITE, MAP_PHYS | MAP_SHARED, NOFD, BCM2711_GPIO_BASE);
    if (data->regs == MAP_FAILED) {
        rc = errno;
        LOGE("Unable to map RPI4's GPIO registers. Error=%s", sstrerror(rc, errStr, sizeof(errStr)));
        goto error;
    }

    // Program gpio to be an pulled-up input with a low level trigger'ed interrupt
    // Function (want input GPIO = 0b000)
    reg = (BCM2711_GPIO_FSEL0 + (state->gpio / 10)) / 4;
    shift = (state->gpio % 10) * 3;
    data->regs[reg] = (data->regs[reg] & ~(0x7 << shift));
    // Pull-up (0b01)
    reg = (BCM2711_GPIO_PULL0 + (state->gpio / 16)) / 4;
    shift = (state->gpio % 16) * 2;
    data->regs[reg] = (data->regs[reg] & ~(0x3 << shift)) | (0x01 << shift);
    // low-level trigger'ed interrupt. Disables the other types of interrupts
    // for this GPIO.
    if (state->gpio < 32) {
        shift = state->gpio;
        data->regs[BCM2711_GPIO_REN0/4] &= ~(1 << shift);
        data->regs[BCM2711_GPIO_FEN0/4] &= ~(1 << shift);
        data->regs[BCM2711_GPIO_HEN0/4] &= ~(1 << shift);
        data->regs[BCM2711_GPIO_LEN0/4] |= 1 << shift;
    } else {
        shift = state->gpio - 32;
        data->regs[BCM2711_GPIO_REN1/4] &= ~(1 << shift);
        data->regs[BCM2711_GPIO_FEN1/4] &= ~(1 << shift);
        data->regs[BCM2711_GPIO_HEN1/4] &= ~(1 << shift);
        data->regs[BCM2711_GPIO_LEN1/4] |= 1 << shift;
    }
    state->platform = data;

    // Clear an interrupt event if there was one.
    // If it was legit, it will be raised again as the level from the MCP2515
    // will still be low.
    platform_rpi4_gpio_irq_clear(state);

    return EOK;

error:
    free(data);
    return rc;
}

void platform_rpi4_deinitialize(drv_state_t* state) {
    if (state) {
        free(state->platform);
        state->platform = NULL;
    }
}

int platform_rpi4_gpio_irq_get(drv_state_t* state) {
    // Get the IRQ associated with the gpio
    assert(state != NULL);
    assert(state->gpio >= BCM2711_GPIO_MIN && state->gpio <= BCM2711_GPIO_MAX);
    
    // On the RPI4, GPIO interrupt's are routed via four lines, which are then
    // exposed via a specific interrupt in the GIC-400 from the video-core block.
    // The BCM2711_GPIO_IRQ* #defines are the actual GIC-400 interrupt number
    // (as seen by the processor).
    // GPIO_IRQ0 is for bank0 GPIOs (0-27)
    // GPIO_IRQ1 is for bank1 GPIOs (28-45)
    // GPIO_IRQ2 is for bank1 GPIOs (46-57)
    // GPIO_IRQ3 is for ALL GPIOs
    if (state->gpio >= 0 && state->gpio <= 27) {
        return BCM2711_GPIO_IRQ0;
    } else if (state->gpio >= 28 && state->gpio <= 45) {
        return BCM2711_GPIO_IRQ1;
    } else if (state->gpio >= 46 && state->gpio <= 57) {
        return BCM2711_GPIO_IRQ2;
    } else {
        return -1;
    }
}

// Check if the GPIO's interrupt is 'active'
int platform_rpi4_gpio_irq_active(drv_state_t* state) {
    platform_rpi4_data_t* data = state->platform;
    int bit;

    assert(state != NULL);
    assert(state->gpio >= BCM2711_GPIO_MIN && state->gpio <= BCM2711_GPIO_MAX);

    if (state->gpio < 32) {
        bit = data->regs[BCM2711_GPIO_EDS0 / 4] & (1 << state->gpio);
    } else {
        bit = data->regs[BCM2711_GPIO_EDS1 / 4] & (1 << (state->gpio - 32));
    }

    return bit != 0;
}

// Clear the interrupt bit
void platform_rpi4_gpio_irq_clear(drv_state_t* state) {
    platform_rpi4_data_t* data = state->platform;

    assert(state != NULL);
    assert(state->gpio >= BCM2711_GPIO_MIN && state->gpio <= BCM2711_GPIO_MAX);

    if (state->gpio < 32) {
        data->regs[BCM2711_GPIO_EDS0 / 4] = (1 << state->gpio);
    } else {
        data->regs[BCM2711_GPIO_EDS1 / 4] = (1 << (state->gpio - 32));
    }
}
