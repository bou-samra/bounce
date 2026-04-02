/* ------------------------------------------------------------------------------
   -
   - main.c – Realistic Physics with Smooth Rolling
   - to compile: 
   - gcc -Wall -O2 main.c -o ball_ramp `pkg-config --cflags --libs sdl2` -lm 
   -----------------------------------------------------------------------------*/

#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>

/* ---------------------- Configuration (units explained) ----------------------
   - Positions/lengths: pixels (px)
   - Time: seconds (s)
   - Linear velocity: px/s
   - Acceleration: px/s^2
   - Angle: radians (rad)
   - Angular velocity (spin): rad/s
---------------------------------------------------------------------------- */
#define SCREEN_W 800
#define SCREEN_H 600

/* If you want "real-world" gravity, pick PIXELS_PER_METER (px/m).
   Example: PIXELS_PER_METER = 100 -> 100 px = 1 m
   Then GRAVITY = 9.81 * PIXELS_PER_METER  (px/s^2).
   For this demo we use a tuned value for pleasing gameplay, not exact Earth g.
*/
#define PIXELS_PER_METER 100.0f
#define GRAVITY_METERS 9.81f
#define GRAVITY      (GRAVITY_METERS * PIXELS_PER_METER)  /* px/s^2 (example: 981 px/s^2 at 100 px/m) */

/* The demo originally used 2200 px/s^2 for a snappier feel; you can set:
   #define GRAVITY 2200.0f
*/

#define BALL_RADIUS  12.0f     /* px */
#define RESTITUTION  0.70f     /* unitless */
#define GRIP         0.20f     /* unitless friction/traction factor */
#define ROLL_DRAG    0.9999f   /* multiplicative damping (unitless per application) */
#define SUBSTEPS     8         /* integer: physics substeps per frame */

/* ----------------------------- Basic types -------------------------------- */
typedef struct { float x, y; } Vec2;

typedef struct {
    Vec2 p1, p2;   /* endpoints in px */
    Vec2 normal;   /* outward unit normal (dimensionless) */
} Ramp;

/* ------------------------- Function prototypes ----------------------------- */
/* Handle collisions with the ramp segment; modifies pos, vel, spin */
void handle_ramp_collision(const Ramp *ramp, Vec2 *pos, Vec2 *vel, float *spin);

/* Handle collision with a flat floor at given y (px); modifies pos, vel, spin */
void handle_floor_collision(float floor_y, Vec2 *pos, Vec2 *vel, float *spin);

/* Utility: clamp a float to a maximum value (used to clamp dt) */
static inline float clamp_max(float v, float max_v) { return v > max_v ? max_v : v; }

/* ------------------------------ Main ------------------------------------- */
int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("Realistic Rolling Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    /* Ramp endpoints (px) */
    Ramp ramp = { {50, 150}, {600, 500} };

    /* Precompute ramp normal (unit vector). dx,dy in px; divide by len (px) -> dimensionless normal. */
    float dx = ramp.p2.x - ramp.p1.x;
    float dy = ramp.p2.y - ramp.p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    if (len == 0.0f) len = 1.0f;
    ramp.normal = (Vec2){ dy/len, -dx/len }; /* clockwise/right-hand perpendicular (unitless) */

    Vec2 ballPos = { 80, 50 };    /* px */
    Vec2 ballVel = { 120, 0 };    /* px/s */
    float ballSpin = 0.0f;        /* rad/s */
    float ballAngle = 0.0f;       /* rad */

    Uint32 lastTicks = SDL_GetTicks();

    /* ---------------- Numerical stability notes (why choices made)
       - Semi-implicit Euler: velocity updated before position improves stability for gravity
         (pos += vel*dt using the updated vel).
       - Substepping (SUBSTEPS): reduces tunneling and improves collision stability when dt is larger.
       - Clamping dt (max 20 ms here) prevents huge jumps after pauses/slow frames.
       - ROLL_DRAG here is multiplicative per floor application; for physically correct continuous
         damping, convert a time constant to exp(-k*dt) per substep instead.
    --------------------------------------------------------------------------- */

    while (true) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) return 0; }

        /* Frame dt in seconds; clamp to avoid large steps */
        float dt = (SDL_GetTicks() - lastTicks) / 1000.0f;
        lastTicks = SDL_GetTicks();
        dt = clamp_max(dt, 0.02f); /* 20 ms cap */

        float sdt = dt / SUBSTEPS;
        for (int s = 0; s < SUBSTEPS; s++) {
            /* Integrate gravity (px/s^2 -> px/s) */
            ballVel.y += GRAVITY * sdt;

            /* Semi-implicit Euler: update position using new velocity (px += (px/s)*s) */
            ballPos.x += ballVel.x * sdt;
            ballPos.y += ballVel.y * sdt;

            /* Integrate angular orientation (rad += rad/s * s) */
            ballAngle += ballSpin * sdt;

            /* Collision handling */
            handle_ramp_collision(&ramp, &ballPos, &ballVel, &ballSpin);
            handle_floor_collision(ramp.p2.y, &ballPos, &ballVel, &ballSpin);

            /* Optional: numerical safety clamp to keep values finite (not required but defensive) */
            if (!isfinite(ballPos.x)) ballPos.x = 0;
            if (!isfinite(ballPos.y)) ballPos.y = 0;
            if (!isfinite(ballVel.x)) ballVel.x = 0;
            if (!isfinite(ballVel.y)) ballVel.y = 0;
            if (!isfinite(ballSpin)) ballSpin = 0;
        }

        /* ---------------- Rendering (unchanged) ---------------- */
        SDL_SetRenderDrawColor(ren, 15, 15, 20, 255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 50, 50, 60, 255);
        SDL_RenderDrawLineF(ren, 0, ramp.p2.y, SCREEN_W, ramp.p2.y);
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDrawLineF(ren, ramp.p1.x, ramp.p1.y, ramp.p2.x, ramp.p2.y);

        SDL_SetRenderDrawColor(ren, 255, 180, 0, 255);
        for (int h = -BALL_RADIUS; h < BALL_RADIUS; h++) {
            float span = sqrtf(BALL_RADIUS * BALL_RADIUS - h * h);
            SDL_RenderDrawLineF(ren, ballPos.x - span, ballPos.y + h, ballPos.x + span, ballPos.y + h);
        }
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderDrawLineF(ren, ballPos.x, ballPos.y,
            ballPos.x + cosf(ballAngle) * BALL_RADIUS,
            ballPos.y + sinf(ballAngle) * BALL_RADIUS);

        SDL_RenderPresent(ren);
    }

    return 0;
}

/* -------------------------- Collision functions --------------------------- */

/* Ramp collision:
   - Projects ball center onto segment p1->p2 to find closest point (contact).
   - If distance < radius, push ball out along normal and apply normal reflection (restitution).
   - Compute tangent and slip (px/s), then remove a fraction via GRIP from linear and angular motion.
   Units:
   - pos: px, vel: px/s, spin: rad/s, BALL_RADIUS: px
*/
void handle_ramp_collision(const Ramp *ramp, Vec2 *pos, Vec2 *vel, float *spin) {
    float r_dx = ramp->p2.x - ramp->p1.x;
    float r_dy = ramp->p2.y - ramp->p1.y;
    float r_len2 = r_dx*r_dx + r_dy*r_dy;
    if (r_len2 == 0.0f) return;

    float t = ((pos->x - ramp->p1.x) * r_dx + (pos->y - ramp->p1.y) * r_dy) / r_len2;

    if (t >= 0.0f && t <= 1.0f) {
        Vec2 contact = { ramp->p1.x + t * r_dx, ramp->p1.y + t * r_dy }; /* px */
        float dx = pos->x - contact.x;
        float dy = pos->y - contact.y;
        float d2 = dx*dx + dy*dy; /* px^2 */

        if (d2 < BALL_RADIUS * BALL_RADIUS) {
            /* Resolve penetration */
            pos->x = contact.x + ramp->normal.x * BALL_RADIUS;
            pos->y = contact.y + ramp->normal.y * BALL_RADIUS;

            /* Normal response: reflect incoming normal velocity component and apply restitution */
            float v_dot_n = vel->x * ramp->normal.x + vel->y * ramp->normal.y; /* px/s */
            if (v_dot_n < 0.0f) {
                vel->x = (vel->x - 2.0f * v_dot_n * ramp->normal.x) * RESTITUTION;
                vel->y = (vel->y - 2.0f * v_dot_n * ramp->normal.y) * RESTITUTION;
            }

            /* Tangential friction: match spin to surface. tangent is perpendicular to normal */
            Vec2 tangent = { -ramp->normal.y, ramp->normal.x }; /* unitless */
            float v_tan = vel->x * tangent.x + vel->y * tangent.y; /* px/s */
            float slip = v_tan + ((*spin) * BALL_RADIUS);         /* px/s */

            /* Apply grip: remove fraction of slip from linear and angular motion */
            vel->x -= tangent.x * slip * GRIP; /* px/s */
            vel->y -= tangent.y * slip * GRIP; /* px/s */
            *spin  -= (slip / BALL_RADIUS) * GRIP; /* rad/s */
        }
    }
}

/* Floor collision at floor_y (px):
   - If ball below floor, clamp to floor and apply friction, restitution, and roll drag.
   - slip uses horizontal velocity + spin*radius to match surface speed (px/s).
*/
void handle_floor_collision(float floor_y, Vec2 *pos, Vec2 *vel, float *spin) {
    if (pos->y > floor_y - BALL_RADIUS) {
        pos->y = floor_y - BALL_RADIUS;

        float slip = vel->x + ((*spin) * BALL_RADIUS); /* px/s */
        vel->x -= slip * GRIP;                          /* px/s */
        *spin  -= (slip / BALL_RADIUS) * GRIP;          /* rad/s */

        /* Vertical bounce: if impact significant, invert with restitution */
        if (fabsf(vel->y) > 100.0f) vel->y *= -RESTITUTION;
        else vel->y = 0.0f;

        /* Rolling drag (dimensionless multiplier applied here) */
        vel->x  *= ROLL_DRAG;
        *spin   *= ROLL_DRAG;
    }
}
