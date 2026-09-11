#pragma once

/*
 * CYD_AnimatedPixelClock - Canvas layout metrics
 *
 * Every clock style positions itself from these values instead of the literal
 * coordinates upstream used, which were tuned for a 128x64 HUB75 panel. They
 * derive from SCREEN_WIDTH / SCREEN_HEIGHT, so a new board is a new [env:]
 * block rather than another pass over fourteen files.
 *
 * ---- What scales and what does not -----------------------------------------
 * Every board renders at DISPLAY_SCALE 2, so one logical pixel is the same
 * physical size on each. That splits the metrics in two:
 *
 *   Sprites (Mario, ghosts, invaders, the dino) are fixed pixel art, so they
 *   cannot be redrawn larger. SPRITE_SCALE magnifies them at draw time instead
 *   - see CydDisplay::setSpriteScale - which keeps them in proportion to the
 *   digit row without touching a single pixel of the art.
 *
 *   Text scales with the canvas. Upstream's five digits filled 90 of 128 px,
 *   about 70% of the width. DIGIT_TEXT_SIZE keeps that proportion on both
 *   boards rather than leaving a big panel with a small clock on it.
 *
 * ---- Vertical composition ---------------------------------------------------
 * The character band is the constraint nobody can scale away: Mario bounces a
 * digit by putting his head against its underside, so the gap between the digit
 * row and the baseline has to stay one sprite tall. Everything else flows from
 * that.
 *
 *      0            top band      - date on some styles, effects, clouds, coins
 *      TIME_Y       digit row     - DIGIT_H tall
 *                   character band- CHAR_BAND tall, sprites walk on GROUND_Y
 *      GROUND_Y     baseline
 *      DATE_Y       date row
 *      DAY_Y        day-of-week row
 *      SCREEN_HEIGHT
 */

#include "config.h"

// ---- Digit row -------------------------------------------------------------
// 6*size px advance per character in the Adafruit GFX font, 8*size tall.
// /40 lands on size 4 at 160 wide and size 6 at 240, both ~75% of the canvas.
constexpr int DIGIT_TEXT_SIZE = SCREEN_WIDTH / 40;
constexpr int DIGIT_W = 6 * DIGIT_TEXT_SIZE;   // advance per digit
constexpr int DIGIT_H = 8 * DIGIT_TEXT_SIZE;   // glyph cell height
constexpr int DIGIT_GLYPH_W = 5 * DIGIT_TEXT_SIZE;  // inked width (advance - 1 col)

// "HH:MM" - five cells including the colon.
constexpr int TIME_STR_W = 5 * DIGIT_W;
constexpr int TIME_X = (SCREEN_WIDTH - TIME_STR_W) / 2;

// Per-digit left edges: hours, then colon, then minutes.
constexpr int DIGIT_X_0 = TIME_X;
constexpr int DIGIT_X_1 = TIME_X + DIGIT_W;
constexpr int DIGIT_X_2 = TIME_X + 2 * DIGIT_W;  // colon
constexpr int DIGIT_X_3 = TIME_X + 3 * DIGIT_W;
constexpr int DIGIT_X_4 = TIME_X + 4 * DIGIT_W;

// ---- Sprite magnification --------------------------------------------------
// Character sprites are fixed pixel art - Mario is 12x16 - so they cannot simply
// be redrawn larger. SPRITE_SCALE magnifies the art at draw time
// (CydDisplay::setSpriteScale) without touching the art or its call sites.
//
// A third of the text size puts Mario at ~50% of the digit height at 160x120
// and ~67% at 240x160. Upstream's cruder 10px figure was 42% of its digit row,
// so this is a little larger as well as far more detailed.
// Override per board env to taste.
#ifndef SPRITE_SCALE
#define SPRITE_SCALE (DIGIT_TEXT_SIZE / 3)
#endif

// Height of the character art, in sprite pixels. Mario is the tallest, and the
// character band is sized from him so his head reaches the digits. Kept here
// rather than included from mario_sprites.h so the layout does not depend on
// one style's art; clock_mario.cpp static_asserts that the two agree.
constexpr int SPRITE_ART_H = 16;

// Size-1 text metrics. Declared here because the vertical bands below are
// measured up from the text rows at the bottom of the canvas.
constexpr int TEXT1_W = 6;
constexpr int TEXT1_H = 8;

// ---- Vertical bands --------------------------------------------------------
// Built from the bottom up, because the two fixed quantities are at the bottom:
// the text rows need a known height, and the character band has to be exactly
// one sprite tall so a character's head reaches the underside of the digit row.
// Whatever is left over becomes the top band - sky, for the scenery. Sizing
// top-down instead would let a larger SPRITE_SCALE push the text off the canvas.
// +4 leaves a little air between a walking character's hat and the digits,
// so he only closes it when he jumps.
constexpr int CHAR_BAND = SPRITE_ART_H * SPRITE_SCALE + 4;

constexpr int DAY_Y = SCREEN_HEIGHT - 2 - TEXT1_H;
constexpr int DATE_Y = DAY_Y - 12;
constexpr int GROUND_Y = DATE_Y - 5;
constexpr int DIGIT_BOTTOM_Y = GROUND_Y - CHAR_BAND;
constexpr int TIME_Y_BASE = DIGIT_BOTTOM_Y - DIGIT_H;

// Sky above the digits, which is where the Mario scenery goes.
constexpr int SKY_TOP = 0;
constexpr int SKY_BOTTOM = TIME_Y_BASE;

// ---- Common helpers --------------------------------------------------------
constexpr int SCREEN_CENTER_X = SCREEN_WIDTH / 2;
constexpr int SCREEN_CENTER_Y = SCREEN_HEIGHT / 2;

// With a bottom-up stack, a larger SPRITE_SCALE eats the sky rather than
// pushing text off the canvas - until it runs out and the digit row would go
// negative. Fail the build there rather than drawing off the top edge.
static_assert(TIME_Y_BASE >= 0,
              "SPRITE_SCALE is too large for this canvas - the digit row would "
              "start above the top edge. Lower it in the board environment.");

// Centre a size-1 string of `len` characters.
constexpr int centerText1(int len) { return (SCREEN_WIDTH - len * TEXT1_W) / 2; }

// Centre a string at an arbitrary text size.
constexpr int centerTextAt(int len, int size) {
  return (SCREEN_WIDTH - len * TEXT1_W * size) / 2;
}

// Date strings are 10 characters ("DD/MM/YYYY") at size 1.
constexpr int DATE_X = (SCREEN_WIDTH - 10 * TEXT1_W) / 2;

// ---- Play areas ------------------------------------------------------------
// Styles that hand the whole canvas to a game (Pong's ball, Tetris' well,
// Asteroids' rocks) bound themselves with these rather than raw literals.
constexpr int PLAY_TOP = 0;
constexpr int PLAY_BOTTOM = SCREEN_HEIGHT - 1;
constexpr int PLAY_LEFT = 0;
constexpr int PLAY_RIGHT = SCREEN_WIDTH - 1;

// The digit block as a rectangle, for collision and masking.
constexpr int DIGITS_LEFT = TIME_X;
constexpr int DIGITS_RIGHT = TIME_X + TIME_STR_W;
constexpr int DIGITS_TOP = TIME_Y_BASE;
constexpr int DIGITS_BOTTOM = DIGIT_BOTTOM_Y;
