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

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include <mcp2515/bit_timing.h>
#include <mcp2515/logging.h>
#include <mcp2515/util.h>

#define SYNC_SEG_NUM_TQ 1

#define FP_MULTIPLIER 1000
#define TO_FP(x) ((x)*FP_MULTIPLIER)
#define FROM_FP(x) ((x) / FP_MULTIPLIER)

struct sample_point_result {
    uint32_t phase1;
    uint32_t phase2;
    uint32_t error;
};
typedef struct sample_point_result sample_point_result_t;

// Returns UINT32_MAX in result->error if a sample point could not be calculated
static void calculate_sample_point(const can_bit_timing_consts_t* btConsts,
                                   uint32_t totalTq, uint32_t spPct, sample_point_result_t* result)
{
    uint32_t phase1;
    uint32_t phase2;
    uint32_t spError;
    uint32_t bestSpError = UINT32_MAX;
    uint32_t actualSpPct;

    result->error = UINT32_MAX;

    // Try to figure out where the sample point would be within totalTq.
    // This essentially figures out tseg1 and tseg2.
    // Note that we will NOT accept a calculated sample point
    // that is greater than the desired sample point
    //
    // Run the calculation twice, via adj, to account for rounding up and rounding down
    for (int adj = 0; adj <= 1; ++adj) {
        // The desired sample point (in ns) is:
        //   bt * spPct
        // Converted to a time quanta index that would be
        //   (bt * spPct) / tq
        // Where
        //   tq == bt / totalTq
        // Therefore
        //  (bt * spPct) / (bt/totalTq)
        //  == (bt * spPct * totalTq) / bt
        //  == spPct * totalTq
        //
        // spPct is fixed point value with multiplier of 1000
        // so we have to divide result by 1000 to get actual.
        //
        // Finally we want the number of quanta in phase2
        // so the result above is subtracted from totalTq
        phase2 = totalTq - FROM_FP(totalTq * spPct);
        LOGD2("  Calculating samplePoint. phase2=%lu (adjusted %lu)", phase2, phase2 - adj);
        phase2 -= adj;
        phase2 = CLAMP(phase2, btConsts->tseg2Min, btConsts->tseg2Max);
        phase1 = totalTq - phase2;
        phase1 = CLAMP(phase1, btConsts->tseg1Min, btConsts->tseg1Max);
        if (phase1 + phase2 != totalTq) {
            // Phase1 got clamped, reset phase2
            phase2 = totalTq - phase1;
        }

        // Calculate the actualSpPct to make sure it doesn't exceed the desired.
        // Otherwise the sampling will occur too late.
        // The calculated value needs to be in the same fixed point as spPct
        // (so they can be compared)
        actualSpPct = TO_FP(phase1) / totalTq;
        if (actualSpPct > spPct) {
            LOGD2("    RESULT Calculated phase1/phase2 (%lu/%lu) sample point of %lu exceeds desired of %lu",
                  phase1, phase2, actualSpPct, spPct);
            continue;
        }

        // Calculate the error (in ns) between desired sample point and actual
        spError = ABS_DIFF(actualSpPct, spPct);
        if (spError >= bestSpError) {
            LOGD2("    RESULT spError %lu is worse than best %lu", spError, bestSpError);
            continue;
        }

        bestSpError = spError;
        // Minus one to remove the sync segment
        result->phase1 = phase1 - 1;
        result->phase2 = phase2;
        result->error = spError;
        LOGD2("    RESULT new best spError %lu", spError);
    }
}

/**
 * Calculate the best bit timings to reach the desired bitrate.
 *
 * Algorithm is derived from what Linux does.
 *
 * @param[in] bitrate The bitrate is bits-per-second
 * @param[in] btConsts The bt constants to use during the calculation
 * @param[out] btOut The resulting bit timings.
 *
 * @returns EOK if successful, an error otherwise.
 */
int can_calculate_bit_timings(uint32_t bitrate, uint32_t clk,
                              const can_bit_timing_consts_t* btConsts, can_bit_timings_t* btOut)
{
    uint64_t btDesired;

    uint64_t bt;
    uint32_t btError;
    uint32_t brp;
    uint32_t totalTq;

    uint32_t bestBtError = UINT32_MAX;
    uint32_t bestBrp = 0;

    uint32_t spPct;
    sample_point_result_t spBest = {.error = UINT32_MAX};
    sample_point_result_t spResult = {0};

    // Algorithm will enter an infinite loop if tseg1Min+tseg2Min == 0
    if (btConsts == NULL || btOut == NULL) {
        LOGE("One btConsts or btOut is NULL");
        return EINVAL;
    } else if (btConsts->brpInc == 0 || btConsts->brpMax == 0) {
        LOGE("One or more of brpInc/brpMax values is 0");
        return EINVAL;
    } else if (btConsts->sjwMax == 0) {
        LOGE("sjwMax is 0");
        return EINVAL;
    } else if (btConsts->tseg1Max == 0 || btConsts->tseg1Min == 0) {
        LOGE("One or more of tseg1Min/Max values is 0");
        return EINVAL;
    } else if (btConsts->tseg2Max == 0 || btConsts->tseg2Min == 0) {
        LOGE("One or more of tseg2Min/Max values is 0");
        return EINVAL;
    } else if (bitrate == 0) {
        LOGE("bitrate == 0");
        return EINVAL;
    } else if (clk == 0) {
        LOGE("clk == 0");
        return EINVAL;
    }

    /**
     * A 'bit' transmission is divided into four segments.
     * SYNC + PROP + PHASE1 + PHASE2
     * each segment takes a certain number of time quantas (tq)
     * where the length of a time quanta depends on the clock
     * running in the hw and the BRP (bitrate pre scaler) applied
     * to that clock.
     *
     * If you know the desired bitrate, then you know the desired
     * time per bit. The work then becomes trying to find the best
     * values for bt.prop, bt.phase1, bt.phase2, bt.brp, and bt.sjw
     * to minimize the amount of error between the desired time per
     * bit and the actual time per bit (based on the settings).
     *
     * In addition, the sample point, when the HW actually latches the
     * bus to read a 'bit' needs to be managed. It happens between PHASE1
     * and PHASE2. CiA gives some recommendations on where that point
     * should happen and this further constrains how bt.phase1 and bt.phase2
     * are set.
     *
     * CiA recommends the sample point and bit timing for the "standard"
     * speeds. See table 1 in section 5.4 in CiA 310 4.2.0 (reproduced below)
     * Table 1: Recommended bit timing settings
     * Bit rate   | Nominal bit time | Valid range for location | Recommended location
     *            |                  | of sample point          | of sample point
     *~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~
     * 1 Mbit/s   | 1us              | 75% to 90%               | 87.5%
     * 800 kbit/s | 1.25us           | 75% to 90%               | 87.5%
     * 500 kbit/s | 2us              | 85% to 90%               | 87.5%
     * 250 kbit/s | 4us              | 85% to 90%               | 87.5%
     * 125 kbit/s | 8us              | 85% to 90%               | 87.5%
     * 50 kbit/s  | 20us             | 85% to 90%               | 87.5%
     * 20 kbit/s  | 50us             | 85% to 90%               | 87.5%
     * 10 kbit/s  | 100us            | 85% to 90%               | 87.5%
     *
     * In addition, CiA apparently recommends how many time quantas (tq)
     * there should be for each bit. This is in CiA 102 v3.1.0, but unfortunately
     * I don't have access to that. This website references it and gives the
     * following table taken from CiA Draft Standard 102 v2.0
     * https://gemac-fieldbus.com/en/suggested-bit-timing-and-bus-length/
     * Bit rate   | Number of tq per bit | Sample point tq | Sample point %
     * ~~~~~~~~~~~|~~~~~~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~~~|~~~~~~~~~~~~~~~
     * 1 Mbit/s   | 8                    | 6               | 75%
     * 800 kbit/s | 10                   | 8               | 80%
     * 500 kbit/s | 16                   | 14              | 87.5%
     * 250 kbit/s | 16                   | 14              | 87.5%
     * 125 kbit/s | 16                   | 14              | 87.5%
     * 50 kbit/s  | 16                   | 14              | 87.5%
     * 20 kbit/s  | 16                   | 14              | 87.5%
     * 10 kbit/s  | 16                   | 14              | 87.5%
     *
     * So there is a bit of a descrepency between these two tables. The second one
     * (from the draft standard) changes the sample point % while the one I pulled
     * from the current published standard has the same sample point for all bitrates.
     *
     * I found a copy of CiA Draft Standard 102 v2.0 and it doesn't have the changing
     * sample points. So who knows if the changing sample points are true or not.
     *
     * I'm not sure how to resolve this. The Linux driver uses the changing sample point,
     * so I guess I'll go with that for now.
     */

    if (bitrate > 800000) {           // NOLINT
        spPct = (uint32_t)TO_FP(0.75);  // 75% NOLINT
    } else if (bitrate > 500000) {    // NOLINT
        spPct = (uint32_t)TO_FP(0.8);   // 80% NOLINT
    } else {
        spPct = (uint32_t)TO_FP(0.875);  // 87.5% NOLINT
    }

    // Figure out the desired bit time (ns per bit) for the given bitrate
    btDesired = SEC_TO_NS(1) / bitrate;
    LOGD2("Bitrate of %ubps gives %lluns/bit", bitrate, btDesired);

    // For each unique tseg (prop + phase1 + phase2) combination, try to figure
    // out the best settings of brp, prop+phase1, and phase2.
    //
    // Multiply by 2 to run each calculation twice, once when rounding up
    // and once when rounding down. The final +1 is to make sure both
    // styles of rounding are done for the final tseg value.
    //
    // prop, phase1, phase2, and tseg are all counting number of quantas
    for (uint32_t tseg = (btConsts->tseg1Max + btConsts->tseg2Max) * 2 + 1;
         tseg >= (btConsts->tseg1Min + btConsts->tseg2Min) * 2; --tseg)
    {
        totalTq = SYNC_SEG_NUM_TQ + tseg / 2;
        LOGD2("Testing totalTq=%u tseg=%u (rounding=%s)", totalTq, tseg / 2, (tseg % 2) == 0 ? "even" : "odd");

        // Figure out what brp is required to get as close as possible to
        // the desired bit time. BRP is defined as tq(secs) * clk ==
        // tq(nsecs) * clk / SEC_TO_NS(1)
        //
        // tq is the total time for a bit divided by the totalTq.
        //
        // So brp = (btDesired / totalTq) * clk / SEC_TO_NS(1)
        //        = btDesired * clk / (totalTq * SEC_TO_NS(1))
        brp = btDesired * clk / (totalTq * SEC_TO_NS(1));
        // Round down if tseg is even, round up if tseg is odd.
        brp += tseg % 2;
        // The HW may only be able to set brp in specific increments.
        // Adjust brp to account for that.
        LOGD2("  brp=%u", brp);
        brp = (brp / btConsts->brpInc) * btConsts->brpInc;
        LOGD2("  adjusted brp=%u", brp);
        // Make sure brp is valid
        if (brp < btConsts->brpMin || brp > btConsts->brpMax) {
            LOGD2("  RESULT: brp is out of range");
            continue;
        }

        // Now that we have brp, calculate the actual bit time that will give
        // us so we can then calculate the error.
        bt = (brp * SEC_TO_NS(1) * totalTq) / clk;
        btError = (uint32_t)ABS_DIFF(bt, btDesired);
        if (btError > bestBtError) {
            LOGD2("  RESULT: btError %lu is worse than best %lu", btError, bestBtError);
            continue;
        } else if (btError < bestBtError) {
            // Reset the spBest.error so that new sample points are
            // calculated for this bitrate
            spBest.error = UINT32_MAX;
        }
        LOGD2("  Calculated bt=%lluns, error=%luns", bt, btError);

        // Now try to figure out where the sample point would be within tseg.
        calculate_sample_point(btConsts, totalTq, spPct, &spResult);
        if (spResult.error >= spBest.error) {
            LOGD2("  RESULT: spError %lu is worse or equal to best %lu", spResult.error, spBest.error);
            continue;
        }

        // This is our best match
        bestBrp = brp;
        bestBtError = btError;
        spBest = spResult;

        LOGD2("  RESULT: NEW best brp=%lu, btError=%lu, spError=%lu", brp, btError, spResult.error);

        if (btError == 0 && spResult.error == 0) {
            // No error, can end early
            break;
        }
    }

    if (bestBrp == 0) {
        LOGE("Unable to caluclate bit timings. No viable bit rate found");
        return EINVAL;
    }

    // Calculate the percentage error between the best bitrate
    // and the desired bitrate. If it exceeds 0.5% then fail.
    btError = (uint64_t)TO_FP(bestBtError) / btDesired;
    if (btError >= (uint32_t)TO_FP(0.5)) {  // NOLINT
        LOGE("Unable to calculate bit timings. Error %u.%u%% exceeds 0.5%%", btError / 1000, btError % 1000);
        return EINVAL;
    }

    btOut->brp = bestBrp;
    // Phase 1 calculated above actually included the propegation segment
    // I don't have a good way of splitting it so just give half to prop.
    btOut->prop = spBest.phase1 / 2;
    btOut->phase1 = spBest.phase1 - btOut->prop;
    btOut->phase2 = spBest.phase2;
    btOut->sjw = max(1, min(btOut->phase1, btOut->phase2 / 2));

    return EOK;
}
