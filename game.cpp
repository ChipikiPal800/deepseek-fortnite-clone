#include <cmath>
#include <cstring>
#include <cstdlib>
#include <emscripten.h>

// ===== Constants =====
#define MAP_SIZE 300.0f
#define GRID_RES 2.0f
#define GRID_DIM 150
#define MAX_PLAYERS 16
#define MAX_CHESTS 20
#define PI 3.14159265359f

// ===== Weapon Types =====
enum Weapon {
  WEAPON_PICKAXE = 0,
  WEAPON_AR = 1,
  WEAPON_SHOTGUN = 2
};

// ===== Build Piece Types =====
enum BuildType {
  BUILD_WALL = 1,
  BUILD_FLOOR = 2,
  BUILD_RAMP = 3
};

// ===== Player Structure =====
struct Player {
  float x, y, z;
  float vx, vy, vz;
  float yaw, pitch;
  float health, shield;
  int weapon;
  int ammo;
  int wood, stone, metal;
  bool alive;
  bool onGround;
  bool isBot;
  float botTimer;
  float botShootCd;
  float botBuildCd;
  float botWanderAngle;
  int botTarget;
};

// ===== Game State =====
struct GameState {
  Player players[MAX_PLAYERS];
  int numPlayers;
  int grid[GRID_DIM * GRID_DIM]; // 0=empty, 1=wall, 2=floor, 3=ramp
  float stormCenterX, stormCenterZ;
  float stormRadius;
  float stormTargetRadius;
  float stormShrinkTimer;
  bool stormActive;
  float chestX[MAX_CHESTS];
  float chestZ[MAX_CHESTS];
  bool chestOpened[MAX_CHESTS];
  int aliveCount;
};

// ===== Global State =====
GameState g_state;

// ===== Input State (written by JS) =====
struct InputState {
  float forward;
  float backward;
  float left;
  float right;
  float jump;
  float shoot;
  float build;
  float switchWeapon;
  float mouseDx;
  float mouseDy;
  int buildPiece; // 0=wall, 1=floor, 2=ramp
};

InputState g_input;
bool g_inputDirty = false;

// ===== Helper Functions =====
static int gridIndex(float x, float z) {
  int ix = (int)((x + MAP_SIZE / 2.0f) / GRID_RES);
  int iz = (int)((z + MAP_SIZE / 2.0f) / GRID_RES);
  if (ix < 0) ix = 0;
  if (ix >= GRID_DIM) ix = GRID_DIM - 1;
  if (iz < 0) iz = 0;
  if (iz >= GRID_DIM) iz = GRID_DIM - 1;
  return iz * GRID_DIM + ix;
}

static bool isSolid(int tile) {
  return tile == BUILD_WALL || tile == BUILD_RAMP;
}

static bool isBlocked(float x, float z) {
  int idx = gridIndex(x, z);
  return isSolid(g_state.grid[idx]);
}

static float clamp(float val, float minVal, float maxVal) {
  if (val < minVal) return minVal;
  if (val > maxVal) return maxVal;
  return val;
}

// ===== Initialize Game =====
extern "C" EMSCRIPTEN_KEEPALIVE
void initGame() {
  memset(&g_state, 0, sizeof(GameState));
  memset(&g_input, 0, sizeof(InputState));

  // Initialize grid
  for (int i = 0; i < GRID_DIM * GRID_DIM; i++) {
    g_state.grid[i] = 0;
  }

  // Place pre-built structures
  for (int s = 0; s < 12; s++) {
    int cx = 20 + (rand() % (GRID_DIM - 40));
    int cz = 20 + (rand() % (GRID_DIM - 40));
    for (int dx = 0; dx < 4; dx++) {
      for (int dz = 0; dz < 4; dz++) {
        int idx = (cz + dz) * GRID_DIM + (cx + dx);
        if (idx >= 0 && idx < GRID_DIM * GRID_DIM) {
          g_state.grid[idx] = BUILD_WALL;
        }
      }
    }
  }

  // Place chests
  for (int i = 0; i < MAX_CHESTS; i++) {
    g_state.chestX[i] = (float)((rand() % (int)(MAP_SIZE - 40)) - (int)(MAP_SIZE / 2) + 20);
    g_state.chestZ[i] = (float)((rand() % (int)(MAP_SIZE - 40)) - (int)(MAP_SIZE / 2) + 20);
    g_state.chestOpened[i] = false;
  }

  // Storm
  g_state.stormCenterX = 0;
  g_state.stormCenterZ = 0;
  g_state.stormRadius = 200.0f;
  g_state.stormTargetRadius = 15.0f;
  g_state.stormShrinkTimer = 60.0f;
  g_state.stormActive = true;

  // Player
  g_state.numPlayers = 1 + 10; // 1 human + 10 bots
  g_state.aliveCount = g_state.numPlayers;

  Player* p = &g_state.players[0];
  p->x = (float)((rand() % 100) - 50);
  p->z = (float)((rand() % 100) - 50);
  p->y = 200.0f;
  p->vx = 0;
  p->vy = 0;
  p->vz = 0;
  p->yaw = 0;
  p->pitch = 0;
  p->health = 100.0f;
  p->shield = 100.0f;
  p->weapon = WEAPON_PICKAXE;
  p->ammo = 30;
  p->wood = 200;
  p->stone = 150;
  p->metal = 100;
  p->alive = true;
  p->onGround = false;
  p->isBot = false;

  // Bots
  for (int i = 1; i < g_state.numPlayers; i++) {
    Player* b = &g_state.players[i];
    b->x = (float)((rand() % 200) - 100);
    b->z = (float)((rand() % 200) - 100);
    b->y = 200.0f;
    b->vx = 0;
    b->vy = 0;
    b->vz = 0;
    b->yaw = (float)(rand() % 360) * PI / 180.0f;
    b->pitch = 0;
    b->health = 100.0f;
    b->shield = (float)(50 + rand() % 50);
    b->weapon = WEAPON_AR;
    b->ammo = 30;
    b->wood = 100;
    b->stone = 80;
    b->metal = 50;
    b->alive = true;
    b->onGround = false;
    b->isBot = true;
    b->botTimer = 0;
    b->botShootCd = 0.3f + ((float)(rand() % 100) / 100.0f) * 0.5f;
    b->botBuildCd = 2.0f + (float)(rand() % 3);
    b->botWanderAngle = (float)(rand() % 360) * PI / 180.0f;
    b->botTarget = 0;
  }
}

// ===== Apply Input From JS =====
extern "C" EMSCRIPTEN_KEEPALIVE
void setInput(
  float fwd, float bwd, float lft, float rgt,
  float jmp, float sht, float bld, float swp,
  float mdx, float mdy, int bp
) {
  g_input.forward = fwd;
  g_input.backward = bwd;
  g_input.left = lft;
  g_input.right = rgt;
  g_input.jump = jmp;
  g_input.shoot = sht;
  g_input.build = bld;
  g_input.switchWeapon = swp;
  g_input.mouseDx = mdx;
  g_input.mouseDy = mdy;
  g_input.buildPiece = bp;
  g_inputDirty = true;
}

// ===== Shoot Logic =====
static void applyDamage(Player* target, float damage) {
  if (!target->alive) return;
  if (target->shield > 0) {
    if (target->shield >= damage) {
      target->shield -= damage;
      return;
    }
    damage -= target->shield;
    target->shield = 0;
  }
  target->health -= damage;
  if (target->health <= 0) {
    target->health = 0;
    target->alive = false;
    g_state.aliveCount--;
  }
}

static void playerShoot(Player* shooter) {
  if (!shooter->alive) return;

  if (shooter->weapon == WEAPON_PICKAXE) {
    // Harvest mode
    float reach = 4.0f;
    float hx = shooter->x + sinf(shooter->yaw) * reach;
    float hz = shooter->z + cosf(shooter->yaw) * reach;
    int idx = gridIndex(hx, hz);
    if (idx >= 0 && idx < GRID_DIM * GRID_DIM) {
      int tile = g_state.grid[idx];
      if (tile == BUILD_WALL) {
        shooter->wood += 20;
        g_state.grid[idx] = 0;
      } else if (tile == BUILD_FLOOR) {
        shooter->stone += 20;
        g_state.grid[idx] = 0;
      } else if (tile == BUILD_RAMP) {
        shooter->metal += 20;
        g_state.grid[idx] = 0;
      }
    }
    return;
  }

  // Ranged weapons
  if (shooter->ammo <= 0) {
    shooter->weapon = WEAPON_PICKAXE;
    return;
  }
  shooter->ammo--;

  float range = (shooter->weapon == WEAPON_SHOTGUN) ? 30.0f : 100.0f;
  float damage = (shooter->weapon == WEAPON_AR) ? 33.0f : 90.0f;
  float spread = (shooter->weapon == WEAPON_SHOTGUN) ? 0.15f : 0.02f;

  for (int i = 0; i < g_state.numPlayers; i++) {
    if (i == 0 && !shooter->isBot) continue; // skip self if player
    if (shooter->isBot && &g_state.players[i] == shooter) continue;
    Player* target = &g_state.players[i];
    if (!target->alive) continue;

    float dx = target->x - shooter->x;
    float dy = target->y - shooter->y;
    float dz = target->z - shooter->z;
    float dist = sqrtf(dx * dx + dy * dy + dz * dz);
    if (dist > range) continue;

    // Check aim
    float dot = sinf(shooter->yaw) * dx + cosf(shooter->yaw) * dz;
    if (dot < 0) continue;
    float cross = fabsf(sinf(shooter->yaw) * dz - cosf(shooter->yaw) * dx);
    if (cross > spread * dist + 1.5f) continue;

    applyDamage(target, damage);
    if (shooter->weapon == WEAPON_SHOTGUN) break; // shotgun hits one target
  }
}

// ===== Build Logic =====
static void playerBuild(Player* p) {
  if (!p->alive) return;
  int cost = 10;
  int type = g_input.buildPiece + 1; // 1=wall, 2=floor, 3=ramp

  if (type == BUILD_WALL && p->wood < cost) return;
  if (type == BUILD_FLOOR && p->stone < cost) return;
  if (type == BUILD_RAMP && p->metal < cost) return;

  float dist = 5.0f;
  float bx = p->x + sinf(p->yaw) * dist;
  float bz = p->z + cosf(p->yaw) * dist;
  int idx = gridIndex(bx, bz);
  if (idx < 0 || idx >= GRID_DIM * GRID_DIM) return;
  if (g_state.grid[idx] != 0) return;

  if (type == BUILD_WALL) p->wood -= cost;
  else if (type == BUILD_FLOOR) p->stone -= cost;
  else p->metal -= cost;

  g_state.grid[idx] = type;
}

// ===== Bot AI =====
static void updateBot(Player* bot, float dt) {
  if (!bot->alive) return;

  // Find nearest target
  float closestDist = 100000.0f;
  int targetIdx = 0;
  for (int i = 0; i < g_state.numPlayers; i++) {
    if (!g_state.players[i].alive) continue;
    if (&g_state.players[i] == bot) continue;
    float dx = g_state.players[i].x - bot->x;
    float dz = g_state.players[i].z - bot->z;
    float d = dx * dx + dz * dz;
    if (d < closestDist) {
      closestDist = d;
      targetIdx = i;
    }
  }
  bot->botTarget = targetIdx;
  Player* target = &g_state.players[targetIdx];

  // Aim at target
  float dx = target->x - bot->x;
  float dz = target->z - bot->z;
  bot->yaw = atan2f(dx, dz);
  float dist = sqrtf(dx * dx + dz * dz);

  // Movement AI
  if (dist > 20.0f) {
    bot->vx = sinf(bot->yaw) * 10.0f;
    bot->vz = cosf(bot->yaw) * 10.0f;
  } else if (dist > 10.0f) {
    bot->botWanderAngle += dt * 3.0f;
    float strafe = sinf(bot->botWanderAngle);
    bot->vx = strafe * cosf(bot->yaw) * 7.0f;
    bot->vz = -strafe * sinf(bot->yaw) * 7.0f;
  } else {
    bot->vx = 0;
    bot->vz = 0;
  }

  // Gravity
  if (!bot->onGround) bot->vy -= 30.0f * dt;
  if (bot->y <= 0) {
    bot->y = 0;
    bot->vy = 0;
    bot->onGround = true;
  }

  // Integrate position
  bot->x += bot->vx * dt;
  bot->y += bot->vy * dt;
  bot->z += bot->vz * dt;

  // World bounds
  float halfMap = MAP_SIZE / 2.0f;
  bot->x = clamp(bot->x, -halfMap + 1, halfMap - 1);
  bot->z = clamp(bot->z, -halfMap + 1, halfMap - 1);

  // Simple collision with grid
  if (isBlocked(bot->x, bot->z)) {
    bot->x -= bot->vx * dt;
    bot->z -= bot->vz * dt;
  }

  // Shooting
  bot->botShootCd -= dt;
  if (dist < 50.0f && bot->botShootCd <= 0 && bot->ammo > 0) {
    playerShoot(bot);
    bot->botShootCd = 0.3f + ((float)(rand() % 100) / 200.0f);
  }

  // Building
  bot->botBuildCd -= dt;
  if (dist < 20.0f && bot->botBuildCd <= 0 && bot->wood >= 10) {
    float bx = bot->x + sinf(bot->yaw) * 4.0f;
    float bz = bot->z + cosf(bot->yaw) * 4.0f;
    int idx = gridIndex(bx, bz);
    if (idx >= 0 && idx < GRID_DIM * GRID_DIM && g_state.grid[idx] == 0) {
      g_state.grid[idx] = BUILD_WALL;
      bot->wood -= 10;
    }
    bot->botBuildCd = 2.0f + (float)(rand() % 2);
  }
}

// ===== Storm Update =====
static void updateStorm(float dt) {
  if (!g_state.stormActive) return;

  if (g_state.stormShrinkTimer > 0) {
    g_state.stormShrinkTimer -= dt;
    if (g_state.stormShrinkTimer <= 0) {
      g_state.stormTargetRadius = fmaxf(10.0f, g_state.stormRadius - 50.0f);
    }
  }

  if (g_state.stormRadius > g_state.stormTargetRadius) {
    g_state.stormRadius -= 2.5f * dt;
    if (g_state.stormRadius < g_state.stormTargetRadius) {
      g_state.stormRadius = g_state.stormTargetRadius;
    }
  }

  // Damage players outside storm
  for (int i = 0; i < g_state.numPlayers; i++) {
    if (!g_state.players[i].alive) continue;
    float dx = g_state.players[i].x - g_state.stormCenterX;
    float dz = g_state.players[i].z - g_state.stormCenterZ;
    if (sqrtf(dx * dx + dz * dz) > g_state.stormRadius) {
      applyDamage(&g_state.players[i], 5.0f * dt);
    }
  }
}

// ===== Main Update =====
extern "C" EMSCRIPTEN_KEEPALIVE
void update(float dt) {
  if (!g_state.players[0].alive) return;

  // Update player 0 (human)
  Player* p = &g_state.players[0];

  // Apply mouse rotation
  p->yaw += g_input.mouseDx;
  p->pitch -= g_input.mouseDy;
  p->pitch = clamp(p->pitch, -1.4f, 0.6f);
  g_input.mouseDx = 0;
  g_input.mouseDy = 0;

  // Movement
  float speed = 14.0f;
  float moveX = 0, moveZ = 0;
  if (g_input.forward > 0) { moveX += sinf(p->yaw); moveZ += cosf(p->yaw); }
  if (g_input.backward > 0) { moveX -= sinf(p->yaw); moveZ -= cosf(p->yaw); }
  if (g_input.left > 0) { moveX -= cosf(p->yaw); moveZ += sinf(p->yaw); }
  if (g_input.right > 0) { moveX += cosf(p->yaw); moveZ -= sinf(p->yaw); }

  float len = sqrtf(moveX * moveX + moveZ * moveZ);
  if (len > 0) { moveX /= len; moveZ /= len; }

  p->vx = moveX * speed;
  p->vz = moveZ * speed;

  // Gravity
  if (!p->onGround) p->vy -= 30.0f * dt;
  if (g_input.jump > 0 && p->onGround) {
    p->vy = 15.0f;
    p->onGround = false;
  }

  // Integrate position
  p->x += p->vx * dt;
  p->y += p->vy * dt;
  p->z += p->vz * dt;

  // Ground collision
  if (p->y <= 0) {
    p->y = 0;
    p->vy = 0;
    p->onGround = true;
  }

  // World bounds
  float halfMap = MAP_SIZE / 2.0f;
  p->x = clamp(p->x, -halfMap + 1, halfMap - 1);
  p->z = clamp(p->z, -halfMap + 1, halfMap - 1);

  // Grid collision
  if (isBlocked(p->x, p->z)) {
    p->x -= p->vx * dt;
    p->z -= p->vz * dt;
  }

  // Actions
  if (g_input.shoot > 0) {
    playerShoot(p);
    g_input.shoot = 0;
  }
  if (g_input.build > 0) {
    playerBuild(p);
    g_input.build = 0;
  }
  if (g_input.switchWeapon > 0) {
    p->weapon = (p->weapon + 1) % 3;
    g_input.switchWeapon = 0;
  }

  // Update bots
  for (int i = 1; i < g_state.numPlayers; i++) {
    updateBot(&g_state.players[i], dt);
  }

  // Update storm
  updateStorm(dt);
}

// ===== Getters for JS =====
extern "C" EMSCRIPTEN_KEEPALIVE int getNumPlayers() { return g_state.numPlayers; }
extern "C" EMSCRIPTEN_KEEPALIVE int getAliveCount() { return g_state.aliveCount; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerX(int i) { return g_state.players[i].x; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerY(int i) { return g_state.players[i].y; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerZ(int i) { return g_state.players[i].z; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerYaw(int i) { return g_state.players[i].yaw; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerPitch(int i) { return g_state.players[i].pitch; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerAlive(int i) { return g_state.players[i].alive ? 1 : 0; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerHealth(int i) { return g_state.players[i].health; }
extern "C" EMSCRIPTEN_KEEPALIVE float getPlayerShield(int i) { return g_state.players[i].shield; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerWeapon(int i) { return g_state.players[i].weapon; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerAmmo(int i) { return g_state.players[i].ammo; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerWood(int i) { return g_state.players[i].wood; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerStone(int i) { return g_state.players[i].stone; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerMetal(int i) { return g_state.players[i].metal; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerIsBot(int i) { return g_state.players[i].isBot ? 1 : 0; }
extern "C" EMSCRIPTEN_KEEPALIVE int getPlayerOnGround(int i) { return g_state.players[i].onGround ? 1 : 0; }

extern "C" EMSCRIPTEN_KEEPALIVE int getGridDim() { return GRID_DIM; }
extern "C" EMSCRIPTEN_KEEPALIVE int getGridTile(int ix, int iz) {
  if (ix < 0 || ix >= GRID_DIM || iz < 0 || iz >= GRID_DIM) return 0;
  return g_state.grid[iz * GRID_DIM + ix];
}

extern "C" EMSCRIPTEN_KEEPALIVE float getChestX(int i) { return g_state.chestX[i]; }
extern "C" EMSCRIPTEN_KEEPALIVE float getChestZ(int i) { return g_state.chestZ[i]; }
extern "C" EMSCRIPTEN_KEEPALIVE int getChestOpened(int i) { return g_state.chestOpened[i] ? 1 : 0; }

extern "C" EMSCRIPTEN_KEEPALIVE float getStormRadius() { return g_state.stormRadius; }
extern "C" EMSCRIPTEN_KEEPALIVE float getStormShrinkTimer() { return g_state.stormShrinkTimer; }
