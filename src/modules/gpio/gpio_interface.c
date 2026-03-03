#include "gpio_interface.h"
#include "debug_log.h"
#include <hardware/gpio.h>
#include <pico/time.h>

static gpio_trigger_callback_t s_enable_cb  = NULL;
static gpio_trigger_callback_t s_disable_cb = NULL;
static gpio_trigger_callback_t s_record_cb  = NULL;
static gpio_trigger_callback_t s_disarm_cb  = NULL;

void fc_irq(uint gpio, uint32_t events) {
    if (gpio == PIN_FC_ENABLE_PIN) {
        if ((events & GPIO_IRQ_EDGE_RISE) && s_enable_cb)
            s_enable_cb();
        if ((events & GPIO_IRQ_EDGE_FALL) && s_disable_cb)
            s_disable_cb();
    }
    if (gpio == PIN_FC_RECORD_PIN) {
        if ((events & GPIO_IRQ_EDGE_RISE) && s_record_cb)
            s_record_cb();
        if ((events & GPIO_IRQ_EDGE_FALL) && s_disarm_cb)
            s_disarm_cb();
    }
}

void initialize_gpio_interface(gpio_trigger_callback_t enable_callback,
                               gpio_trigger_callback_t disable_callback,
                               gpio_trigger_callback_t record_callback,
                               gpio_trigger_callback_t disarm_callback) {
    s_enable_cb  = enable_callback;
    s_disable_cb = disable_callback;
    s_record_cb  = record_callback;
    s_disarm_cb  = disarm_callback;
    gpio_set_irq_enabled_with_callback(PIN_FC_ENABLE_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &fc_irq);
    gpio_set_irq_enabled_with_callback(PIN_FC_RECORD_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &fc_irq);

#ifdef FPV_SL_PICO_PROBE_DEBUG
    // Initialise les sorties simulateur FC (GP8 → GP2, GP9 → GP3)
    gpio_init(PIN_FC_SIM_ENABLE);
    gpio_set_dir(PIN_FC_SIM_ENABLE, GPIO_OUT);
    gpio_put(PIN_FC_SIM_ENABLE, 0);

    gpio_init(PIN_FC_SIM_RECORD);
    gpio_set_dir(PIN_FC_SIM_RECORD, GPIO_OUT);
    gpio_put(PIN_FC_SIM_RECORD, 0);
#endif
}

#ifdef FPV_SL_PICO_PROBE_DEBUG
void gpio_sim_set_enable(bool active) {
    gpio_put(PIN_FC_SIM_ENABLE, active ? 1 : 0);
}

void gpio_sim_set_record(bool active) {
    gpio_put(PIN_FC_SIM_RECORD, active ? 1 : 0);
}
#endif
