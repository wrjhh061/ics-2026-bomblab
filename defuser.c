#include "defuser.h"

#include <inttypes.h>
#include <stdio.h>

/* Reference implementation for this personal bomb. Use uint32_t arithmetic
 * except when extracting bits from a uint64_t challenge or context. */
static int write_answer(char *out, size_t cap, const uint32_t *values, size_t count) {
    if (out == NULL || cap == 0 || count == 0) return 1;
    size_t used = 0;
    for (size_t i = 0; i < count; ++i) {
        int size = snprintf(out + used, cap - used, "%s%" PRIu32, i ? " " : "", values[i]);
        if (size < 0 || (size_t)size >= cap - used) return 1;
        used += (size_t)size;
    }
    return 0;
}

int solve_phase_1(uint64_t challenge, char *out, size_t cap) {
    uint32_t low = (uint32_t)challenge;
    uint32_t middle = (uint32_t)(challenge >> 16);
    uint32_t high = (uint32_t)(challenge >> 32);
    uint32_t first = (high * UINT32_C(3) + middle) ^ UINT32_C(0xb78f);
    uint32_t second = (low + high * UINT32_C(4)) ^ UINT32_C(0x35f4);
    uint32_t values[] = {second * UINT32_C(2) + (first ^ UINT32_C(0xb171))};
    return write_answer(out, cap, values, 1);
}

int solve_phase_2(uint64_t challenge, char *out, size_t cap) {
    uint32_t values[6];
    values[0] = (uint32_t)((challenge >> 38) & UINT64_C(127)) + UINT32_C(15);
    for (unsigned i = 1; i < 6; ++i) {
        uint32_t delta = (uint32_t)((challenge >> (15 + i)) & UINT64_C(15)) ^ i;
        values[i] = values[i - 1] * UINT32_C(4) + delta;
    }
    return write_answer(out, cap, values, 6);
}

int solve_phase_3(uint64_t challenge, char *out, size_t cap) {
    uint32_t index = (uint32_t)challenge & UINT32_C(7);
    uint32_t x = (uint32_t)(challenge >> 3) ^ (uint32_t)(challenge >> 32);
    uint32_t answer;
    /* Jump table targets are cases 3, 7, 1, 6, 4, 2, 5, 0. */
    switch (index) {
        case 0: answer = (x * UINT32_C(4)) ^ UINT32_C(0x1bf6); break;
        case 1: answer = x * UINT32_C(5) + UINT32_C(0x1d4c); break;
        case 2: answer = (x ^ UINT32_C(0x8e8f)) + (x >> 2); break;
        case 3: answer = (x * UINT32_C(4)) ^ UINT32_C(0x3bd2); break;
        case 4: answer = (x ^ UINT32_C(0xe22b)) + (x >> 1); break;
        case 5: answer = x * UINT32_C(3) + UINT32_C(0xd12a); break;
        case 6: answer = x * UINT32_C(3) + UINT32_C(0x2513); break;
        default: answer = x * UINT32_C(5) + UINT32_C(0x1efd7); break;
    }
    uint32_t values[] = {index, answer};
    return write_answer(out, cap, values, 2);
}

static uint32_t walk(uint32_t target, uint32_t low, uint32_t high, uint32_t salt) {
    uint32_t middle = low + ((high - low) >> 1);
    if (target == middle) return (middle + UINT32_C(0x22)) ^ salt;
    uint32_t child;
    uint32_t branch;
    if (target < middle) {
        child = walk(target, low, middle - 1, salt);
        branch = UINT32_C(6);
    } else {
        child = walk(target, middle + 1, high, salt);
        branch = UINT32_C(10);
    }
    return (child * UINT32_C(5)) ^ (middle + salt) ^ branch;
}

int solve_phase_4(uint64_t challenge, char *out, size_t cap) {
    uint32_t target = (uint32_t)challenge & UINT32_C(127);
    uint32_t salt = (uint32_t)(challenge >> 7) ^ (uint32_t)(challenge >> 32);
    uint32_t values[] = {walk(target, 0, 127, salt)};
    return write_answer(out, cap, values, 1);
}

int solve_phase_5(uint64_t challenge, char *out, size_t cap) {
    /* Original node offsets: value +0, tag +4, next +8. Store links as indices. */
    static const struct {
        uint32_t value;
        uint32_t tag;
        unsigned next;
    } nodes[12] = {
        {UINT32_C(0x71ff), UINT32_C(0x017), 1},
        {UINT32_C(0x14fb), UINT32_C(0x60c), 10},
        {UINT32_C(0xf0dd), UINT32_C(0x844), 5},
        {UINT32_C(0x8a93), UINT32_C(0x545), 12},
        {UINT32_C(0x27f9), UINT32_C(0x97d), 7},
        {UINT32_C(0xbd4f), UINT32_C(0x125), 0},
        {UINT32_C(0x490f), UINT32_C(0x418), 3},
        {UINT32_C(0xe444), UINT32_C(0x264), 9},
        {UINT32_C(0x023d), UINT32_C(0x37a), 4},
        {UINT32_C(0x8479), UINT32_C(0xa4a), 6},
        {UINT32_C(0xe651), UINT32_C(0xb6c), 8},
        {UINT32_C(0x9515), UINT32_C(0x71a), 2},
    };
    static const unsigned heads[8] = {11, 2, 5, 0, 1, 10, 8, 4};
    unsigned node = heads[challenge & UINT64_C(7)];
    unsigned steps = (unsigned)((challenge >> 3) & UINT64_C(3));
    uint32_t accumulator = (uint32_t)(challenge >> 6)
                           ^ (uint32_t)(challenge >> 32) ^ UINT32_C(0x6fdf);
    uint32_t tag = 0;
    for (unsigned i = 0; i <= steps; ++i) {
        tag = nodes[node].tag;
        accumulator = (accumulator ^ nodes[node].value) + tag;
        if (i < steps) node = nodes[node].next;
    }
    uint32_t values[] = {accumulator, tag};
    return write_answer(out, cap, values, 2);
}

static uint32_t rotate_right(uint32_t x, unsigned bits) {
    /* All callers use 1 or 5; neither shift is by 0 or 32. */
    return (x >> bits) | (x << (32 - bits));
}

static uint32_t operation_0(uint32_t x) {
    return ((x << 2) | (x >> 30)) ^ UINT32_C(0xb2a5);
}

static uint32_t operation_1(uint32_t x) {
    return rotate_right(x, 5);
}

static uint32_t operation_2(uint32_t x) {
    return (x + UINT32_C(0x59a)) ^ (x >> 5);
}

static uint32_t operation_3(uint32_t x) {
    return (x * UINT32_C(8) + UINT32_C(0xafcc)) ^ (x * UINT32_C(7));
}

static uint32_t operation_4(uint32_t x) {
    return (x ^ UINT32_C(0xcdea)) + x * UINT32_C(8);
}

static uint32_t operation_5(uint32_t x) {
    return (x ^ (x >> 2)) + UINT32_C(0x5325);
}

int solve_phase_6(uint64_t challenge, char *out, size_t cap) {
    static uint32_t (*const operations[8])(uint32_t) = {
        operation_2, operation_1, operation_3, operation_4,
        operation_5, operation_4, operation_4, operation_0
    };
    unsigned first = (unsigned)(challenge & UINT64_C(7));
    unsigned second = (unsigned)((challenge >> 3) & UINT64_C(7));
    uint32_t x = (uint32_t)(challenge >> 6) ^ (uint32_t)(challenge >> 32);
    uint32_t intermediate = operations[first](x) ^ UINT32_C(0xf7cf);
    uint32_t values[] = {operations[second](intermediate) + UINT32_C(0x51f8)};
    return write_answer(out, cap, values, 1);
}

int solve_secret(uint64_t challenge, uint64_t context, char *out, size_t cap) {
    if (out == NULL || cap == 0) return 1;
    uint32_t mode = (uint32_t)context & UINT32_C(3);
    uint32_t a = (uint32_t)challenge ^ (uint32_t)(challenge >> 32);
    uint32_t b = (uint32_t)context ^ (uint32_t)(context >> 32);
    uint32_t first = rotate_right(a ^ b, 1) ^ UINT32_C(0x575f)
                     ^ (mode * UINT32_C(0x567c)) ^ (uint32_t)(context >> 16);
    uint32_t second = first ^ ((a >> 1) + b + mode + UINT32_C(0x123));
    /* The local object holds 32 prefix bytes, little-endian OPEN, then text. */
    int size = snprintf(out, cap, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAOPEN%" PRIu32
                        " %" PRIu32 " %" PRIu32, mode, first, second);
    return size < 0 || (size_t)size >= cap;
}
