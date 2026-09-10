#pragma once

/*
 * CYD_AnimatedPixelClock - Mario sprite art
 *
 * Transcribed from the classic Super Mario Bros. small-Mario sprite: 12 wide by
 * 16 tall, facing right. Upstream's figure was 8x10 and heavily abstracted -
 * the whole head was one red block and the torso one blue block, with no face,
 * hair, moustache, shirt or buttons - which is why it read as a red-and-blue
 * blob rather than as Mario.
 *
 * ---- Editing ---------------------------------------------------------------
 * The art is deliberately kept as rows of characters rather than packed bytes
 * so it can be read and edited in place. One character is one sprite pixel:
 *
 *   .  transparent      R  cap and shirt       F  face / skin
 *   H  hair             B  overalls            K  black (eye, moustache)
 *   Y  overall button   S  shoe
 *
 * Every row must be exactly MARIO_W characters. A static_assert cannot check
 * that, so the drawing code stops at the row's terminator to stay safe.
 *
 * Left-facing is drawn by mirroring at draw time - there is no separate art.
 */

#include <Arduino.h>

constexpr int MARIO_W = 12;
constexpr int MARIO_H = 16;

// Standing / idle.
static const char *const MARIO_STAND[MARIO_H] = {
    "....RRRRR...",
    "...RRRRRRRRR",
    "...HHHFFKF..",
    "..HFHHFFFKFF",
    "..HFHHHFFFKF",
    "..HHFFFFFFF.",
    "....FFFFFF..",
    "...RRBRRR...",
    "..RRRBRRBRR.",
    ".RRRRBBBBRRR",
    "FFRRBYBBYBRR",
    "FFFBBBBBBBFF",
    "..BBBBBBBB..",
    "..BBB..BBB..",
    ".SSS....SSS.",
    "SSSS....SSSS",
};

// Walk frame A - trailing leg back, arms swinging.
static const char *const MARIO_WALK_A[MARIO_H] = {
    "....RRRRR...",
    "...RRRRRRRRR",
    "...HHHFFKF..",
    "..HFHHFFFKFF",
    "..HFHHHFFFKF",
    "..HHFFFFFFF.",
    "....FFFFFF..",
    "...RRRRRR.FF",
    "..RRRBRRBRFF",
    ".RRRRBBBBRRF",
    "FFRRBBYBBYBR",
    "FFFBBBBBBBB.",
    ".BBBBBBBBB..",
    "..BBBBBB....",
    "...SSSSS....",
    "..SSS..SS...",
};

// Walk frame B - full stride, legs apart.
static const char *const MARIO_WALK_B[MARIO_H] = {
    "....RRRRR...",
    "...RRRRRRRRR",
    "...HHHFFKF..",
    "..HFHHFFFKFF",
    "..HFHHHFFFKF",
    "..HHFFFFFFF.",
    "....FFFFFF..",
    "FF.RRRRRR...",
    "FFRRRBRRBRR.",
    "FRRRRBBBBRRR",
    ".RRRBBYBBYBR",
    "..BBBBBBBBBF",
    ".BBBBBBBBBFF",
    ".BBB..BBB...",
    "SSSS..SSSS..",
    "SSS....SSSS.",
};

// Jumping - one arm raised, legs tucked. Classic pose.
static const char *const MARIO_JUMP[MARIO_H] = {
    "....RRRRR.FF",
    "...RRRRRRRFF",
    "...HHHFFKFF.",
    "..HFHHFFFKFF",
    "..HFHHHFFFKF",
    "..HHFFFFFFF.",
    "....FFFFFF..",
    "..RRRRRRRR..",
    ".RRRBRRBRRR.",
    "FRRRBBBBBRRR",
    "FFRBBYBBYBBF",
    "FF.BBBBBBBFF",
    "...BBBBBBB..",
    "..BBB...BBB.",
    ".SSSS...SSS.",
    "SSSS......S.",
};
