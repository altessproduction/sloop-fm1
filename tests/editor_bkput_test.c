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

static void test_fun7_roundtrip(void)
{
    project_t before, replacement, restored;
    uint8_t raw[sizeof(project_t)];
    stop_all();
    proj_capture(&before);
    replacement = before;
    replacement.roots_enabled = 1u;
    replacement.roots_gain[0] = 0u;
    replacement.roots_gain[1] = 8192u;
    replacement.roots_gain[2] = 16384u;
    replacement.roots_gain[3] = 32768u;
    replacement.sum = proj_sum(&replacement);
    memcpy(raw, &replacement, sizeof raw);

    bk_send_exact(0, raw, sizeof raw, st_crc32(raw, sizeof raw), sizeof raw);
    expect_commit_rc(0, "valid FUN7 BK_PUT restore");
    audio_blocks(2);
    proj_capture(&restored);
    if (!restored.roots_enabled ||
        restored.roots_gain[0] != 0u ||
        restored.roots_gain[1] != 8192u ||
        restored.roots_gain[2] != 16384u ||
        restored.roots_gain[3] != 32768u)
        fail("valid FUN7 BK_PUT did not apply and retain ROOTS settings");

    /* Restore the initial working project so later cases remain independent. */
    memcpy(raw, &before, sizeof raw);
    bk_send_exact(0, raw, sizeof raw, st_crc32(raw, sizeof raw), sizeof raw);
    expect_commit_rc(0, "restore initial project after FUN7 roundtrip");
    audio_blocks(2);
    proj_capture(&restored);
    if (memcmp(&before, &restored, sizeof before))
        fail("FUN7 roundtrip cleanup did not restore the initial project");
}

static void test_project_checksum_rejected(void)
{
    project_t before, corrupted, after;
    uint8_t raw[sizeof(project_t)];
    stop_all();
    proj_capture(&before);
    corrupted = before;
    corrupted.sum ^= 1u; /* valid BK_PUT transport CRC, invalid FUN7 project checksum */
    memcpy(raw, &corrupted, sizeof raw);

    bk_send_exact(0, raw, sizeof raw, st_crc32(raw, sizeof raw), sizeof raw);
    expect_commit_rc(2, "invalid FUN7 project checksum must be rejected");
    proj_capture(&after);
    if (memcmp(&before, &after, sizeof before))
        fail("invalid FUN7 project checksum changed the working project");
}

static void test_out_of_order_and_abort(void)
{
    project_t before, after;
    uint8_t raw[sizeof(project_t)];
    uint8_t a[320];
    uint32_t len = sizeof(project_t), n;
    stop_all();
    proj_capture(&before);
    memcpy(raw, &before, len);

    a[0] = 0; a[1] = 0; put5(a + 2, len); put5(a + 7, st_crc32(raw, len));
    cmdv(ED_BK_PUT, a, 12);
    if (rx_n < 8 || rx_last[7] != 0) fail("out-of-order transfer begin");

    /* The protocol accepts only the exact next offset; this must not advance staging. */
    a[0] = 1; a[1] = 0; put5(a + 2, 1);
    n = 7u + pack7(a + 7, raw, 128u);
    cmdv(ED_BK_PUT, a, n);
    if (rx_n < 8 || rx_last[7] != 1) fail("out-of-order BK_PUT chunk must be rejected");

    a[0] = 3; a[1] = 0; cmdv(ED_BK_PUT, a, 2);
    if (rx_n < 8 || rx_last[5] != 3 || rx_last[7] != 0)
        fail("BK_PUT abort must close the staged transfer");
    proj_capture(&after);
    if (memcmp(&before, &after, sizeof before))
        fail("out-of-order/aborted BK_PUT changed the working project");
}

static void test_transport_busy(void)
{
    project_t before, after;
    uint8_t raw[sizeof(project_t)];
    uint8_t a[320];
    uint32_t len = sizeof(project_t), n, pos = 0;
    stop_all();
    proj_capture(&before);
    memcpy(raw, &before, len);
    a[0] = 0; a[1] = 0; put5(a + 2, len); put5(a + 7, st_crc32(raw, len));
    cmdv(ED_BK_PUT, a, 12);
    if (rx_n < 8 || rx_last[7] != 0) fail("busy transfer begin");
    while (pos < len) {
        uint32_t k = len - pos > 256u ? 256u : len - pos;
        a[0] = 1; a[1] = 0; put5(a + 2, pos);
        n = 7u + pack7(a + 7, raw + pos, k);
        cmdv(ED_BK_PUT, a, n);
        if (rx_n < 8 || rx_last[7] != 0) fail("busy transfer data");
        pos += k;
    }
    song.playing = 1;
    a[0] = 2; a[1] = 0; cmdv(ED_BK_PUT, a, 2);
    song.playing = 0;
    expect_commit_rc(3, "BK_PUT commit must reject active transport");
    proj_capture(&after);
    if (memcmp(&before, &after, sizeof before))
        fail("busy BK_PUT changed the working project");
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

static void test_section_fun7_roundtrip(void)
{
    project_t original, replacement, persisted;
    uint8_t raw[sizeof(project_t)];
    stop_all();
    project_save(0);
    if (!flash_ok || !proj_ok(&proj_slot[0]))
        fail("section FUN7 setup save failed");
    memcpy(&original, &proj_slot[0], sizeof original);

    replacement = original;
    replacement.roots_enabled = 1u;
    replacement.roots_gain[0] = 0u;
    replacement.roots_gain[1] = 8192u;
    replacement.roots_gain[2] = 24576u;
    replacement.roots_gain[3] = 32768u;
    replacement.sum = proj_sum(&replacement);
    memcpy(raw, &replacement, sizeof raw);
    bk_send_exact(2, raw, sizeof raw, st_crc32(raw, sizeof raw), sizeof raw);
    expect_commit_rc(0, "valid FUN7 section BK_PUT restore");
    if (memcmp(&replacement, &proj_slot[0], sizeof replacement))
        fail("valid FUN7 section BK_PUT did not update the RAM section");
    if (st_load(OBJ_PROJECT0, &persisted, sizeof persisted) != (int)sizeof persisted ||
        memcmp(&replacement, &persisted, sizeof replacement))
        fail("valid FUN7 section BK_PUT did not persist ROOTS settings to flash");

    /* Put the original section back so later tests start from the same committed state. */
    memcpy(raw, &original, sizeof raw);
    bk_send_exact(2, raw, sizeof raw, st_crc32(raw, sizeof raw), sizeof raw);
    expect_commit_rc(0, "restore original section after FUN7 roundtrip");
    if (memcmp(&original, &proj_slot[0], sizeof original) ||
        st_load(OBJ_PROJECT0, &persisted, sizeof persisted) != (int)sizeof persisted ||
        memcmp(&original, &persisted, sizeof original))
        fail("FUN7 section roundtrip cleanup did not restore RAM and flash");
}

static void test_section_flash_failure(void)
{
    project_t original, replacement, persisted;
    uint8_t raw[sizeof(project_t)];
    uint32_t erases_before;
    stop_all();
    project_save(0);
    if (!flash_ok || proj_slot[0].magic != PROJ_MAGIC) fail("section setup save failed");
    if (st_load(OBJ_PROJECT0, &persisted, sizeof persisted) != (int)sizeof persisted ||
        memcmp(&persisted, &proj_slot[0], sizeof persisted))
        fail("section setup was not committed to the simulated NOR");
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
    test_fun7_roundtrip();
    test_project_checksum_rejected();
    test_out_of_order_and_abort();
    test_transport_busy();
    test_fun6_migration();
    test_section_fun7_roundtrip();
    test_section_flash_failure();
    puts("deterministic BK_PUT integration tests passed");
    return 0;
}
