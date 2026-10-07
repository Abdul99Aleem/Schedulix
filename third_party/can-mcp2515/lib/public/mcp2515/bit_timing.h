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

#ifndef LIB_PUBLIC_MCP2515_BIT_TIMING_H_
#define LIB_PUBLIC_MCP2515_BIT_TIMING_H_

#include <stdint.h>

/**
 * @file
 *
 * Declerations related to calculating the bit timing values to reach
 * a desired bitrate.
 */

/**
 * Bit timing constants/constraints used when generating bit timing values for
 * a given bitrate.
 *
 * These are all in terms of number of Tq periods.
 *
 * These usually depend on the specific hardware.
 */
struct can_bit_timing_consts {
  uint32_t tseg1Min;    ///< The minimum valid number of Tq in tseg1. 
  uint32_t tseg1Max;    ///< The maximum valid number of Tq in tseg2.
  uint32_t tseg2Min;    ///< The minimum valid number of Tq in tseg2.
  uint32_t tseg2Max;    ///< The maximum valid number of Tq in tseg2.
  uint32_t sjwMax;      ///< The maximum number of Tq allowed for SJW.
  uint32_t brpMin;      ///< The minimum valid BRP.
  uint32_t brpMax;      ///< The maximum valid BRP.
  uint32_t brpInc;      ///< The amount to increment the BRP each step.
};
typedef struct can_bit_timing_consts can_bit_timing_consts_t;

/**
 * Bit timing parameters.
 *
 * These are all in terms of number of Tq periods.
 *
 * Calculated from the provided @ref can_bit_timinig_consts and the desired bitrate.
 */
struct can_bit_timings {
  uint32_t prop;        ///< The number of Tq in the propegation segment.
  uint32_t phase1;      ///< The number of Tq in phase1.
  uint32_t phase2;      ///< The number of Tq in phase2.
  uint32_t sjw;         ///< The number of Tq for SJW.
  uint32_t brp;         ///< The BRP to use.
};
typedef struct can_bit_timings can_bit_timings_t;

/**
 * Calculate the best bit timings to reach the desired bitrate.
 *
 * Algorithm is derived from what Linux does.
 *
 * @param[in] bitrate The bitrate in bits-per-second
 * @param[in] clk The bit rate clock in Hz
 * @param[in] btConsts The bt constants to use during the calculation
 * @param[out] btOut The resulting bit timings.
 *
 * @returns EOK if successful, an error otherwise.
 */
int can_calculate_bit_timings(uint32_t bitrate, uint32_t clk, const can_bit_timing_consts_t* btConsts,
                              can_bit_timings_t* btOut);

#endif  // LIB_PUBLIC_MCP2515_BIT_TIMING_H_
