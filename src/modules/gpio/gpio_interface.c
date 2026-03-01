#include "gpio_interface.h"
#include "debug_log.h"
#include <hardware/gpio.h>
#include <pico/time.h>

static gpio_trigger_callback_t enable_or_delete_trigger_function = NULL;
static gpio_trigger_callback_t i2s_dma_trigger_function = NULL;

void fc_irq(uint gpio, uint32_t events) {
    if (gpio == PIN_FC_RECORD_PIN && (events & GPIO_IRQ_EDGE_RISE)) {
        if (i2s_dma_trigger_function != NULL) {
            i2s_dma_trigger_function();
        } else {
            LOGI("Callback function pointer [i2s_dma_trigger_function] not initialized.");
        }
    }
    if (gpio == PIN_FC_ENABLE_PIN && (events & GPIO_IRQ_EDGE_RISE)) {
        if (enable_or_delete_trigger_function != NULL) {
            enable_or_delete_trigger_function();
        } else {
            LOGI("Callback function pointer [enable_or_delete_trigger_function] not initialized.");
        }
    }
}

void initialize_gpio_interface(gpio_trigger_callback_t enable_callback, gpio_trigger_callback_t record_callback) {
    enable_or_delete_trigger_function = enable_callback;
    i2s_dma_trigger_function = record_callback;
    gpio_set_irq_enabled_with_callback(PIN_FC_ENABLE_PIN, GPIO_IRQ_EDGE_RISE, true, &fc_irq);
    gpio_set_irq_enabled_with_callback(PIN_FC_RECORD_PIN, GPIO_IRQ_EDGE_RISE, true, &fc_irq);

#ifdef USE_PICO_ONBOARD_LED
    // Initialise les sorties simulateur FC (GP8 → GP2, GP9 → GP3)
    gpio_init(PIN_FC_SIM_ENABLE);
    gpio_set_dir(PIN_FC_SIM_ENABLE, GPIO_OUT);
    gpio_put(PIN_FC_SIM_ENABLE, 0);

    gpio_init(PIN_FC_SIM_RECORD);
    gpio_set_dir(PIN_FC_SIM_RECORD, GPIO_OUT);
    gpio_put(PIN_FC_SIM_RECORD, 0);
#endif
}

#ifdef USE_PICO_ONBOARD_LED
void gpio_sim_set_enable(bool active) {
    gpio_put(PIN_FC_SIM_ENABLE, active ? 1 : 0);
}

void gpio_sim_set_record(bool active) {
    gpio_put(PIN_FC_SIM_RECORD, active ? 1 : 0);
}
#endif
