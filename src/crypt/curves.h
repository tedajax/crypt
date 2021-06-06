#pragma once

#include "flecs.h"

typedef struct GameCurves {
    int dummy;
} GameCurves;

void GameCurvesImport(ecs_world_t* world);

#define GameCurvesImportHandles(handles)