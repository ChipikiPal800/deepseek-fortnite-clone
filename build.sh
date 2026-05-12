#!/bin/bash
emcc game.cpp -o game.wasm \
  -O3 \
  -s WASM=1 \
  -s EXPORTED_FUNCTIONS='["_initGame","_setInput","_update","_getNumPlayers","_getAliveCount","_getPlayerX","_getPlayerY","_getPlayerZ","_getPlayerYaw","_getPlayerPitch","_getPlayerAlive","_getPlayerHealth","_getPlayerShield","_getPlayerWeapon","_getPlayerAmmo","_getPlayerWood","_getPlayerStone","_getPlayerMetal","_getPlayerIsBot","_getPlayerOnGround","_getGridDim","_getGridTile","_getChestX","_getChestZ","_getChestOpened","_getStormRadius","_getStormShrinkTimer"]' \
  -s ALLOW_MEMORY_GROWTH=1 \
  --no-entry

echo "Build complete: game.wasm"
