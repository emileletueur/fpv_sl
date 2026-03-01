#include "test_framework.h"
#include "gpio/gpio_interface.h"
#include "pico/time.h"
#include <stdbool.h>

/* ── Flags positionnés par les callbacks ───────────────────────── */

static volatile bool enable_cb_called = false;
static volatile bool record_cb_called = false;

static int8_t enable_cb(void) { enable_cb_called = true;  return 0; }
static int8_t record_cb(void) { record_cb_called = true;  return 0; }

/* ── Helpers ────────────────────────────────────────────────────── */

static void reset_flags(void) {
    enable_cb_called = false;
    record_cb_called = false;
}

/* Génère un front montant propre sur le simulateur ENABLE :
   assure LOW d'abord, puis passe HIGH. Laisse le temps à l'IRQ de s'exécuter. */
static void rising_edge_enable(void) {
    gpio_sim_set_enable(false);
    sleep_us(50);
    gpio_sim_set_enable(true);
    sleep_us(200);
}

static void rising_edge_record(void) {
    gpio_sim_set_record(false);
    sleep_us(50);
    gpio_sim_set_record(true);
    sleep_us(200);
}

static void pins_low(void) {
    gpio_sim_set_enable(false);
    gpio_sim_set_record(false);
}

/* ── Tests ──────────────────────────────────────────────────────── */

/* Front montant sur GP8 → callback ENABLE appelé, pas RECORD */
void test_enable_callback_called(void) {
    reset_flags();
    rising_edge_enable();
    TEST_EXPECT_TRUE(enable_cb_called);
    TEST_EXPECT_FALSE(record_cb_called);
    pins_low();
}

/* Front montant sur GP9 → callback RECORD appelé, pas ENABLE */
void test_record_callback_called(void) {
    reset_flags();
    rising_edge_record();
    TEST_EXPECT_TRUE(record_cb_called);
    TEST_EXPECT_FALSE(enable_cb_called);
    pins_low();
}

/* Maintenir HIGH (pas de nouveau front) → pas de second déclenchement */
void test_no_retrigger_when_already_high(void) {
    reset_flags();
    gpio_sim_set_enable(true);
    sleep_us(200);
    enable_cb_called = false;   /* reset après le premier déclenchement */
    gpio_sim_set_enable(true);  /* même niveau → pas de front montant */
    sleep_us(200);
    TEST_EXPECT_FALSE(enable_cb_called);
    pins_low();
}

/* Retour à LOW puis HIGH → nouveau front → nouveau déclenchement */
void test_second_rising_edge_triggers_again(void) {
    reset_flags();
    rising_edge_enable();
    TEST_EXPECT_TRUE(enable_cb_called);
    enable_cb_called = false;
    /* Deuxième front : LOW → HIGH */
    rising_edge_enable();
    TEST_EXPECT_TRUE(enable_cb_called);
    pins_low();
}

/* Séquence réaliste : ENABLE puis RECORD */
void test_sequence_enable_then_record(void) {
    reset_flags();
    rising_edge_enable();
    TEST_EXPECT_TRUE(enable_cb_called);
    TEST_EXPECT_FALSE(record_cb_called);
    rising_edge_record();
    TEST_EXPECT_TRUE(record_cb_called);
    pins_low();
}

/* Callbacks NULL → IRQ déclenché mais pas de crash (log attendu) */
void test_null_callbacks_no_crash(void) {
    initialize_gpio_interface(NULL, NULL);
    reset_flags();
    rising_edge_enable();
    rising_edge_record();
    /* Aucun callback appelé, aucun crash : test passé si on arrive ici */
    pins_low();
    /* Restore pour les tests éventuellement lancés après */
    initialize_gpio_interface(enable_cb, record_cb);
}

/* ── Suite entry point ─────────────────────────────────────────── */

void run_gpio_tests(void) {
    initialize_gpio_interface(enable_cb, record_cb);
    LOGI("── GPIO interface tests ──────────────────────");
    RUN_TEST(test_enable_callback_called);
    RUN_TEST(test_record_callback_called);
    RUN_TEST(test_no_retrigger_when_already_high);
    RUN_TEST(test_second_rising_edge_triggers_again);
    RUN_TEST(test_sequence_enable_then_record);
    RUN_TEST(test_null_callbacks_no_crash);
}
