

#include "fpv_sl_core.h"
#include <stdint.h>

static execution_condition_t execution_condition;

uint8_t compute_execution_condition(fpv_sl_conf_t *fpv_sl_conf) {
    if (fpv_sl_conf->conf_is_loaded) {
        if (fpv_sl_conf->always_rcd)
            execution_condition = ALWAY_RCD_TYPE;
        else if (!fpv_sl_conf->always_rcd && !fpv_sl_conf->use_enable_pin)
            execution_condition = RCD_ONLY_TYPE;
        else if (!fpv_sl_conf->always_rcd && fpv_sl_conf->use_enable_pin)
            execution_condition = CLASSIC_TYPE;
        return 0;
    } else
        return -1;
}

void fpv_sl_core_loop(void) {
    switch (execution_condition) {
    case CLASSIC_TYPE:
        // Setup new file name
        // waiting for ENABLE trigger to start I2S DMA
        // waiting for ARM trigger to write sound data
        // Record until DESARM
        // Finalize WAV
    case RCD_ONLY_TYPE:
        // Setup new file name and start I2S DMA
        // waiting for ARM trigger to write sound data
        // Record until DESARM
        // Finalize WAV
    case ALWAY_RCD_TYPE:
        // Setup new file name
        // start I2S DMA
        // Record until DESARM
        // Finalize WAV
        break;
    }
}
