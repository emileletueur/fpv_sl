#include <stdbool.h>
#include <stdint.h>

#ifdef FPV_SL_PICO_PROBE_DEBUG
// GP1/GP2 sont réservés au SWD en mode debug probe → décalage sur GP2/GP3
#ifndef PIN_FC_ENABLE_PIN
#define PIN_FC_ENABLE_PIN 2
#endif
#ifndef PIN_FC_RECORD_PIN
#define PIN_FC_RECORD_PIN 3
#endif
// Sorties simulateur FC : fronts montants/descendants envoyés vers GP2/GP3
#define PIN_FC_SIM_ENABLE 8
#define PIN_FC_SIM_RECORD 9
#else
#ifndef PIN_FC_ENABLE_PIN
#define PIN_FC_ENABLE_PIN 1
#endif
#ifndef PIN_FC_RECORD_PIN
#define PIN_FC_RECORD_PIN 2
#endif
#endif

typedef int8_t (*gpio_trigger_callback_t)(void);
void initialize_gpio_interface(gpio_trigger_callback_t enable_callback, gpio_trigger_callback_t record_callback);

#ifdef FPV_SL_PICO_PROBE_DEBUG
// Positionne l'état de GP8 (simulateur ENABLE → GP2).
// Passer false puis true pour générer un front montant détecté par l'IRQ.
void gpio_sim_set_enable(bool active);
// Positionne l'état de GP9 (simulateur RECORD → GP3).
void gpio_sim_set_record(bool active);
#endif
