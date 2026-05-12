#include <cmath>
#include <cstdlib>
#include <cstring>
#include <emscripten.h>

// ---------- Constants ----------
#define MAP_SIZE 300
#define GRID_RES 2.0f
#define GRID_DIM (int)(MAP_SIZE / GRID_RES)
#define MAX_PLAYERS 16
#define MAX_BOTS 10
#define PI 3.14159265359f

// ---------- Enums & Structs ----------
enum WeaponType { WEAPON_PICKAXE, WEAPON_AR, WEAPON_SHOTGUN };
enum BuildPiece { BUILD_WALL, BUILD_FLOOR, BUILD_RAMP };

struct Player {
    float x, y, z;        // position
    float vx, vy, vz;     // velocity
    float yaw, pitch;     // view angles
    float health;
    float shield;
    int weapon;
    int ammo;
    int wood, stone, metal;
    bool alive;
    bool onGround;
    bool isBot;
    float botTimer;
};

struct BotState {
    int target;
    float shootCooldown;
    float buildCooldown;
    float wanderAngle;
};

struct GameState {
    Player players[MAX_PLAYERS];
    int numPlayers;
    int grid[GRID_DIM][GRID_DIM];  // 0=empty, 1=wall, 2=floor, 3=ramp
    float stormCenterX, stormCenterZ;
    float stormRadius;
    float stormTargetRadius;
    float stormShrinkTimer;
    int chestPositions[20][2];
    bool chestOpened[20];
    int killFeed[5];
    int killFeedCount;
};

// ---------- Globals ----------
GameState g;
BotState bots[MAX_BOTS];
float g_deltaTime = 0.0f;

// Input state (set from JS)
struct Input {
    bool forward, backward, left, right, jump;
    bool shoot, build, switchWeapon;
    bool buildMode;
    int buildPiece; // 0=wall, 1=floor, 2=ramp
    float mouseDx, mouseDy;
};
Input g_input;

// ---------- Helper Functions ----------
static int gridIdx(float x, float z) {
    int ix = (int)((x + MAP_SIZE/2) / GRID_RES);
    int iz = (int)((z + MAP_SIZE/2) / GRID_RES);
    if (ix < 0) ix = 0; if (ix >= GRID_DIM) ix = GRID_DIM-1;
    if (iz < 0) iz = 0; if (iz >= GRID_DIM) iz = GRID_DIM-1;
    return iz * GRID_DIM + ix;
}

static bool isBlocked(float x, float z) {
    int idx = gridIdx(x, z);
    int iz = idx / GRID_DIM;
    int ix = idx % GRID_DIM;
    return g.grid[iz][ix] == 1 || g.grid[iz][ix] == 3; // solid walls/ramps
}

static bool isSolid(int tile) { return tile == 1 || tile == 3; }

// ---------- Game Init ----------
extern "C" EMSCRIPTEN_KEEPALIVE
void init() {
    memset(&g, 0, sizeof(g));
    g.stormCenterX = 0;
    g.stormCenterZ = 0;
    g.stormRadius = 200.0f;
    g.stormTargetRadius = 10.0f;
    g.stormShrinkTimer = 30.0f;
    g.numPlayers = 1 + MAX_BOTS;

    // Place chests
    for (int i = 0; i < 20; i++) {
        g.chestPositions[i][0] = (rand() % MAP_SIZE) - MAP_SIZE/2;
        g.chestPositions[i][1] = (rand() % MAP_SIZE) - MAP_SIZE/2;
        g.chestOpened[i] = false;
    }

    // Initialize player
    Player* p = &g.players[0];
    p->x = (rand() % 100) - 50;
    p->z = (rand() % 100) - 50;
    p->y = 200; // start from bus
    p->health = 100;
    p->shield = 100;
    p->weapon = WEAPON_PICKAXE;
    p->ammo = 30;
    p->wood = 100; p->stone = 50; p->metal = 20;
    p->alive = true;
    p->onGround = false;

    // Initialize bots
    for (int i = 1; i < g.numPlayers; i++) {
        Player* b = &g.players[i];
        b->x = (rand() % 200) - 100;
        b->z = (rand() % 200) - 100;
        b->y = 200;
        b->health = 100;
        b->shield = 50 + rand()%50;
        b->weapon = WEAPON_AR;
        b->ammo = 30 + rand()%30;
        b->alive = true;
        b->onGround = false;
        b->isBot = true;
        BotState* bs = &bots[i-1];
        bs->target = 0;
        bs->shootCooldown = 0.5f + (rand()%100)/50.0f;
        bs->buildCooldown = 2.0f + rand()%3;
    }

    // Place some pre-built structures
    for (int i = 0; i < 15; i++) {
        int cx = rand() % (GRID_DIM-10) + 5;
        int cz = rand() % (GRID_DIM-10) + 5;
        for (int dx = 0; dx < 4; dx++)
            for (int dz = 0; dz < 4; dz++)
                g.grid[cz+dz][cx+dx] = 1; // walls
    }

    g.killFeedCount = 0;
    memset(g.killFeed, 0, sizeof(g.killFeed));
}

// ---------- Player Movement & Physics ----------
static void movePlayer(Player* p, float dt) {
    if (!p->alive) return;
    float speed = 12.0f;
    float moveX = 0, moveZ = 0;
    if (g_input.forward) { moveX += sin(p->yaw); moveZ += cos(p->yaw); }
    if (g_input.backward) { moveX -= sin(p->yaw); moveZ -= cos(p->yaw); }
    if (g_input.left) { moveX -= cos(p->yaw); moveZ += sin(p->yaw); }
    if (g_input.right) { moveX += cos(p->yaw); moveZ -= sin(p->yaw); }

    float len = sqrt(moveX*moveX + moveZ*moveZ);
    if (len > 0) { moveX /= len; moveZ /= len; }

    // Apply movement
    p->vx = moveX * speed;
    p->vz = moveZ * speed;

    // Gravity
    if (!p->onGround) p->vy -= 30.0f * dt;
    if (g_input.jump && p->onGround) {
        p->vy = 14.0f;
        p->onGround = false;
    }

    // Integrate
    p->x += p->vx * dt;
    p->y += p->vy * dt;
    p->z += p->vz * dt;

    // Collision with ground (height = 0)
    if (p->y <= 0.0f) {
        p->y = 0.0f;
        p->vy = 0;
        p->onGround = true;
    }

    // Collision with grid structures (simple AABB)
    int gi = gridIdx(p->x, p->z);
    int iz = gi / GRID_DIM;
    int ix = gi % GRID_DIM;
    if (isSolid(g.grid[iz][ix])) {
        // Push back
        float cx = (ix + 0.5f) * GRID_RES - MAP_SIZE/2;
        float cz = (iz + 0.5f) * GRID_RES - MAP_SIZE/2;
        float dx = p->x - cx;
        float dz = p->z - cz;
        if (fabs(dx) > fabs(dz)) p->x = cx + (dx>0?1.0f:-1.0f)*GRID_RES/2;
        else p->z = cz + (dz>0?1.0f:-1.0f)*GRID_RES/2;
    }

    // World bounds
    if (p->x < -MAP_SIZE/2) p->x = -MAP_SIZE/2;
    if (p->x > MAP_SIZE/2) p->x = MAP_SIZE/2;
    if (p->z < -MAP_SIZE/2) p->z = -MAP_SIZE/2;
    if (p->z > MAP_SIZE/2) p->z = MAP_SIZE/2;
}

// ---------- Building ----------
static void build(Player* p) {
    if (p->wood < 10 && p->stone < 10 && p->metal < 10) return;
    // Determine build position in front of player
    float dist = 4.0f;
    float fx = p->x + sin(p->yaw) * dist;
    float fz = p->z + cos(p->yaw) * dist;
    int gi = gridIdx(fx, fz);
    int iz = gi / GRID_DIM;
    int ix = gi % GRID_DIM;
    if (iz < 0 || iz >= GRID_DIM || ix < 0 || ix >= GRID_DIM) return;
    if (g.grid[iz][ix] != 0) return;

    int type = g_input.buildPiece;
    // Check materials
    if (type == BUILD_WALL && p->wood >= 10) { p->wood -= 10; }
    else if (type == BUILD_FLOOR && p->stone >= 10) { p->stone -= 10; }
    else if (type == BUILD_RAMP && p->metal >= 10) { p->metal -= 10; }
    else return;

    g.grid[iz][ix] = (type == BUILD_WALL) ? 1 : (type == BUILD_FLOOR ? 2 : 3);
}

// ---------- Shooting ----------
static void shoot(Player* shooter, Player* target, float dmg) {
    if (!target->alive) return;
    if (target->shield > 0) {
        if (target->shield >= dmg) { target->shield -= dmg; return; }
        dmg -= target->shield;
        target->shield = 0;
    }
    target->health -= dmg;
    if (target->health <= 0) {
        target->health = 0;
        target->alive = false;
        // Add kill feed entry (simplified)
        g.killFeed[g.killFeedCount % 5] = 1; // dummy
        g.killFeedCount++;
    }
}

static void playerShoot(Player* p) {
    if (p->ammo <= 0 && p->weapon != WEAPON_PICKAXE) return;
    if (p->weapon == WEAPON_PICKAXE) {
        // Harvesting: check if looking at grid block
        float dist = 3.5f;
        float fx = p->x + sin(p->yaw) * dist;
        float fz = p->z + cos(p->yaw) * dist;
        int gi = gridIdx(fx, fz);
        int iz = gi / GRID_DIM;
        int ix = gi % GRID_DIM;
        if (iz>=0 && iz<GRID_DIM && ix>=0 && ix<GRID_DIM) {
            int tile = g.grid[iz][ix];
            if (tile == 1) { p->wood += 15; g.grid[iz][ix] = 0; }
            else if (tile == 2) { p->stone += 15; g.grid[iz][ix] = 0; }
            else if (tile == 3) { p->metal += 15; g.grid[iz][ix] = 0; }
        }
    } else {
        // Hitscan (simplified): check all players in front
        for (int i = 0; i < g.numPlayers; i++) {
            if (i == 0) continue; // skip self (player is index 0)
            Player* target = &g.players[i];
            if (!target->alive) continue;
            float dx = target->x - p->x;
            float dz = target->z - p->z;
            float dot = sin(p->yaw)*dx + cos(p->yaw)*dz;
            if (dot < 0) continue;
            float cross = fabs(sin(p->yaw)*dz - cos(p->yaw)*dx);
            if (cross > 2.0f) continue; // aim tolerance
            float dist = sqrt(dx*dx+dz*dz);
            if (dist > 100.0f) continue;
            float dmg = (p->weapon == WEAPON_AR) ? 33.0f : 90.0f;
            shoot(p, target, dmg);
            p->ammo--;
            break;
        }
    }
}

// ---------- AI Bot Logic ----------
static void updateBot(int idx, Player* bot, BotState* bs, float dt) {
    if (!bot->alive) return;
    // Find nearest enemy
    float closestDist = 10000;
    int targetIdx = 0;
    for (int i = 0; i < g.numPlayers; i++) {
        if (i == idx) continue;
        if (!g.players[i].alive) continue;
        float dx = g.players[i].x - bot->x;
        float dz = g.players[i].z - bot->z;
        float d = dx*dx+dz*dz;
        if (d < closestDist) { closestDist = d; targetIdx = i; }
    }
    bs->target = targetIdx;
    Player* target = &g.players[targetIdx];

    // Aim toward target
    float dx = target->x - bot->x;
    float dz = target->z - bot->z;
    bot->yaw = atan2(dx, dz);

    // Move toward target if far, circle if close
    float dist = sqrt(dx*dx+dz*dz);
    if (dist > 15.0f) {
        // Simulate forward input
        bot->vx = sin(bot->yaw) * 8.0f;
        bot->vz = cos(bot->yaw) * 8.0f;
    } else {
        // Strafe
        bs->wanderAngle += dt * 2.0f;
        float strafe = sin(bs->wanderAngle);
        bot->vx = strafe * cos(bot->yaw) * 6.0f;
        bot->vz = -strafe * sin(bot->yaw) * 6.0f;
    }
    // Gravity
    if (!bot->onGround) bot->vy -= 30.0f*dt;
    if (bot->y <= 0) { bot->y = 0; bot->vy = 0; bot->onGround = true; }
    bot->x += bot->vx * dt;
    bot->z += bot->vz * dt;
    bot->y += bot->vy * dt;

    // Shooting
    bs->shootCooldown -= dt;
    if (dist < 40.0f && bs->shootCooldown <= 0) {
        // Simulate shot
        float dmg = 25.0f;
        if (target->shield > 0) {
            if (target->shield >= dmg) { target->shield -= dmg; dmg = 0; }
            else { dmg -= target->shield; target->shield = 0; }
        }
        target->health -= dmg;
        if (target->health <= 0) target->alive = false;
        bs->shootCooldown = 0.3f + (rand()%100)/200.0f;
    }

    // Occasionally build
    bs->buildCooldown -= dt;
    if (dist < 20.0f && bs->buildCooldown <= 0 && bot->wood>=10) {
        bot->wood -= 10;
        float bx = bot->x + sin(bot->yaw)*3.0f;
        float bz = bot->z + cos(bot->yaw)*3.0f;
        int gi = gridIdx(bx, bz);
        int iz = gi/GRID_DIM, ix = gi%GRID_DIM;
        if (g.grid[iz][ix] == 0) g.grid[iz][ix] = 1;
        bs->buildCooldown = 2.0f + rand()%2;
    }
}

// ---------- Storm Update ----------
static void updateStorm(float dt) {
    if (g.stormShrinkTimer > 0) {
        g.stormShrinkTimer -= dt;
        if (g.stormShrinkTimer <= 0) {
            // New target radius
            g.stormTargetRadius = fmax(5.0f, g.stormRadius - 40.0f);
        }
    }
    // Shrink radius toward target
    if (g.stormRadius > g.stormTargetRadius) {
        g.stormRadius -= 2.0f * dt;
        if (g.stormRadius < g.stormTargetRadius) g.stormRadius = g.stormTargetRadius;
    }

    // Damage players outside storm
    for (int i = 0; i < g.numPlayers; i++) {
        if (!g.players[i].alive) continue;
        float dx = g.players[i].x - g.stormCenterX;
        float dz = g.players[i].z - g.stormCenterZ;
        if (sqrt(dx*dx+dz*dz) > g.stormRadius) {
            g.players[i].health -= 5.0f * dt;
            if (g.players[i].health <= 0) g.players[i].alive = false;
        }
    }
}

// ---------- Main Update (called from JS) ----------
extern "C" EMSCRIPTEN_KEEPALIVE
void update(float dt) {
    g_deltaTime = dt;
    // Update player (index 0)
    movePlayer(&g.players[0], dt);
    // Input-based actions
    if (g_input.shoot && g.players[0].alive) {
        playerShoot(&g.players[0]);
        g_input.shoot = false;
    }
    if (g_input.build && g.players[0].alive) {
        build(&g.players[0]);
        g_input.build = false;
    }
    if (g_input.switchWeapon) {
        g.players[0].weapon = (g.players[0].weapon + 1) % 3;
        g_input.switchWeapon = false;
    }

    // Update bots
    for (int i = 1; i < g.numPlayers; i++) {
        updateBot(i, &g.players[i], &bots[i-1], dt);
    }
    updateStorm(dt);
}

// ---------- Getters for JS (positions) ----------
extern "C" EMSCRIPTEN_KEEPALIVE
int getNumPlayers() { return g.numPlayers; }

extern "C" EMSCRIPTEN_KEEPALIVE
float getPlayerX(int idx) { return g.players[idx].x; }
extern "C" EMSCRIPTEN_KEEPALIVE
float getPlayerY(int idx) { return g.players[idx].y; }
extern "C" EMSCRIPTEN_KEEPALIVE
float getPlayerZ(int idx) { return g.players[idx].z; }
extern "C" EMSCRIPTEN_KEEPALIVE
float getPlayerYaw(int idx) { return g.players[idx].yaw; }
extern "C" EMSCRIPTEN_KEEPALIVE
int getPlayerAlive(int idx) { return g.players[idx].alive ? 1 : 0; }
extern "C" EMSCRIPTEN_KEEPALIVE
float getPlayerHealth(int idx) { return g.players[idx].health; }
extern "C" EMSCRIPTEN_KEEPALIVE
float getPlayerShield(int idx) { return g.players[idx].shield; }

extern "C" EMSCRIPTEN_KEEPALIVE
int getGridDim() { return GRID_DIM; }
extern "C" EMSCRIPTEN_KEEPALIVE
int getGridTile(int ix, int iz) { return g.grid[iz][ix]; }

extern "C" EMSCRIPTEN_KEEPALIVE
float getStormRadius() { return g.stormRadius; }
