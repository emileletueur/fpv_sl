#include "unity.h"
#include "fpv_sl_core.h"
#include <stdint.h>

/* process_sample est implémentée dans fpv_sl_core.c mais n'est pas exposée
   dans fpv_sl_core.h (le header public déclare apply_filter_and_gain).
   On déclare ici pour les tests uniquement. */
int32_t process_sample(hp_filter_t *f, int32_t sample);

void setUp(void)    {}
void tearDown(void) {}

/* Valeur d'entrée brute (24 bits dans mot 32 bits, comme l'INMP441).
   Après le décalage >> 8 dans process_sample : 0x01000000 >> 8 = 65536. */
#define RAW_SAMPLE_POS  ((int32_t)0x01000000)
#define RAW_SAMPLE_NEG  ((int32_t)(-0x01000000))

static hp_filter_t fresh_filter(void) {
    hp_filter_t f = {0.959f, 0.0f, 0.0f};
    return f;
}

static int32_t iabs32(int32_t x) { return x < 0 ? -x : x; }

/* ── Entrée nulle ────────────────────────────────────────────────────────── */

void test_zero_input_gives_zero_output(void) {
    hp_filter_t f = fresh_filter();
    TEST_ASSERT_EQUAL_INT32(0, process_sample(&f, 0));
}

/* ── Rejet DC ────────────────────────────────────────────────────────────── */

void test_dc_rejection(void) {
    /* Un signal constant est une composante DC.
       Le filtre passe-haut doit l'atténuer vers 0 : y[n] = alpha * y[n-1] (alpha < 1).
       Après 200 échantillons, la sortie doit être négligeable. */
    hp_filter_t f = fresh_filter();
    int32_t output = 0;
    for (int i = 0; i < 200; i++) {
        output = process_sample(&f, RAW_SAMPLE_POS);
    }
    /* Tolérance : moins de 1 % de la valeur d'entrée après >>8 (65536) */
    TEST_ASSERT_INT32_WITHIN(656, 0, output);
}

/* ── Premier échantillon : gain et signe ─────────────────────────────────── */

void test_first_sample_is_positive_for_positive_input(void) {
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, RAW_SAMPLE_POS);
    TEST_ASSERT_GREATER_THAN_INT32(0, output);
}

void test_first_sample_is_negative_for_negative_input(void) {
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, RAW_SAMPLE_NEG);
    TEST_ASSERT_LESS_THAN_INT32(0, output);
}

void test_first_sample_gain_less_than_input_after_shift(void) {
    /* input >> 8 = 65536 ; sortie ≈ 65536 * 0.959 * 0.8 ≈ 50 303 < 65536 */
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, RAW_SAMPLE_POS);
    TEST_ASSERT_LESS_THAN_INT32(65536, output);
}

void test_first_sample_gain_matches_expected(void) {
    /* Valeur attendue : floor(65536 * 0.959 * 0.8) = 50303 (±500 pour arrondi flottant) */
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, RAW_SAMPLE_POS);
    TEST_ASSERT_INT32_WITHIN(500, 50303, output);
}

/* ── Symétrie signe ──────────────────────────────────────────────────────── */

void test_positive_and_negative_inputs_are_symmetric(void) {
    hp_filter_t f_pos = fresh_filter();
    hp_filter_t f_neg = fresh_filter();
    int32_t out_pos = process_sample(&f_pos, RAW_SAMPLE_POS);
    int32_t out_neg = process_sample(&f_neg, RAW_SAMPLE_NEG);
    /* Les deux filtres partent du même état → résultats opposés */
    TEST_ASSERT_EQUAL_INT32(out_pos, -out_neg);
}

/* ── Signal alterné (fréquence de Nyquist) ───────────────────────────────── */

void test_alternating_signal_passes_through(void) {
    /* Un signal ±RAW_SAMPLE est à la fréquence de Nyquist.
       Le filtre passe-haut le laisse passer.
       On vérifie que le pic de sortie est significatif après convergence. */
    hp_filter_t f = fresh_filter();
    int32_t sign = 1;
    int32_t peak = 0;
    for (int i = 0; i < 100; i++) {
        int32_t out = process_sample(&f, sign * RAW_SAMPLE_POS);
        int32_t a = iabs32(out);
        if (a > peak) peak = a;
        sign = -sign;
    }
    /* Le pic doit être au moins 50 % de la sortie du premier échantillon (~50 303) */
    TEST_ASSERT_GREATER_THAN_INT32(25000, peak);
}

/* ── Alpha = 0 : tout bloqué ─────────────────────────────────────────────── */

void test_alpha_zero_blocks_all(void) {
    hp_filter_t f = {0.0f, 0.0f, 0.0f};
    int32_t output = process_sample(&f, RAW_SAMPLE_POS);
    TEST_ASSERT_EQUAL_INT32(0, output);
}

/* ── État interne mis à jour correctement ────────────────────────────────── */

void test_state_last_x_is_updated(void) {
    hp_filter_t f = fresh_filter();
    /* input >> 8 = 65536 */
    process_sample(&f, RAW_SAMPLE_POS);
    /* last_x doit valoir 65536.0f */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 65536.0f, f.last_x);
}

void test_state_reset_gives_clean_output(void) {
    hp_filter_t f = fresh_filter();
    /* Charger le filtre avec un signal DC */
    for (int i = 0; i < 50; i++) {
        process_sample(&f, RAW_SAMPLE_POS);
    }
    /* Reset manuel de l'état */
    f.last_x = 0.0f;
    f.last_y = 0.0f;
    /* Sur entrée nulle après reset : sortie nulle */
    int32_t output = process_sample(&f, 0);
    TEST_ASSERT_EQUAL_INT32(0, output);
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_zero_input_gives_zero_output);
    RUN_TEST(test_dc_rejection);
    RUN_TEST(test_first_sample_is_positive_for_positive_input);
    RUN_TEST(test_first_sample_is_negative_for_negative_input);
    RUN_TEST(test_first_sample_gain_less_than_input_after_shift);
    RUN_TEST(test_first_sample_gain_matches_expected);
    RUN_TEST(test_positive_and_negative_inputs_are_symmetric);
    RUN_TEST(test_alternating_signal_passes_through);
    RUN_TEST(test_alpha_zero_blocks_all);
    RUN_TEST(test_state_last_x_is_updated);
    RUN_TEST(test_state_reset_gives_clean_output);

    return UNITY_END();
}
