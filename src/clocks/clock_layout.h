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
 * Both boards render at DISPLAY_SCALE 2, so one logical pixel is the same
 * physical size on each. That splits the metrics in two:
 *
 *   Sprites (Mario, ghosts, invaders, the dino) are fixed pixel art. They keep
 *   their logical size, so they stay the same physical size on both panels and
 *   the 4.0" simply shows more room around them. Scaling them would mean
 *   redrawing every sprite.
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

// ---- Vertical bands --------------------------------------------------------
// One sprite tall. Fixed, for the reason in the header comment.
constexpr int CHAR_BAND = 14;

constexpr int TIME_Y_BASE = SCREEN_HEIGHT / 4;
constexpr int DIGIT_BOTTOM_Y = TIME_Y_BASE + DIGIT_H;
constexpr int GROUND_Y = DIGIT_BOTTOM_Y + CHAR_BAND;

// Text rows below the baseline. 8px and 14px are the size-1 glyph cell and a
// comfortable line pitch; both stay legible because they are scaled x2 anyway.
constexpr int DATE_Y = GROUND_Y + 8;
constexpr int DAY_Y = DATE_Y + 14;

// ---- Common helpers --------------------------------------------------------
constexpr int SCREEN_CENTER_X = SCREEN_WIDTH / 2;
constexpr int SCREEN_CENTER_Y = SCREEN_HEIGHT / 2;

// Size-1 text metrics, for centring status strings.
constexpr int TEXT1_W = 6;
constexpr int TEXT1_H = 8;

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
