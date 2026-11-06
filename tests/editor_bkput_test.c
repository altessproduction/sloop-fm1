/* SPDX-License-Identifier: GPL-3.0-only */
/* Deterministic BK_PUT integration regressions on the real editor + simulated NOR harness.
 * Built by tests/editor_fuzz.sh alongside the randomized fuzzer. */
#define main editor_fuzz_main
#include "editor_fuzz.c"
#undef main

static void bk_send_exact(uint32_t id, const uint8_t *raw, uint32_t len, uint32_t crc, uint32_t send_len)
{
    uint8_t a[320];
    uint32_t n, pos = 0;
    a[0] = 0; a[1] = (uint8_t)id;
    put5(a + 2, len);
    put5(a + 7, crc);
    cmdv(ED_BK_PUT, a, 12);
    if (rx_n < 8 || rx_last[5] != 0 || rx_last[7] != 0)
        fail("BK_PUT begin rejected");
    while (pos < send_len) {
        uint32_t k = send_len - pos > 256u ? 256u : send_len - pos;
        a[0] = 1; a[1] = (uint8_t)id;
        put5(a + 2, pos);
        n = 7u + pack7(a + 7, raw + pos, k);
        cmdv(ED_BK_PUT, a, n);
        if (rx_n < 8 || rx_last[5] != 1 || rx_last[7] != 0)
            fail("BK_PUT data rejected");
        pos += k;
    }
    a[0] = 2; a[1] = (uint8_t)id;
    cmdv(ED_BK_PUT, a, 2);
}

static void expect_commit_rc(uint32_t rc, const char *what)
{
    if (rx_n < 8 || rx_last[5] != 2 || rx_last[7] != rc) {
        fprintf(stderr, "%s: got rc %u (reply bytes %u)\n", what, rx_n >= 8 ? rx_last[7] : 255u, rx_n);
        fail(what);
    }
}

static void test_bad_crc_and_incomplete(void)
{
    project_t before, after;
    uint8_t raw[sizeof(project_t)];
    uint8_t a[320];
    uint32_t len = sizeof(project_t), n;
    stop_all();
    proj_capture(&before);
    memcpy(raw, &before, len);

    a[0] = 0; a[1] = 0; put5(a + 2, len); put5(a + 7, st_crc32(raw, len) ^ 1u);
    cmdv(ED_BK_PUT, a, 12);
    if (rx_n < 8 || rx_last[7] != 0) fail("bad-CRC begin");
    n = 7u + pack7(a + 7, raw, len > 256u ? 256u : len);
    a[0] = 1; a[1] = 0; put5(a + 2, 0);
    cmdv(ED_BK_PUT, a, n);
    if (rx_n < 8 || rx_last[7] != 0) fail("bad-CRC payload");
    /* Send the remainder so only the CRC, not the length, is wrong. */
    {
        uint32_t pos = len > 256u ? 256u : len;
        while (pos < len) {
            uint32_t k = len - pos > 256u ? 256u : len - pos;
            a[0] = 1; a[1] = 0; put5(a + 2, pos);
            n = 7u + pack7(a + 7, raw + pos, k);
            cmdv(ED_BK_PUT, a, n);
            if (rx_n < 8 || rx_last[7] != 0) fail("bad-CRC remaining payload");
            pos += k;
        }
    }
    a[0] = 2; a[1] = 0; cmdv(ED_BK_PUT, a, 2);
    expect_commit_rc(2, "incorrect BK_PUT CRC must be rejected");
    proj_capture(&after);
    if (memcmp(&before, &after, sizeof before)) fail("incorrect BK_PUT CRC changed the working project");

    /* Start a fresh transfer but commit after only its first chunk. */
    a[0] = 0; a[1] = 0; put5(a + 2, len); put5(a + 7, st_crc32(raw, len));
    cmdv(ED_BK_PUT, a, 12);
    if (rx_n < 8 || rx_last[7] != 0) fail("incomplete transfer begin");
    a[0] = 1; a[1] = 0; put5(a + 2, 0);
    n = 7u + pack7(a + 7, raw, 128u);
    cmdv(ED_BK_PUT, a, n);
    if (rx_n < 8 || rx_last[7] != 0) fail("incomplete transfer payload");
    a[0] = 2; a[1] = 0; cmdv(ED_BK_PUT, a, 2);
    expect_commit_rc(2, "incomplete BK_PUT must be rejected");
    proj_capture(&after);
    if (memcmp(&before, &after, sizeof before)) fail("incomplete BK_PUT changed the working project");
}

static void test_fun6_migration(void)
{
    project_t current, restored;
    project_v6_t old;
    uint32_t i;
    stop_all();
    proj_capture(&current);
    memset(&old, 0, sizeof old);
    old.magic = PROJ_MAGIC_V6;
    old.size = sizeof old;
    memcpy(old.g, current.g, sizeof old.g);
    old.sel = current.sel;
    old.drdly = current.drdly;
    memcpy(old.rsv, current.rsv, sizeof old.rsv);
    memcpy(old.t, current.t, sizeof old.t);
    old.sum = proj_v6_sum(&old);
    bk_send_exact(0, (const uint8_t *)&old, sizeof old, st_crc32(&old, sizeof old), sizeof old);
    expect_commit_rc(0, "FUN6 BK_PUT migration");
    audio_blocks(2);
    proj_capture(&restored);
    if (restored.magic != PROJ_MAGIC || restored.size != sizeof restored)
        fail("FUN6 BK_PUT did not migrate to FUN7");
    if (restored.roots_enabled)
        fail("FUN6 migration must leave Roots Dub disabled");
    for (i = 0; i < 4u; i++)
        if (restored.roots_gain[i] != 32768u)
            fail("FUN6 migration must initialize neutral Roots Dub gains");
}

static void test_section_flash_failure(void)
{
    project_t original, replacement;
    uint8_t raw[sizeof(project_t)];
    uint32_t erases_before;
    stop_all();
    project_save(0);
    if (proj_slot[0].magic != PROJ_MAGIC) fail("section setup save failed");
    memcpy(&original, &proj_slot[0], sizeof original);
    erases_before = fl_erases;
    proj_capture(&replacement);
    replacement.roots_gain[0] = 0u;
    replacement.sum = proj_sum(&replacement);
    memcpy(raw, &replacement, sizeof raw);

    fl_fail_at = 1; /* first page program after the alternate A/B sector has been erased */
    bk_send_exact(2, raw, sizeof raw, st_crc32(raw, sizeof raw), sizeof raw);
    if (fl_fail_at != -1 || fl_erases != erases_before + 1u) {
        fl_fail_at = -1;
        fail("section BK_PUT did not reach the injected post-erase flash failure");
    }
    expect_commit_rc(4, "section BK_PUT must report injected flash failure");
    if (memcmp(&original, &proj_slot[0], sizeof original))
        fail("failed section BK_PUT changed the in-RAM previous section");
    /* A fresh load must still resolve the old committed A/B copy. */
    memset(&proj_slot[0], 0, sizeof proj_slot[0]);
    project_load(0);
    if (memcmp(&original, &proj_slot[0], sizeof original))
        fail("failed section BK_PUT did not preserve the previous flash copy");
}

int main(void)
{
    harness_init();
    after();
    test_bad_crc_and_incomplete();
    test_fun6_migration();
    test_section_flash_failure();
    puts("deterministic BK_PUT integration tests passed");
    return 0;
}
