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
#ifndef CAN_MCP2515_DRIVER_RPI4_H_
#define CAN_MCP2515_DRIVER_RPI4_H_

/**
 * @file
 *
 * Abstract the RPI4 platform support.
 *
 * In order to use HW features of the RPI4, mainly the GPIOs and associated
 * IRQs, direct communication with the HW is required.
 *
 * At the moment, only RPI4 is supported but when that changes, this API
 * should be replicated into different hardware-specific implementations then
 * accessed through a struct of function pointers.
 */

#include <stdint.h>
#include "drv_state.h"

/**
 * Initialize the RPI4 platform support.
 *
 * @param[in]   state   The state context for the driver
 *
 * @return EOK(0) if successful, one of the other errno codes on failure.
 */
int platform_rpi4_initialize(drv_state_t* state);

/**
 * Deinitialize the RPI4 platform support.
 *
 * Calling this API when the platform support is NOT initialized is ok
 * and results in a no-op.
 *
 * @param[in]   state   The state context for the driver
 */
void platform_rpi4_deinitialize(drv_state_t* state);

/**
 * Get the IRQ number associated with a GPIO.
 *
 * The GPIO used is specified by the drv_state_t.gpio memeber.
 *
 * @param[in]   state   The state context for the driver
 *
 * @return The positive number for the IRQ associated with the GPIO
 *         drv_state_t.gpio member. If there is an error -1 will
 *         be returned.
 */
int platform_rpi4_gpio_irq_get(drv_state_t* state);

/**
 * Checks if the IRQ associated with a GPIO is active.
 *
 * The GPIO used is specified by the drv_state_t.gpio memeber.
 *
 * @param[in]   state   The state context for the driver
 *
 * @return True (non-zero) if the IRQ is active (it has fired).
 *         False (zero) if the IRQ is inactive (it has NOT fired).
 */
int platform_rpi4_gpio_irq_active(drv_state_t* state);

/**
 * Clears the IRQ associated with a GPIO, making it inactive.
 *
 * The GPIO used is specified by the drv_state_t.gpio memeber.
 *
 * @note If the conditions that caused the IRQ to go active have not yet been
 *       cleared, then the IRQ may immediately go active again.
 *
 * @param[in]   state   The state context for the driver
 */
void platform_rpi4_gpio_irq_clear(drv_state_t* state);

#endif  // CAN_MCP2515_DRIVER_RPI4_H_
