#include "unity.h"
#include "audio_buffer.h"
#include <string.h>

static audio_pipeline_t pipeline;

void setUp(void)    { audio_pipeline_init(&pipeline); }
void tearDown(void) {}

/* ── Init ────────────────────────────────────────────────────────────────── */

void test_init_all_blocks_free(void) {
    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        TEST_ASSERT_EQUAL_INT(BLOCK_FREE, pipeline.state[i]);
    }
}

void test_init_indices_zero(void) {
    TEST_ASSERT_EQUAL_UINT8(0, pipeline.dma_write_idx);
    TEST_ASSERT_EQUAL_UINT8(0, pipeline.process_read_idx);
    TEST_ASSERT_EQUAL_UINT8(0, pipeline.sd_write_idx);
}

void test_init_stats_zero(void) {
    TEST_ASSERT_EQUAL_UINT32(0, pipeline.overrun_count);
    TEST_ASSERT_EQUAL_UINT32(0, pipeline.blocks_captured);
    TEST_ASSERT_EQUAL_UINT32(0, pipeline.blocks_written);
}

/* ── DMA side ────────────────────────────────────────────────────────────── */

void test_get_dma_buffer_returns_valid_ptr(void) {
    int32_t *buf = audio_pipeline_get_dma_buffer(&pipeline);
    TEST_ASSERT_NOT_NULL(buf);
}

void test_dma_fill_transitions_to_dma_filling(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    TEST_ASSERT_EQUAL_INT(BLOCK_DMA_FILLING, pipeline.state[0]);
}

void test_dma_complete_transitions_to_ready(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    TEST_ASSERT_EQUAL_INT(BLOCK_READY, pipeline.state[0]);
}

void test_dma_complete_increments_blocks_captured(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    TEST_ASSERT_EQUAL_UINT32(1, pipeline.blocks_captured);
}

void test_dma_advances_write_idx(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    TEST_ASSERT_EQUAL_UINT8(1, pipeline.dma_write_idx);
}

/* ── Core 0 : traitement ─────────────────────────────────────────────────── */

void test_process_not_available_on_free_block(void) {
    TEST_ASSERT_FALSE(audio_pipeline_process_available(&pipeline));
}

void test_process_available_after_dma_complete(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    TEST_ASSERT_TRUE(audio_pipeline_process_available(&pipeline));
}

void test_get_process_buffer_transitions_to_processing(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    TEST_ASSERT_EQUAL_INT(BLOCK_PROCESSING, pipeline.state[0]);
}

void test_get_process_buffer_returns_null_if_not_ready(void) {
    int32_t *buf = audio_pipeline_get_process_buffer(&pipeline);
    TEST_ASSERT_NULL(buf);
}

void test_process_done_transitions_to_writing(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    TEST_ASSERT_EQUAL_INT(BLOCK_WRITING, pipeline.state[0]);
}

void test_process_done_advances_process_read_idx(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    TEST_ASSERT_EQUAL_UINT8(1, pipeline.process_read_idx);
}

/* ── Core 1 : écriture SD ───────────────────────────────────────────────── */

void test_write_not_available_on_free_block(void) {
    TEST_ASSERT_FALSE(audio_pipeline_write_available(&pipeline));
}

void test_write_available_after_process_done(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    TEST_ASSERT_TRUE(audio_pipeline_write_available(&pipeline));
}

void test_get_write_buffer_returns_null_if_not_writing(void) {
    int32_t *buf = audio_pipeline_get_write_buffer(&pipeline);
    TEST_ASSERT_NULL(buf);
}

void test_write_done_transitions_to_free(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    audio_pipeline_get_write_buffer(&pipeline);
    audio_pipeline_write_done(&pipeline);
    TEST_ASSERT_EQUAL_INT(BLOCK_FREE, pipeline.state[0]);
}

void test_write_done_increments_blocks_written(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    audio_pipeline_get_write_buffer(&pipeline);
    audio_pipeline_write_done(&pipeline);
    TEST_ASSERT_EQUAL_UINT32(1, pipeline.blocks_written);
}

void test_write_done_advances_sd_write_idx(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    audio_pipeline_get_write_buffer(&pipeline);
    audio_pipeline_write_done(&pipeline);
    TEST_ASSERT_EQUAL_UINT8(1, pipeline.sd_write_idx);
}

/* ── Overrun ─────────────────────────────────────────────────────────────── */

void test_overrun_on_full_ring(void) {
    /* Remplir tous les blocs sans consommer */
    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        audio_pipeline_get_dma_buffer(&pipeline);
        audio_pipeline_dma_complete(&pipeline);
    }
    /* Le prochain appel DMA tombe sur un bloc non-FREE → overrun */
    audio_pipeline_get_dma_buffer(&pipeline);
    TEST_ASSERT_GREATER_THAN_UINT32(0, pipeline.overrun_count);
}

void test_overrun_count_stays_zero_on_normal_cycle(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    audio_pipeline_get_write_buffer(&pipeline);
    audio_pipeline_write_done(&pipeline);
    TEST_ASSERT_EQUAL_UINT32(0, pipeline.overrun_count);
}

/* ── Ring wrap ───────────────────────────────────────────────────────────── */

void test_ring_indices_wrap_after_full_cycles(void) {
    for (int i = 0; i < AUDIO_BLOCK_COUNT; i++) {
        audio_pipeline_get_dma_buffer(&pipeline);
        audio_pipeline_dma_complete(&pipeline);
        audio_pipeline_get_process_buffer(&pipeline);
        audio_pipeline_process_done(&pipeline);
        audio_pipeline_get_write_buffer(&pipeline);
        audio_pipeline_write_done(&pipeline);
    }
    TEST_ASSERT_EQUAL_UINT8(0, pipeline.dma_write_idx);
    TEST_ASSERT_EQUAL_UINT8(0, pipeline.process_read_idx);
    TEST_ASSERT_EQUAL_UINT8(0, pipeline.sd_write_idx);
}

/* ── Pending count ───────────────────────────────────────────────────────── */

void test_pending_count_zero_at_init(void) {
    TEST_ASSERT_EQUAL_UINT8(0, audio_pipeline_get_pending_count(&pipeline));
}

void test_pending_count_equals_ready_blocks(void) {
    for (int i = 0; i < 3; i++) {
        audio_pipeline_get_dma_buffer(&pipeline);
        audio_pipeline_dma_complete(&pipeline);
    }
    TEST_ASSERT_EQUAL_UINT8(3, audio_pipeline_get_pending_count(&pipeline));
}

void test_pending_count_decreases_on_write_done(void) {
    audio_pipeline_get_dma_buffer(&pipeline);
    audio_pipeline_dma_complete(&pipeline);
    audio_pipeline_get_process_buffer(&pipeline);
    audio_pipeline_process_done(&pipeline);
    audio_pipeline_get_write_buffer(&pipeline);
    audio_pipeline_write_done(&pipeline);
    TEST_ASSERT_EQUAL_UINT8(0, audio_pipeline_get_pending_count(&pipeline));
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_init_all_blocks_free);
    RUN_TEST(test_init_indices_zero);
    RUN_TEST(test_init_stats_zero);

    RUN_TEST(test_get_dma_buffer_returns_valid_ptr);
    RUN_TEST(test_dma_fill_transitions_to_dma_filling);
    RUN_TEST(test_dma_complete_transitions_to_ready);
    RUN_TEST(test_dma_complete_increments_blocks_captured);
    RUN_TEST(test_dma_advances_write_idx);

    RUN_TEST(test_process_not_available_on_free_block);
    RUN_TEST(test_process_available_after_dma_complete);
    RUN_TEST(test_get_process_buffer_transitions_to_processing);
    RUN_TEST(test_get_process_buffer_returns_null_if_not_ready);
    RUN_TEST(test_process_done_transitions_to_writing);
    RUN_TEST(test_process_done_advances_process_read_idx);

    RUN_TEST(test_write_not_available_on_free_block);
    RUN_TEST(test_write_available_after_process_done);
    RUN_TEST(test_get_write_buffer_returns_null_if_not_writing);
    RUN_TEST(test_write_done_transitions_to_free);
    RUN_TEST(test_write_done_increments_blocks_written);
    RUN_TEST(test_write_done_advances_sd_write_idx);

    RUN_TEST(test_overrun_on_full_ring);
    RUN_TEST(test_overrun_count_stays_zero_on_normal_cycle);

    RUN_TEST(test_ring_indices_wrap_after_full_cycles);

    RUN_TEST(test_pending_count_zero_at_init);
    RUN_TEST(test_pending_count_equals_ready_blocks);
    RUN_TEST(test_pending_count_decreases_on_write_done);

    return UNITY_END();
}
