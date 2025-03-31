#include "raylib.h"
#include <math.h>

#include "raymath.h"

#define GRID_ROWS 5
#define GRID_COLS 7
#define CELL_SIZE 100
#define OBSTACLE_SIZE (CELL_SIZE * 0.8f)
#define SCREEN_WIDTH (GRID_COLS * CELL_SIZE + 250)
#define SCREEN_HEIGHT (GRID_ROWS * CELL_SIZE)
#define MAX_ENEMIES 35
#define OBSTACLE_COUNT 5

typedef enum {
    EMPTY,
    CASTLE,
    ENEMY,
    OBSTACLE
} CellType;

typedef struct {
    Vector2 position;
    Vector2 targetCell;
    int health;
    int speed;
    int active;
    int worth;
} Enemy;

typedef struct {
    CellType grid[GRID_ROWS][GRID_COLS];
    int castleHealth;
    int wave;
    int enemiesInWave;
    int enemiesAlive;
    Enemy enemies[MAX_ENEMIES];
    float spawnTimer;
    float waveTimer;
    bool waveInProgress;
    int score;
} GameState;

Vector2 GetGridPosition(Vector2 pos) {
    return (Vector2){(int)(pos.x / CELL_SIZE), (int)(pos.y / CELL_SIZE)};
}

bool IsCellWalkable(GameState *game, int x, int y) {
    if (x < 0 || x >= GRID_COLS || y < 0 || y >= GRID_ROWS) return false;
    return game->grid[y][x] != OBSTACLE && game->grid[y][x] != CASTLE;
}

void InitializeGame(GameState *game) {
    // Initialize grid
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            game->grid[y][x] = EMPTY;
        }
    }

    // Castle 
    for (int y = 0; y < GRID_ROWS; y++) {
        game->grid[y][GRID_COLS-1] = CASTLE;
    }

    // Random obstacles with 1 cell between castle
    for (int i = 0; i < OBSTACLE_COUNT; i++) {
        int x, y;
        do {
            x = GetRandomValue(0, GRID_COLS-3);
            y = GetRandomValue(0, GRID_ROWS-1);
        } while (game->grid[y][x] != EMPTY);

        game->grid[y][x] = OBSTACLE;
    }

    game->castleHealth = 10;

    // Initialize enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        game->enemies[i].active = false;
    }

    game->wave = 0;
    game->enemiesInWave = 0;
    game->enemiesAlive = 0;
    game->spawnTimer = 0;
    game->waveTimer = 3.0f;
    game->waveInProgress = false;
    game->score = 0;
}

void SpawnWave(GameState *game) {
    game->wave++;
    game->enemiesInWave = game->wave * 3;
    game->enemiesAlive = game->enemiesInWave;
    game->spawnTimer = 0.5f;
    game->waveInProgress = true;
}

void SpawnEnemy(GameState *game) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) {
            int spawnY = GetRandomValue(0, GRID_ROWS-1);

            game->enemies[i].position = (Vector2){CELL_SIZE/2, spawnY * CELL_SIZE + CELL_SIZE/2};
            game->enemies[i].targetCell = GetGridPosition(game->enemies[i].position);
            game->enemies[i].health = 1;
            game->enemies[i].speed = 50 + game->wave * 10;
            game->enemies[i].active = true;
            game->enemies[i].worth = 10;

            game->enemiesInWave--;
            break;
        }
    }
}

Vector2 FindNextPathCell(GameState *game, Vector2 currentCell) {
    // First try to move right
    if (IsCellWalkable(game, currentCell.x + 1, currentCell.y)) {
        return (Vector2){currentCell.x + 1, currentCell.y};
    }

    // If blocked, try moving up or down
    bool canMoveUp = IsCellWalkable(game, currentCell.x, currentCell.y - 1);
    bool canMoveDown = IsCellWalkable(game, currentCell.x, currentCell.y + 1);

    if (canMoveUp && canMoveDown) {
        // Choose randomly if both are available
        return (GetRandomValue(0, 1) == 0 ?
            (Vector2){currentCell.x, currentCell.y - 1} :
            (Vector2){currentCell.x, currentCell.y + 1});
    }
    else if (canMoveUp) {
        return (Vector2){currentCell.x, currentCell.y - 1};
    }
    else if (canMoveDown) {
        return (Vector2){currentCell.x, currentCell.y + 1};
    }

    // If completely blocked, try diagonal moves
    if (IsCellWalkable(game, currentCell.x + 1, currentCell.y - 1)) {
        return (Vector2){currentCell.x + 1, currentCell.y - 1};
    }
    if (IsCellWalkable(game, currentCell.x + 1, currentCell.y + 1)) {
        return (Vector2){currentCell.x + 1, currentCell.y + 1};
    }

    return currentCell;
}

void UpdateEnemies(GameState *game, float deltaTime) {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) continue;

        Vector2 currentCell = GetGridPosition(game->enemies[i].position);
        Vector2 targetCenter = {
            game->enemies[i].targetCell.x * CELL_SIZE + CELL_SIZE/2,
            game->enemies[i].targetCell.y * CELL_SIZE + CELL_SIZE/2
        };

        // Pathfinding
        if (Vector2Distance(game->enemies[i].position, targetCenter) < 5 ||
            !IsCellWalkable(game, game->enemies[i].targetCell.x, game->enemies[i].targetCell.y)) {
            game->enemies[i].targetCell = FindNextPathCell(game, currentCell);
            targetCenter = (Vector2){
                game->enemies[i].targetCell.x * CELL_SIZE + CELL_SIZE/2,
                game->enemies[i].targetCell.y * CELL_SIZE + CELL_SIZE/2
            };
        }

        //moved toward castle
        Vector2 direction = {
            targetCenter.x - game->enemies[i].position.x,
            targetCenter.y - game->enemies[i].position.y
        };

        // direction, helped with internet
        float length = sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 0) {
            direction.x /= length;
            direction.y /= length;
        }

        game->enemies[i].position.x += direction.x * game->enemies[i].speed * deltaTime;
        game->enemies[i].position.y += direction.y * game->enemies[i].speed * deltaTime;

        // check if enemies passed castle
        if (game->enemies[i].position.x >= (GRID_COLS-1) * CELL_SIZE) {
            game->enemies[i].active = false;
            game->castleHealth--;
            game->enemiesAlive--;
        }
    }
}
void HandleMouseClick(GameState *game) {
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        Vector2 mousePos = GetMousePosition();

        if (mousePos.x < GRID_COLS * CELL_SIZE) {
            for (int i = 0; i < MAX_ENEMIES; i++) {
                if (game->enemies[i].active && CheckCollisionPointCircle(mousePos, game->enemies[i].position, 20)) {
                    game->enemies[i].active = false;
                    game->enemiesAlive--;
                    game->score += game->enemies[i].worth;

                    break;

                }
            }

        }
    }
}
void UpdateGame(GameState *game, float deltaTime) {
    if (game->castleHealth <= 0) return;
    if (game->wave >= 5 && game->enemiesAlive == 0 && !game->waveInProgress) return;

    HandleMouseClick(game);

    if (!game->waveInProgress) {
        game->waveTimer -= deltaTime;
        if (game->waveTimer <= 0 && game->wave < 5) {
            SpawnWave(game);
        }
    } else {
        if (game->enemiesInWave > 0 && game->spawnTimer <= 0) {
            SpawnEnemy(game);
            game->spawnTimer = 0.5f;
        }
        game->spawnTimer -= deltaTime;

        UpdateEnemies(game, deltaTime);

        if (game->enemiesAlive == 0 && game->enemiesInWave == 0) {
            game->waveInProgress = false;
            game->waveTimer = 3.0f;
        }
    }
}

void DrawGame(GameState *game) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // grid code
    for (int y = 0; y < GRID_ROWS; y++) {
        for (int x = 0; x < GRID_COLS; x++) {
            Rectangle cell = {x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE};
            DrawRectangleLinesEx(cell, 2, BLACK);

            if (game->grid[y][x] == CASTLE) {
                DrawRectangle(x * CELL_SIZE, y * CELL_SIZE, CELL_SIZE, CELL_SIZE, RED);
            }
            else if (game->grid[y][x] == OBSTACLE) {
                DrawRectangle(
                    x * CELL_SIZE + (CELL_SIZE - OBSTACLE_SIZE)/2,
                    y * CELL_SIZE + (CELL_SIZE - OBSTACLE_SIZE)/2,
                    OBSTACLE_SIZE,
                    OBSTACLE_SIZE,
                    BROWN
                );
            }
        }
    }

    // the enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game->enemies[i].active) {
            DrawCircle(game->enemies[i].position.x, game->enemies[i].position.y, 20, BLUE);
        }
    }

    // create ui
    DrawRectangle(GRID_COLS * CELL_SIZE, 0, 250, SCREEN_HEIGHT, LIGHTGRAY);
    DrawText("TOWER DEFENSE", GRID_COLS * CELL_SIZE + 10, 10, 20, BLACK);
    DrawText(TextFormat("Wave: %d/5", game->wave), GRID_COLS * CELL_SIZE + 10, 40, 20, BLACK);
    DrawText(TextFormat("Castle Health: %d/10", game->castleHealth), GRID_COLS * CELL_SIZE + 10, 70, 20, BLACK);
    DrawText(TextFormat("Score: %d", game->score), GRID_COLS * CELL_SIZE + 10, 100, 20, BLACK);

    if (game->wave < 5 && !game->waveInProgress) {
        DrawText(TextFormat("Next wave in: %.1f", game->waveTimer), GRID_COLS * CELL_SIZE + 10, 130, 20, BLACK);
    }
    if (game->castleHealth <= 0) {
        DrawText("GAME OVER", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 40, RED);
    } else if (game->wave >= 5 && game->enemiesAlive == 0 && !game->waveInProgress) {
        DrawText("YOU WIN!", SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2, 40, GREEN);
        DrawText(TextFormat("Final Score: %d", game->score), SCREEN_WIDTH/2 - 100, SCREEN_HEIGHT/2 + 50, 30, BLACK);
    }
    EndDrawing();
}
int main() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Tower Defense Game");
    SetTargetFPS(60);
    GameState game;
    InitializeGame(&game);
    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();
        UpdateGame(&game, deltaTime);
        DrawGame(&game);
    }
    CloseWindow();
    return 0;
}