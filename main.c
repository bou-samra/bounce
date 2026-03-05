/***************************************************
 main.c – Realistic Physics with Smooth Rolling
to compile: 
gcc -Wall -O2 main.c -o ball_ramp `pkg-config --cflags --libs sdl2` -lm 
****************************************************/
#include <SDL2/SDL.h>
#include <math.h>
#include <stdbool.h>

#define SCREEN_W 800
#define SCREEN_H 600
#define GRAVITY      2200.0f 
#define BALL_RADIUS  12.0f
#define RESTITUTION  0.70f   
#define GRIP         0.20f   /* Traction: higher = ball rolls instantly */
#define ROLL_DRAG    0.9999f /* 1.0 = perpetual motion, < 1.0 = slow stop */
#define SUBSTEPS     8

typedef struct { float x, y; } Vec2;

typedef struct {
    Vec2 p1, p2, normal;
} Ramp;

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *win = SDL_CreateWindow("Realistic Rolling Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H, 0);
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    Ramp ramp = { {50, 150}, {600, 500} };
    float dx = ramp.p2.x - ramp.p1.x, dy = ramp.p2.y - ramp.p1.y;
    float len = sqrtf(dx*dx + dy*dy);
    ramp.normal = (Vec2){ dy/len, -dx/len };

    Vec2 ballPos = { 80, 50 }; 
    Vec2 ballVel = { 120, 0 };
    float ballSpin = 0.0f; 
    float ballAngle = 0.0f;

    Uint32 lastTicks = SDL_GetTicks();
    while (true) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) { if (e.type == SDL_QUIT) return 0; }

        float dt = (SDL_GetTicks() - lastTicks) / 1000.0f;
        lastTicks = SDL_GetTicks();
        if (dt > 0.02f) dt = 0.02f;

        float sdt = dt / SUBSTEPS;
        for (int s = 0; s < SUBSTEPS; s++) {
            ballVel.y += GRAVITY * sdt;
            ballPos.x += ballVel.x * sdt;
            ballPos.y += ballVel.y * sdt;
            ballAngle += ballSpin * sdt;

            // --- 1. Ramp Collision ---
            float r_dx = ramp.p2.x - ramp.p1.x, r_dy = ramp.p2.y - ramp.p1.y;
            float r_len2 = r_dx*r_dx + r_dy*r_dy;
            float t = ((ballPos.x - ramp.p1.x) * r_dx + (ballPos.y - ramp.p1.y) * r_dy) / r_len2;
            
            if (t >= 0 && t <= 1) {
                Vec2 contact = { ramp.p1.x + t*r_dx, ramp.p1.y + t*r_dy };
                float d2 = powf(ballPos.x - contact.x, 2) + powf(ballPos.y - contact.y, 2);
                
                if (d2 < BALL_RADIUS * BALL_RADIUS) {
                    ballPos.x = contact.x + ramp.normal.x * BALL_RADIUS;
                    ballPos.y = contact.y + ramp.normal.y * BALL_RADIUS;

                    float v_dot_n = ballVel.x * ramp.normal.x + ballVel.y * ramp.normal.y;
                    if (v_dot_n < 0) {
                        ballVel.x = (ballVel.x - 2 * v_dot_n * ramp.normal.x) * RESTITUTION;
                        ballVel.y = (ballVel.y - 2 * v_dot_n * ramp.normal.y) * RESTITUTION;
                    }

                    // Friction: Match spin to surface velocity
                    Vec2 tangent = { -ramp.normal.y, ramp.normal.x };
                    float v_tan = ballVel.x * tangent.x + ballVel.y * tangent.y;
                    float slip = v_tan + (ballSpin * BALL_RADIUS);
                    ballVel.x -= tangent.x * slip * GRIP;
                    ballVel.y -= tangent.y * slip * GRIP;
                    ballSpin -= (slip / BALL_RADIUS) * GRIP;
                }
            }

            // --- 2. Floor Collision ---
            if (ballPos.y > ramp.p2.y - BALL_RADIUS) {
                ballPos.y = ramp.p2.y - BALL_RADIUS;

                // Match spin with horizontal floor speed
                float slip = ballVel.x + (ballSpin * BALL_RADIUS);
                ballVel.x -= slip * GRIP;
                ballSpin -= (slip / BALL_RADIUS) * GRIP;

                if (fabsf(ballVel.y) > 100) ballVel.y *= -RESTITUTION;
                else ballVel.y = 0;

                // Constant Roll Drag (Newtonian damping)
                ballVel.x *= ROLL_DRAG;
                ballSpin *= ROLL_DRAG;
            }
        }

        // --- Render ---
        SDL_SetRenderDrawColor(ren, 15, 15, 20, 255);
        SDL_RenderClear(ren);

        SDL_SetRenderDrawColor(ren, 50, 50, 60, 255);
        SDL_RenderDrawLineF(ren, 0, ramp.p2.y, SCREEN_W, ramp.p2.y);
        SDL_SetRenderDrawColor(ren, 200, 200, 200, 255);
        SDL_RenderDrawLineF(ren, ramp.p1.x, ramp.p1.y, ramp.p2.x, ramp.p2.y);

        // Draw Ball
        SDL_SetRenderDrawColor(ren, 255, 180, 0, 255);
        for (int h = -BALL_RADIUS; h < BALL_RADIUS; h++) {
            float span = sqrtf(BALL_RADIUS * BALL_RADIUS - h * h);
            SDL_RenderDrawLineF(ren, ballPos.x - span, ballPos.y + h, ballPos.x + span, ballPos.y + h);
        }
        // Rotation Spoke
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderDrawLineF(ren, ballPos.x, ballPos.y, 
            ballPos.x + cosf(ballAngle) * BALL_RADIUS, 
            ballPos.y + sinf(ballAngle) * BALL_RADIUS);

        SDL_RenderPresent(ren);
    }
    return 0;
}
