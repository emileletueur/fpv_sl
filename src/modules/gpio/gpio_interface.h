#include <stdint.h>
#ifndef PIN_FC_ENABLE_PIN
#define PIN_FC_ENABLE_PIN 1
#endif

#ifndef PIN_FC_RECORD_PIN
#define PIN_FC_RECORD_PIN 2
#endif

typedef int8_t (*gpio_trigger_callback_t)(void);
void initialize_gpio_interface(gpio_trigger_callback_t enable_callback, gpio_trigger_callback_t record_callback);
