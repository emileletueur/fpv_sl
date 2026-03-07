#include "unity.h"
#include "fpv_sl_core.h"
#include <stdint.h>

/* process_sample et compute_*_alpha sont déclarées dans fpv_sl_core.h. */

void setUp(void)    {}
void tearDown(void) {}

/* Valeur d'entrée brute (24 bits dans mot 32 bits, comme l'INMP441).
   Après le décalage >> 8 dans process_sample : 0x01000000 >> 8 = 65536. */
#define RAW_SAMPLE_POS  ((int32_t)0x01000000)
#define RAW_SAMPLE_NEG  ((int32_t)(-0x01000000))

#define TEST_SAMPLE_RATE  44100
#define TEST_HP_CUTOFF    300
#define TEST_LP_CUTOFF    8000

static hp_filter_t fresh_hp(void) {
    hp_filter_t f = {compute_hp_alpha(TEST_HP_CUTOFF, TEST_SAMPLE_RATE), 0.0f, 0.0f};
    return f;
}

static lp_filter_t fresh_lp(void) {
    lp_filter_t f = {compute_lp_alpha(TEST_LP_CUTOFF, TEST_SAMPLE_RATE), 0.0f};
    return f;
}

/* Compat. alias utilisé par les tests HP existants. */
static hp_filter_t fresh_filter(void) { return fresh_hp(); }

static int32_t iabs32(int32_t x) { return x < 0 ? -x : x; }

/* ── Entrée nulle ────────────────────────────────────────────────────────── */

void test_zero_input_gives_zero_output(void) {
    hp_filter_t f = fresh_filter();
    TEST_ASSERT_EQUAL_INT32(0, process_sample(&f, NULL,0));
}

/* ── Rejet DC ────────────────────────────────────────────────────────────── */

void test_dc_rejection(void) {
    /* Un signal constant est une composante DC.
       Le filtre passe-haut doit l'atténuer vers 0 : y[n] = alpha * y[n-1] (alpha < 1).
       Après 200 échantillons, la sortie doit être négligeable. */
    hp_filter_t f = fresh_filter();
    int32_t output = 0;
    for (int i = 0; i < 200; i++) {
        output = process_sample(&f, NULL,RAW_SAMPLE_POS);
    }
    /* Tolérance : moins de 1 % de la valeur d'entrée après >>8 (65536) */
    TEST_ASSERT_INT32_WITHIN(656, 0, output);
}

/* ── Premier échantillon : gain et signe ─────────────────────────────────── */

void test_first_sample_is_positive_for_positive_input(void) {
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, NULL,RAW_SAMPLE_POS);
    TEST_ASSERT_GREATER_THAN_INT32(0, output);
}

void test_first_sample_is_negative_for_negative_input(void) {
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, NULL,RAW_SAMPLE_NEG);
    TEST_ASSERT_LESS_THAN_INT32(0, output);
}

void test_first_sample_gain_less_than_input_after_shift(void) {
    /* input >> 8 = 65536 ; sortie ≈ 65536 * 0.959 * 0.8 ≈ 50 303 < 65536 */
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, NULL,RAW_SAMPLE_POS);
    TEST_ASSERT_LESS_THAN_INT32(65536, output);
}

void test_first_sample_gain_matches_expected(void) {
    /* Valeur attendue : floor(65536 * 0.959 * 0.8) = 50303 (±500 pour arrondi flottant) */
    hp_filter_t f = fresh_filter();
    int32_t output = process_sample(&f, NULL,RAW_SAMPLE_POS);
    TEST_ASSERT_INT32_WITHIN(500, 50303, output);
}

/* ── Symétrie signe ──────────────────────────────────────────────────────── */

void test_positive_and_negative_inputs_are_symmetric(void) {
    hp_filter_t f_pos = fresh_filter();
    hp_filter_t f_neg = fresh_filter();
    int32_t out_pos = process_sample(&f_pos, NULL,RAW_SAMPLE_POS);
    int32_t out_neg = process_sample(&f_neg, NULL,RAW_SAMPLE_NEG);
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
        int32_t out = process_sample(&f, NULL,sign * RAW_SAMPLE_POS);
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
    int32_t output = process_sample(&f, NULL,RAW_SAMPLE_POS);
    TEST_ASSERT_EQUAL_INT32(0, output);
}

/* ── État interne mis à jour correctement ────────────────────────────────── */

void test_state_last_x_is_updated(void) {
    hp_filter_t f = fresh_filter();
    /* input >> 8 = 65536 */
    process_sample(&f, NULL,RAW_SAMPLE_POS);
    /* last_x doit valoir 65536.0f */
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 65536.0f, f.last_x);
}

void test_state_reset_gives_clean_output(void) {
    hp_filter_t f = fresh_filter();
    /* Charger le filtre avec un signal DC */
    for (int i = 0; i < 50; i++) {
        process_sample(&f, NULL,RAW_SAMPLE_POS);
    }
    /* Reset manuel de l'état */
    f.last_x = 0.0f;
    f.last_y = 0.0f;
    /* Sur entrée nulle après reset : sortie nulle */
    int32_t output = process_sample(&f, NULL,0);
    TEST_ASSERT_EQUAL_INT32(0, output);
}

/* ── compute_hp/lp_alpha ─────────────────────────────────────────────────── */

void test_hp_alpha_range(void) {
    /* Pour fc = 300 Hz à 44100 Hz : alpha ≈ 0.9590, dans ]0 ; 1[. */
    float a = compute_hp_alpha(300, 44100);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.959f, a);
}

void test_lp_alpha_range(void) {
    /* alpha_lp + alpha_hp = 1 quand même fc. */
    float ahp = compute_hp_alpha(TEST_HP_CUTOFF, TEST_SAMPLE_RATE);
    float alp = compute_lp_alpha(TEST_HP_CUTOFF, TEST_SAMPLE_RATE);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, ahp + alp);
}

/* ── Filtre passe-bas ────────────────────────────────────────────────────── */

void test_lp_dc_passes_through(void) {
    /* Un signal DC doit atteindre la sortie LP après convergence. */
    lp_filter_t lp = fresh_lp();
    int32_t output = 0;
    for (int i = 0; i < 500; i++) {
        output = process_sample(NULL, &lp, RAW_SAMPLE_POS);
    }
    /* Après convergence : sortie ≈ entrée décalée * gain = 65536 * 0.8 = 52428 (±2000). */
    TEST_ASSERT_INT32_WITHIN(2000, 52428, output);
}

void test_lp_nyquist_attenuated(void) {
    /* Un signal à la fréquence de Nyquist (alternance ±sample) doit être fortement
       atténué par le LP (fc=8kHz << Nyquist=22050Hz). */
    lp_filter_t lp = fresh_lp();
    int32_t sign = 1;
    int32_t peak = 0;
    for (int i = 0; i < 200; i++) {
        int32_t out = process_sample(NULL, &lp, sign * RAW_SAMPLE_POS);
        int32_t a = iabs32(out);
        if (a > peak) peak = a;
        sign = -sign;
    }
    /* Le passe-bas doit couper le signal à Nyquist : pic < 10 % de l'entrée décalée (65536). */
    TEST_ASSERT_LESS_THAN_INT32(6554, peak);
}

void test_lp_zero_input_gives_zero_output(void) {
    lp_filter_t lp = fresh_lp();
    TEST_ASSERT_EQUAL_INT32(0, process_sample(NULL, &lp, 0));
}

/* ── Passe-bande (HP + LP simultanés) ───────────────────────────────────── */

void test_bandpass_dc_rejected(void) {
    /* DC bloquée par le HP même si LP actif. */
    hp_filter_t hp = fresh_hp();
    lp_filter_t lp = fresh_lp();
    int32_t output = 0;
    for (int i = 0; i < 200; i++) {
        output = process_sample(&hp, &lp, RAW_SAMPLE_POS);
    }
    TEST_ASSERT_INT32_WITHIN(656, 0, output);
}

void test_bandpass_nyquist_attenuated(void) {
    /* Nyquist bloqué par le LP même si HP actif. */
    hp_filter_t hp = fresh_hp();
    lp_filter_t lp = fresh_lp();
    int32_t sign = 1;
    int32_t peak = 0;
    for (int i = 0; i < 200; i++) {
        int32_t out = process_sample(&hp, &lp, sign * RAW_SAMPLE_POS);
        int32_t a = iabs32(out);
        if (a > peak) peak = a;
        sign = -sign;
    }
    TEST_ASSERT_LESS_THAN_INT32(6554, peak);
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
    RUN_TEST(test_hp_alpha_range);
    RUN_TEST(test_lp_alpha_range);
    RUN_TEST(test_lp_dc_passes_through);
    RUN_TEST(test_lp_nyquist_attenuated);
    RUN_TEST(test_lp_zero_input_gives_zero_output);
    RUN_TEST(test_bandpass_dc_rejected);
    RUN_TEST(test_bandpass_nyquist_attenuated);

    return UNITY_END();
}
