/*
 * AnimatedPixelClock - Clock Animation Constants
 *
 * Named constants for clock animation values.
 * These values are tuned for specific animation behaviors.
 */

#ifndef CLOCK_CONSTANTS_H
#define CLOCK_CONSTANTS_H

#include "clock_layout.h"

// ========== Mario Clock Constants ==========
// Starting position (off-screen left, one sprite clear of the edge)
#define MARIO_START_X -15

// Walking speed (pixels per frame at MARIO_ANIM_SPEED)
#define MARIO_WALK_SPEED 2.0f

// Velocity after hitting a digit (bounce upward)
#define MARIO_BOUNCE_VELOCITY 2.0f

// Second trigger threshold for animation (56 prevents digit revert during transition)
#define MARIO_ANIMATION_TRIGGER_SECOND 56

// ========== Space Clock Constants ==========
// Laser offset from character top (where laser starts)
#define SPACE_LASER_OFFSET_Y 4

// Explosion frames before moving to next target (ticks; 16 x 16ms = ~250ms,
// same real time as the original 10 x 25ms)
#define SPACE_EXPLOSION_FRAMES 16

// ========== Digit Positioning ==========
// Derived from the canvas - see clock_layout.h.
constexpr int DIGIT_SPACING_PX = DIGIT_W;
constexpr int DIGIT_START_X = TIME_X;

// ========== Common Values ==========
// Movement threshold (considered "at target" when within this distance)
#define MOVEMENT_THRESHOLD 1.0f

// Walk direction proximity threshold (within 3 pixels = at target)
#define MARIO_TARGET_PROXIMITY 3

// Date display width calculation (for centering): 10 chars at size 1.
constexpr int DATE_DISPLAY_WIDTH = 10 * TEXT1_W;

// SCREEN_CENTER_X comes from clock_layout.h.

#endif // CLOCK_CONSTANTS_H
