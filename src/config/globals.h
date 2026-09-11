#pragma once

/*
 * CYD_AnimatedPixelClock - Shared types and globals
 *
 * Structs, enums and extern declarations shared across modules. User-tuneable
 * constants live in include/config.h, not here.
 *
 * Ported from AnimatedPixelClock by Keralots. The PC-metrics, audio-visualizer
 * and ambient-screensaver types that lived here are in archive/upstream-src/.
 */

#include <Arduino.h>

#include "config.h"        // include/config.h - user-tuneable constants
#include "../clocks/clock_layout.h"  // canvas-derived layout metrics
#include "color_slots.h"

// ========== Settings Structure ========== 
struct Settings {
  char cycleConfig[128];       // Ordered style:seconds pairs; 0 seconds disables
  // Clock settings
  uint8_t clockStyle;       // 0=Mario, 1=Standard, 2=Large, 3=Space Invader, 4=Space Ship, 5=Pong, 6=Pac-Man, 7=Snake, 8=Tetris, 9=Cycle All, 10=Asteroids, 11=Dino Runner, 12=Matrix Rain, 14=Weather, 15=Bomberman, 16=TRON (13 retired: Missile Command)
  int16_t gmtOffset;        // GMT offset in minutes (deprecated, kept for migration)
  bool daylightSaving;      // DST enabled (deprecated, kept for migration)
  char timezoneString[64];  // POSIX TZ string (e.g., "CET-1CEST,M3.5.0/02:00,M10.5.0/03:00")
  uint8_t timezoneIndex;    // Index into timezone database (for UI display)
  bool use24Hour;           // 24-hour format
  uint8_t dateFormat;       // 0=DD/MM/YYYY, 1=MM/DD/YYYY, 2=YYYY-MM-DD, 3=DD.MM.YYYY
  uint8_t clockPosition;    // 0=Center, 1=Left, 2=Right
  int8_t clockOffset;       // Fine-tune clock position (-10 to +10)
  // Display settings
  uint8_t colonBlinkMode;   // 0=Always On, 1=Blink, 2=Always Off
  uint8_t colonBlinkRate;   // Tenths of Hz (10 = 1.0 Hz)
  uint8_t displayBrightness;   // Display brightness 0-255 (default: 255)

  // Scheduled dimming (night mode). Start/end are hour:minute of day.
  bool enableScheduledDimming;  // Enable time-based automatic dimming
  uint8_t dimStartHour;         // Hour to start dimming (0-23)
  uint8_t dimStartMinute;       // Minute to start dimming (0-59)
  uint8_t dimEndHour;           // Hour to end dimming (0-23)
  uint8_t dimEndMinute;         // Minute to end dimming (0-59)
  uint8_t dimBrightness;        // Brightness level during dim period (0-255)

  // Scheduled power-off (nightly full-dark window; spares the LEDs). Takes
  // priority over dimming. Home Assistant can also toggle live via
  // /api/display/on|off (runtime override, separate from this schedule).
  bool enableScheduledOff;      // Enable the nightly off window
  uint8_t offStartHour;         // Hour to power the panel off (0-23)
  uint8_t offStartMinute;       // Minute to power the panel off (0-59)
  uint8_t offEndHour;           // Hour to power it back on (0-23)
  uint8_t offEndMinute;         // Minute to power it back on (0-59)

  // Notification banner (POST /api/notify)
  bool notifyEnabled;           // Master toggle for the /api/notify endpoint
  uint8_t notifyPosition;       // Default banner position: 0=Bottom, 1=Top

  // Weather clock (style 14, Open-Meteo)
  bool weatherEnabled;          // Master toggle for the fetch task
  float weatherLat;             // Location latitude (0,0 = unset)
  float weatherLon;             // Location longitude
  bool weatherUseFahrenheit;    // false = Celsius
  char weatherApiKey[33];       // Optional commercial API key ("" = free endpoint)
  char weatherPlace[48];        // Place name from the web UI search ("" = coordinates typed by hand)

  // Network settings
  char deviceName[32];          // Device name for mDNS and app (default: "pixelclock")
  bool showIPAtBoot;          // Show IP address on the panel at startup (default: true)
  bool useStaticIP;
  char staticIP[16];
  char gateway[16];
  char subnet[16];
  char dns1[16];
  char dns2[16];
  char ntpServer1[64];        // Primary NTP server (blank = compiled default)
  char ntpServer2[64];        // Secondary NTP server (blank = none)

  // TRON clock settings
  uint8_t tronBikeStyle;       // 0=Motorcycle profile, 1=Light cycle top view

  // Mario clock settings
  uint8_t marioBounceHeight;  // Tenths (40 = 4.0)
  uint8_t marioBounceSpeed;   // Tenths (6 = 0.6)
  bool marioSmoothAnimation;  // Enable 4-frame walk cycle (default: false = 2-frame)
  uint8_t marioWalkSpeed;     // Tenths (20 = 2.0, 25 = 2.5 old/fast)
  bool marioIdleEncounters;   // Enable idle enemy encounters (default: false)
  uint8_t marioEncounterFreq; // 0=Rare(25-35s), 1=Normal(15-25s), 2=Frequent(8-15s)
  uint8_t marioEncounterSpeed; // 0=Slow, 1=Normal, 2=Fast (default: 1)
  bool marioScenery;          // Classic World 1-1 backdrop in the sky band

  // Space clock settings
  uint8_t spaceCharacterType;   // 0=Invader, 1=Ship
  uint8_t spacePatrolSpeed;     // Tenths (10 = 1.0)
  uint8_t spaceAttackSpeed;     // Tenths (25 = 2.5)
  uint8_t spaceLaserSpeed;      // Tenths (40 = 4.0)
  uint8_t spaceExplosionGravity; // Tenths (5 = 0.5)

  // Pong clock settings
  uint8_t pongBallSpeed;        // Fixed-point (16 = 1.0)
  uint8_t pongBounceStrength;   // Tenths (3 = 0.3)
  uint8_t pongBounceDamping;    // Hundredths (85 = 0.85)
  uint8_t pongPaddleWidth;      // Pixels (20)
  bool pongHorizontalBounce;    // Enable horizontal digit bounce on side hits
  bool pongDigitShatter;        // Shatter/reassemble on digit change (off = blink only)

  // Pac-Man clock settings
  uint8_t pacmanSpeed;          // Tenths (10 = 1.0)
  uint8_t pacmanEatingSpeed;    // Tenths (20 = 2.0)
  uint8_t pacmanMouthSpeed;     // Mouth animation speed (10 = 100ms)
  uint8_t pacmanPelletCount;    // Number of patrol pellets (0-20)
  bool pacmanPelletRandomSpacing;  // Random or even spacing
  bool pacmanBounceEnabled;     // Enable digit bounce on eat

  // Snake clock settings
  uint8_t snakeSpeed;           // Step pace, tenths (higher = faster)
  uint8_t snakeLength;          // Base body length in cells (4-12)
  bool snakeWallBorder;         // Draw Nokia-style arena frame
  bool snakeShowDate;           // Show date row (off = snake uses full screen)

  // Tetris clock settings
  uint8_t tetrisFallSpeed;      // Slab/dot drop accel, tenths (12 = 1.2)
  uint8_t tetrisBlockStyle;     // 0=LCD grid (gaps), 1=Solid blocks
  bool tetrisIdleTumble;        // Show occasional tumbling piece when idle
  uint8_t tetrisAnimStyle;      // 0=Drop-in slabs, 1=Falling dots build-up
  bool tetrisShowDate;          // Show the date row (off = cleaner screen)
  uint8_t tetrisDatePosition;   // 0=Top, 1=Bottom
  uint8_t tetrisDotSpeed;       // Falling-dot build speed, tenths (12 = 1.2)
  uint8_t tetrisDotOrder;       // 0=Bottom-up, 1=Random
  bool tetrisDigitBounce;       // Bounce the new digit after it rebuilds
  bool tetrisSmoothGame;        // Block Game plays near-perfectly (smart piece pick, avoids holes)
  bool tetrisSmallClock;        // Small corner clock; frees the panel for a taller block-game well (auto-enables Block game)
  uint8_t tetrisSmallClockPos;  // 0=Top-left, 1=Top-right

  // Asteroids clock settings
  uint8_t asteroidsShipSpeed;   // Ship thrust/drift scale, tenths (12 = 1.2)
  uint8_t asteroidsRockCount;   // Rocks kept in play (1-4)
  uint8_t asteroidsRockSpeed;   // Rock drift speed, tenths (8 = 0.8)
  bool asteroidsShowDate;       // Show date row (off = centred clock)
  bool asteroidsTransparent;    // No mask behind digits, ship flies through (default: true)

  // Dino Runner clock settings
  uint8_t dinoSpeed;            // World scroll speed, tenths (12 = 1.2)
  uint8_t dinoCactusFreq;       // 0=Rare, 1=Normal, 2=Frequent
  bool dinoShowClouds;          // Parallax clouds (default: true)
  bool dinoShowDate;            // Show date row (off = centred clock)

  // Matrix Rain clock settings
  uint8_t matrixRainSpeed;      // Rain fall speed, tenths (12 = 1.2)
  uint8_t matrixRainDensity;    // 0=Sparse, 1=Normal, 2=Dense
  bool matrixShowDate;          // Show date row (off = centred clock)
  bool matrixTransparent;       // No mask behind digits, rain falls through (default: false)

  // Missile Command clock settings (style retired; kept inert for NVS/export stability)
  uint8_t mcMissileSpeed;       // Enemy missile speed, tenths (12 = 1.2)
  uint8_t mcMissileFreq;        // Idle volley cadence: 0=Rare, 1=Normal, 2=Frequent
  bool mcShowDate;              // Show date row (off = centred clock)

  // ---- CYD hardware (no upstream equivalent) ----
  bool touchEnabled;            // Tap the screen to advance the clock style
  bool ldrAutoBrightness;       // Drive the backlight from the onboard LDR
  uint8_t ldrMinBrightness;     // Floor for auto-brightness (0-255)
  bool rgbLedEnabled;           // Mirror WiFi / notification state on the RGB LED

  // User-editable sprite colors (RGB565), indexed by ColorSlot.
  uint16_t spriteColors[COL_COUNT];
};

// ========== Mario Clock Types ==========
enum MarioState {
  MARIO_IDLE,
  MARIO_WALKING,
  MARIO_JUMPING,
  MARIO_WALKING_OFF,
  // Idle encounter states
  MARIO_ENCOUNTER_WALKING,
  MARIO_ENCOUNTER_JUMPING,
  MARIO_ENCOUNTER_SHOOTING,
  MARIO_ENCOUNTER_SQUASH,
  MARIO_ENCOUNTER_RETURNING
};

// Enemy types for idle encounters
enum EnemyType { ENEMY_NONE, ENEMY_GOOMBA, ENEMY_SPINY, ENEMY_KOOPA };
enum EnemyState { ENEMY_WALKING, ENEMY_SQUASHING, ENEMY_HIT, ENEMY_DEAD, ENEMY_SHELL_SLIDING };

struct MarioEnemy {
  EnemyType type;
  EnemyState state;
  float x;
  int walkFrame;
  uint8_t animTimer;
  bool fromRight;
};

struct MarioFireball {
  float x, y;
  float vy;
  bool active;
};

// Mario animation constants
#define MARIO_ANIM_SPEED 16
#define ENCOUNTER_ANIM_SPEED 16  // ~60fps for smooth encounter animations
// Motion constants were tuned for the original 35 ms walk/jump tick. Both the
// encounter states (which always ran at 16 ms) and, since the tick was unified
// at 16 ms, the walk/jump states scale their per-tick motion by this factor so
// all real-time speeds stay as tuned.
#define MARIO_TICK_SCALE (16.0f / 35.0f)
#define ENCOUNTER_TIME_SCALE MARIO_TICK_SCALE
#define JUMP_POWER -4.5
#define GRAVITY 0.6
// Layout metrics now derive from the canvas - see clocks/clock_layout.h.
constexpr int TIME_Y = TIME_Y_BASE;
// How far below Mario's origin his head sits; he bounces a digit by putting it
// against the underside of the digit row. Scales with the magnified art.
constexpr int MARIO_HEAD_OFFSET = SPRITE_ART_H * SPRITE_SCALE;
constexpr int DIGIT_BOTTOM = DIGIT_BOTTOM_Y;

// Digit X positions for time display
extern const int DIGIT_X[5];

// ========== Space Clock Types ==========
enum SpaceState {
  SPACE_PATROL,
  SPACE_SLIDING,
  SPACE_SHOOTING,
  SPACE_EXPLODING_DIGIT,
  SPACE_MOVING_NEXT,
  SPACE_RETURNING
};

struct Laser {
  float x, y;
  float length;
  bool active;
  int target_digit_idx;
};

#define MAX_SPACE_FRAGMENTS 20
#define LASER_MAX_LENGTH 50
constexpr int SPACE_PATROL_LEFT = 20;
constexpr int SPACE_PATROL_RIGHT = SCREEN_WIDTH - 20;

struct SpaceFragment {
  float x, y;
  float vx, vy;
  bool active;
};

// ========== Pong Clock Types ==========
enum PongBallState {
  PONG_BALL_NORMAL,
  PONG_BALL_SPAWNING
};

enum DigitTransitionState {
  DIGIT_NORMAL,
  DIGIT_BREAKING,
  DIGIT_ASSEMBLING
};

struct PongBall {
  int x, y;       // Fixed-point (multiply by 16)
  int vx, vy;     // Velocity (fixed-point)
  PongBallState state;
  unsigned long spawn_timer;
  bool active;
  int inside_digit;  // -1 = none, 0-4 = digit index
};

struct DigitTransition {
  DigitTransitionState state;
  char old_char;
  char new_char;
  unsigned long state_timer;
  int hit_count;
  int fragments_spawned;
  float assembly_progress;
};

struct BreakoutPaddle {
  int x;           // Center X position
  int target_x;    // Target X for auto-tracking
  int width;       // Paddle width in pixels
  int speed;       // Movement speed
};

struct FragmentTarget {
  int target_digit;
  int target_x;
  int target_y;
};

// Pong constants
#define MAX_PONG_BALLS 2
#define MAX_PONG_FRAGMENTS 40
#define PONG_BALL_SIZE 2
constexpr int PONG_TIME_Y = TIME_Y_BASE;
// Ball may travel above the digits, into the date band.
constexpr int PONG_PLAY_AREA_TOP = 10;
// Paddle rides just above the bottom edge.
constexpr int BREAKOUT_PADDLE_Y = SCREEN_HEIGHT - 4;
#define BREAKOUT_PADDLE_HEIGHT 2
#define PONG_UPDATE_INTERVAL 16        // ms; matches the 60 Hz render frame (constants
                                       // below are per-tick, rescaled from the 20 ms tuning)
#define BALL_SPAWN_DELAY 500
#define PONG_FRAG_GRAVITY 0.19         // 0.3 * 0.64 (accel scales with tick^2)
#define PONG_FRAG_SPEED 1.2            // 1.5 * 0.8 (velocity scales with tick)
#define BALL_HIT_THRESHOLD 3
#define DIGIT_TRANSITION_TIMEOUT 3000
#define DIGIT_ASSEMBLY_DURATION 800
#define PONG_BALL_SPEED_BOOST 22       // 28 * 0.8
#define MULTIBALL_ACTIVATE_SECOND 55
#define PADDLE_WRONG_DIRECTION_CHANCE 0    // 0 = disabled (smooth tracking), 10-20 = adds chaos
#define PADDLE_STICK_MIN_DELAY 0
#define PADDLE_STICK_MAX_DELAY 300
#define PADDLE_MOMENTUM_MULTIPLIER 2
#define BALL_RELEASE_RANDOM_VARIATION 2
#define BALL_COLLISION_ANGLE_VARIATION 3

extern const float FRAGMENT_SPAWN_PERCENT[3];

// ========== Pac-Man Clock Types ==========
enum PacmanState {
  PACMAN_PATROL,
  PACMAN_TARGETING,
  PACMAN_EATING,
  PACMAN_RETURNING
};

struct PatrolPellet {
  int x;
  bool active;
};

struct PathStep {
  uint8_t col;  // Column in grid (0-4)
  uint8_t row;  // Row in grid (0-6)
};

// Pac-Man constants
#define PACMAN_ANIM_SPEED 16  // ms; matches the 60 Hz render frame (speeds are
                              // divided by 18.75 instead of 20 to compensate)
constexpr int PACMAN_PATROL_Y = GROUND_Y;
#define MAX_PATROL_PELLETS 20
constexpr int TIME_Y_PACMAN = TIME_Y_BASE;
// Pac-Man draws its digits as a 5x7 pellet grid rather than as text, so the
// pellet pitch - not the font - sets the digit size. Upstream used a 5px pitch
// against a size-3 glyph; this keeps that same ratio at whatever size the
// board's digits are.
constexpr int PELLET_SPACING = DIGIT_TEXT_SIZE + 2;
constexpr int PELLET_SIZE = DIGIT_TEXT_SIZE / 2;
#define DIGIT_GRID_W 5
#define DIGIT_GRID_H 7

// ========== Extern Global Declarations ==========
// These are defined in main.cpp

// Settings and state
extern Settings settings;

extern bool displayAvailable;
extern bool ntpSynced;
extern unsigned long lastNtpSyncTime;
extern unsigned long lastReceived;
extern unsigned long wifiDisconnectTime;
extern unsigned long nextDisplayUpdate;
extern bool wifiConnected;  // WiFi connection status for icon display

// Mario clock globals
extern MarioState mario_state;
extern float mario_x;
extern float mario_jump_y;
extern float jump_velocity;
extern int mario_base_y;
extern bool mario_facing_right;
extern int mario_walk_frame;
extern unsigned long last_mario_update;
extern int displayed_hour;
extern int displayed_min;
extern bool displayed_is_pm;
extern bool time_overridden;
extern int last_minute;
extern bool animation_triggered;
extern bool digit_bounce_triggered;
extern int num_targets;
extern int target_x_positions[4];
extern int target_digit_index[4];
extern int target_digit_values[4];
extern int current_target_index;
extern float digit_offset_y[5];
extern float digit_velocity[5];
extern float digit_offset_x[5];
extern float digit_velocity_x[5];

// Mario idle encounter globals
extern MarioEnemy currentEnemy;
extern MarioFireball marioFireball;
extern unsigned long lastEncounterEnd;
extern unsigned long nextEncounterDelay;

// Space clock globals
extern SpaceState space_state;
extern float space_x;
extern const float space_y;
extern int space_anim_frame;
extern int space_patrol_direction;
extern unsigned long last_space_update;
extern unsigned long last_space_sprite_toggle;
extern Laser space_laser;
extern SpaceFragment space_fragments[MAX_SPACE_FRAGMENTS];
extern int space_explosion_timer;

// Pong clock globals
extern PongBall pong_balls[MAX_PONG_BALLS];
extern SpaceFragment pong_fragments[MAX_PONG_FRAGMENTS];
extern FragmentTarget fragment_targets[MAX_PONG_FRAGMENTS];
extern DigitTransition digit_transitions[5];
extern BreakoutPaddle breakout_paddle;
extern unsigned long last_pong_update;
extern bool ball_stuck_to_paddle[MAX_PONG_BALLS];
extern unsigned long ball_stick_release_time[MAX_PONG_BALLS];
extern int ball_stuck_x_offset[MAX_PONG_BALLS];
extern int paddle_last_x;

// Pac-Man clock globals
extern PacmanState pacman_state;
extern float pacman_x;
extern float pacman_y;
extern int pacman_direction;
extern int pacman_mouth_frame;
extern unsigned long last_pacman_update;
extern unsigned long last_pacman_mouth_toggle;
extern int last_minute_pacman;
extern bool pacman_animation_triggered;
extern bool digit_being_eaten[5];
extern int digit_eaten_rows_left[5];
extern int digit_eaten_rows_right[5];
extern PatrolPellet patrol_pellets[MAX_PATROL_PELLETS];
extern int num_pellets;
extern uint8_t digitEatenPellets[5][5];
extern uint8_t current_eating_digit_index;
extern uint8_t current_eating_digit_value;
extern uint8_t current_path_step;
extern float pellet_eat_distance;
extern uint8_t target_digit_queue[4];
extern uint8_t target_digit_new_values[4];
extern uint8_t target_queue_length;
extern uint8_t target_queue_index;
extern uint8_t pending_digit_index;
extern uint8_t pending_digit_value;

