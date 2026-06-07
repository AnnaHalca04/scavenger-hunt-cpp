#define GRAPHICS_API_OPENGL_21   // Forțează utilizarea OpenGL 2.1 (pentru compatibilitate cu sisteme vechi)
#include "raylib.h"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <string>
#include <vector>
#include <array>

// -----------------------------------------------------------------------------
// Funcție custom pentru desenarea textului cu wrapping (word-wrap)
// -----------------------------------------------------------------------------
void DrawTextRecCustom(Font font, const char* text, Rectangle rec,
                         float fontSize, float spacing, bool wordWrap, Color tint)
{
    if (!wordWrap) {
        DrawTextEx(font, text, { rec.x, rec.y }, fontSize, spacing, tint);
        return;
    }
    std::string s(text);
    std::vector<std::string> words;
    size_t pos = 0;
    while (true) {
        size_t found = s.find_first_of(" \n", pos);
        if (found == std::string::npos) {
            if (pos < s.size()) words.push_back(s.substr(pos));
            break;
        }
        if (found > pos) {
            words.push_back(s.substr(pos, found - pos));
        }
        if (s[found] == '\n') {
            words.push_back("\n");
        }
        pos = found + 1;
    }
    std::string line;
    float lineHeight = fontSize + spacing;
    float y = rec.y;
    for (size_t i = 0; i < words.size(); i++) {
        if (words[i] == "\n") {
            DrawTextEx(font, line.c_str(), { rec.x, y }, fontSize, spacing, tint);
            y += lineHeight;
            line.clear();
            continue;
        }
        std::string testLine = (line.empty() ? words[i] : line + " " + words[i]);
        Vector2 sizeText = MeasureTextEx(font, testLine.c_str(), fontSize, spacing);
        if (sizeText.x > rec.width && !line.empty()) {
            DrawTextEx(font, line.c_str(), { rec.x, y }, fontSize, spacing, tint);
            y += lineHeight;
            line = words[i];
        } else {
            line = testLine;
        }
    }
    if (!line.empty()) {
        DrawTextEx(font, line.c_str(), { rec.x, y }, fontSize, spacing, tint);
    }
}

// ------------------ CONSTANTE GLOBALE ------------------
const int PLAYER_SIZE = 32;
const float ITEM_SCALE = 1.0f;   // (potion, gem, speed) => 48×48
const float NPC_SCALE  = 1.0f;   // Wizard => 48×48
const float COIN_SCALE = 1.0f;   // Coin => 24×24

enum EnemyMoveType { MOVE_HORIZONTAL, MOVE_VERTICAL, MOVE_RANDOM, MOVE_PATROL };

enum GameScreen {
    SCREEN_MENU,
    SCREEN_INSTRUCTIONS,
    SCREEN_CREDENTIALS,
    SCREEN_STORY,
    SCREEN_GAME,
    SCREEN_WIN,
    SCREEN_END,
    SCREEN_CORRECT_ANSWERS
};

static int GetRandomDirection() {
    return GetRandomValue(0, 3);
}

bool IsColliding(const Rectangle &a, const Rectangle &b) {
    return CheckCollisionRecs(a, b);
}

// --------------------------------------------------
// ------------------ CLASA QUEST -------------------
// --------------------------------------------------
class Quest {
public:
    std::string title;
    std::string description;
    bool isActive;
    bool isCompleted;
    int requiredGemCount;
    int currentGemCount;
    int requiredEnemyKillCount;
    int currentEnemyKillCount;
    int rewardPoints;  // +5 la finalizarea questului

    Quest()
        : isActive(false), isCompleted(false),
          requiredGemCount(0), currentGemCount(0),
          requiredEnemyKillCount(0), currentEnemyKillCount(0),
          rewardPoints(0) {}

    void UpdateProgress(int collectedGems, int killedEnemies) {
        if (isCompleted) return;
        currentGemCount       = collectedGems;
        currentEnemyKillCount = killedEnemies;
        if (currentGemCount >= requiredGemCount &&
            currentEnemyKillCount >= requiredEnemyKillCount) {
            isCompleted = true;
        }
    }
};

// --------------------------------------------------
// ------------------ CLASA QUESTION ----------------
// --------------------------------------------------
class Question {
public:
    std::string prompt;
    std::array<std::string, 4> answers;
    int correctIndex;

    Question() : correctIndex(0) {}
    Question(const std::string &p, const std::array<std::string, 4> &a, int c)
        : prompt(p), answers(a), correctIndex(c) {}
};

// --------------------------------------------------
// ------------------ CLASA PLAYER ------------------
// --------------------------------------------------
class Player {
public:
    float x, y;
    float speed;
    int score;
    int health;
    int totalGemsCollected;
    int totalEnemiesKilled;
    int coins; // 1 monedă pentru fiecare răspuns corect și pentru quest-uri finalizate

    Player(float posX = 0, float posY = 0, float spd = 4.0f)
        : x(posX), y(posY), speed(spd), score(0), health(100),
          totalGemsCollected(0), totalEnemiesKilled(0), coins(0) {}

    void Draw() {
        DrawRectangle((int)x, (int)y, PLAYER_SIZE, PLAYER_SIZE, BLUE);
    }
    void KeepInBounds(int screenWidth, int screenHeight) {
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (x > screenWidth - PLAYER_SIZE) x = screenWidth - PLAYER_SIZE;
        if (y > screenHeight - PLAYER_SIZE) y = screenHeight - PLAYER_SIZE;
    }
};

// --------------------------------------------------
// -------------------- CLASA ZONE ------------------
// --------------------------------------------------
class Zone {
public:
    float x, y, width, height;
    bool active;
    int questionIndex;
    static Font questionFont;

    Zone(float posX, float posY, float w, float h, int qIndex)
        : x(posX), y(posY), width(w), height(h),
          active(true), questionIndex(qIndex) {}

    void Draw() {
        Color col = active ? Color{255, 215, 0, 150} : Color{150, 75, 0, 100};
        DrawRectangle((int)x, (int)y, (int)width, (int)height, col);
        Color border = active ? YELLOW : BROWN;
        DrawRectangleLinesEx({ x, y, width, height }, 2, border);

        if (active) {
            int textSize = 22;
            const char* questionMark = "?";
            Vector2 sizeTextVec = MeasureTextEx(questionFont, questionMark, (float)textSize, 1.0f);
            float centerX = x + width / 2 - sizeTextVec.x / 2;
            float centerY = y + height / 2 - sizeTextVec.y / 2;
            DrawTextEx(questionFont, questionMark, { centerX, centerY }, (float)textSize, 1.0f, BLACK);
        }
    }
};
Font Zone::questionFont;

// --------------------------------------------------
// ------------------ CLASA OBSTACLE ----------------
// --------------------------------------------------
class Obstacle {
public:
    float x, y, width, height;
    Texture2D texture;
    bool active;

    Obstacle(float px, float py, float w, float h, Texture2D tex)
        : x(px), y(py), width(w), height(h), texture(tex), active(true) {}

    void Draw() {
        if (active) {
            DrawTexture(texture, (int)x, (int)y, WHITE);
        }
    }
};

// --------------------------------------------------
// ------------------- CLASA ITEM -------------------
// --------------------------------------------------
class Item {
public:
    float x, y, width, height;
    Texture2D texture;
    bool active;
    std::string itemName;

    Item(float px, float py, float w, float h, Texture2D tex, const std::string &name)
        : x(px), y(py), width(w), height(h), texture(tex), active(true), itemName(name) {}

    void Draw() {
        if (active) {
            DrawTextureEx(texture, { x, y }, 0.0f, ITEM_SCALE, WHITE);
        }
    }
};

// --------------------------------------------------
// -------------------- CLASA TILE ------------------
// --------------------------------------------------
class Tile {
public:
    float x, y;
    int size;
    Texture2D texture;
    bool useTexture;
    Color color;

    Tile(float px, float py, int s, Texture2D tex, Color c)
        : x(px), y(py), size(s), texture(tex), useTexture(true), color(c) {}

    void Draw() {
        DrawTexture(texture, (int)x, (int)y, WHITE);
    }
};

// --------------------------------------------------
// ------------------ CLASA ENEMY -------------------
// --------------------------------------------------
class Enemy {
public:
    float x, y;
    float speed;
    bool active;
    int health;
    EnemyMoveType moveType;
    float oldX, oldY;
    float randDirectionTime;
    float randDirectionLimit;
    int direction;
    float patrolTimer;
    float stuckTime;

    Enemy(float px, float py, float spd, EnemyMoveType mt)
        : x(px), y(py), speed(spd), active(true), health(30),
          moveType(mt), oldX(px), oldY(py),
          randDirectionTime(0), randDirectionLimit(0),
          direction(0), patrolTimer(0), stuckTime(0) {}

    void Update(float dt, int screenWidth, int screenHeight, const std::vector<Obstacle> &obstacles) {
        float moveVal = speed * dt * 60.f;
        oldX = x;
        oldY = y;

        // Deplasarea în funcție de tipul inamicului
        switch(moveType) {
            case MOVE_HORIZONTAL:
                x += moveVal;
                // verificare margini
                if(x < 0 || x > screenWidth - 32) {
                    x = oldX;
                    speed = -speed;
                }
                break;
            case MOVE_VERTICAL:
                y += moveVal;
                // verificare margini
                if(y < 0 || y > screenHeight - 32) {
                    y = oldY;
                    speed = -speed;
                }
                break;
            case MOVE_RANDOM:
                randDirectionTime += dt;
                if(randDirectionTime > randDirectionLimit) {
                    randDirectionTime = 0;
                    randDirectionLimit = (float)GetRandomValue(2, 4);
                    direction = GetRandomDirection();
                }
                switch(direction) {
                    case 0: y -= moveVal; break;
                    case 1: y += moveVal; break;
                    case 2: x -= moveVal; break;
                    case 3: x += moveVal; break;
                }
                // verificare margini
                if(x < 0) x = 0;
                if(x > screenWidth - 32) x = screenWidth - 32;
                if(y < 0) y = 0;
                if(y > screenHeight - 32) y = screenHeight - 32;
                break;
            case MOVE_PATROL:
                patrolTimer += dt;
                if(patrolTimer > 3.f) {
                    patrolTimer = 0;
                    direction = GetRandomDirection();
                }
                switch(direction) {
                    case 0: y -= moveVal; break;
                    case 1: y += moveVal; break;
                    case 2: x -= moveVal; break;
                    case 3: x += moveVal; break;
                }
                // verificare margini
                if(x < 0) x = 0;
                if(x > screenWidth - 32) x = screenWidth - 32;
                if(y < 0) y = 0;
                if(y > screenHeight - 32) y = screenHeight - 32;
                break;
        }

        // Coliziune cu obstacolele - dezlipire dacă e blocat
        for(auto &obs : obstacles) {
            if(obs.active) {
                Rectangle eRect  = { x, y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
                Rectangle obsRect= { obs.x, obs.y, obs.width, obs.height };
                if(CheckCollisionRecs(eRect, obsRect)) {
                    // Revenim la poziția anterioară
                    x = oldX;
                    y = oldY;
                    // Schimbăm direcția / inversăm viteza
                    direction = GetRandomDirection();
                    speed = -speed;
                    // Mutăm puțin inamicul pentru a-l scoate din coliziune
                    x += 5.0f;
                    y += 5.0f;
                    break;
                }
            }
        }

        // Mecanism "stuckTime" – dacă e prea mult blocat, forțăm altă direcție
        if(std::fabs(x - oldX) < 0.1f && std::fabs(y - oldY) < 0.1f) {
            stuckTime += dt;
        } else {
            stuckTime = 0;
        }
        if(stuckTime > 0.3f) {
            direction = GetRandomDirection();
            stuckTime = 0;
        }
    }

    void Draw() {
        if(active && health > 0) {
            DrawRectangle((int)x, (int)y, 32, 32, RED);
        }
    }
};

// --------------------------------------------------
// ----------------- CLASA BULLET --------------------
// --------------------------------------------------
class Bullet {
public:
    float x, y;
    float speed;
    bool active;
    int width, height;
    int direction; // 0 = up, 1 = down, 2 = left, 3 = right

    Bullet()
        : x(0), y(0), speed(8.f), active(false),
          width(8), height(8), direction(0) {}

    void Update(float dt, int scrW, int scrH, const std::vector<Obstacle> &obstacles) {
        if(!active) return;
        float move = speed * dt * 60.f;
        switch(direction) {
            case 0: y -= move; break;
            case 1: y += move; break;
            case 2: x -= move; break;
            case 3: x += move; break;
        }
        if(x < 0 || x > scrW || y < 0 || y > scrH) {
            active = false;
        } else {
            for(auto &obs : obstacles) {
                if(obs.active) {
                    Rectangle bRect = { x, y, (float)width, (float)height };
                    Rectangle obsRect= { obs.x, obs.y, obs.width, obs.height };
                    if(CheckCollisionRecs(bRect, obsRect)) {
                        active = false;
                        break;
                    }
                }
            }
        }
    }

    void Draw() {
        if(active) {
            DrawRectangle((int)x, (int)y, width, height, YELLOW);
        }
    }
};

// --------------------------------------------------
// ------------- CLASA TILEMAP (Harta) -------------
// --------------------------------------------------
class TileMap {
public:
    std::vector<Tile> tiles;

    TileMap(int scrW, int scrH, int tileSize, Texture2D t1, Texture2D t2) {
        for(int i = 0; i < scrW; i += tileSize) {
            for(int j = 0; j < scrH; j += tileSize) {
                Texture2D tex = (((i / tileSize) + (j / tileSize)) % 2 == 0) ? t1 : t2;
                tiles.emplace_back(i, j, tileSize, tex, WHITE);
            }
        }
    }

    void Draw() {
        for(auto &t: tiles) {
            t.Draw();
        }
    }
};

// --------------------------------------------------
// ------------------- CLASA NPC --------------------
// --------------------------------------------------
class NPC {
public:
    float x, y;
    int questIndex;
    bool questGiven;

    NPC(float px, float py, int qIndex)
        : x(px), y(py), questIndex(qIndex), questGiven(false) {}

    void Draw(Texture2D wizardTex) {
        DrawTextureEx(wizardTex, { x, y }, 0.0f, NPC_SCALE, WHITE);
    }
};

// --------------------------------------------------
// ------------------- CLASA GAME -------------------
// --------------------------------------------------
class Game {
public:
    const int internalWidth = 900;
    const int internalHeight = 650;

    RenderTexture2D target;

    Texture2D tileTex1, tileTex2;
    Texture2D wallTexture;
    Texture2D potionTexture, gemTexture, speedTexture, coinTexture;
    Texture2D npcWizardTexture;

    Font customFont;

    // Variabile noi pentru a mări textul pe ecranele de content
    int contentBigFontSize = 36;
    int contentFontSize = 28;

    TileMap* map;
    Player player;
    std::vector<Bullet> bullets;
    std::vector<Question> questions;
    std::vector<Zone> zones;
    std::vector<Obstacle> obstacles;
    std::vector<Item> items;
    std::vector<Enemy> enemies;
    std::vector<NPC> npcs;
    std::vector<Quest> quests;

    bool inQuestion;
    bool questionAnswered;
    bool answerCorrect;
    std::string questionMessage;
    Question currentQuestion;
    int selectedAnswer;
    bool showCorrectAnswer; // folosit pentru a afișa răspunsul corect

    GameScreen currentScreen;
    bool paused;
    int pauseMenuSelection;

    float credentialsScrollY;
    float instructionsScrollY;
    float storyScrollY;
    float correctAnswersScrollY; // scroll pentru ecranul de răspunsuri corecte

    // Variabila highScore - scorul maxim obținut în sesiune
    int highScore;

    // Texte (story, instructiuni, credite)
    std::string storyText;
    std::string instructionsText;
    std::string credText;

    int bigFontSize = 28;     // folosit în meniul principal
    int normalFontSize = 22;  // folosit în meniul principal

    Game()
        : player((900 - PLAYER_SIZE) / 2.f, (650 - PLAYER_SIZE) / 2.f),
          inQuestion(false), questionAnswered(false), answerCorrect(false),
          selectedAnswer(0), showCorrectAnswer(false),
          currentScreen(SCREEN_MENU),
          paused(false), pauseMenuSelection(0),
          credentialsScrollY(0), instructionsScrollY(0), storyScrollY(0), correctAnswersScrollY(0),
          highScore(0)
    {
        bullets.resize(10);

        // STORY complet
        storyText =
"GAME DESCRIPTION:\n\n"
"You are a blue square navigating a challenging maze. Collect coins by picking up gems (+2 points each) and by answering questions.\n"
"Defeat enemies or complete NPC quests (+5 points) to earn more points. Wrong answers deduct points and HP.\n"
"Use the mini-map to guide your way through the maze.\n"
"(Use UP/DOWN keys to scroll)\n";

        // Instructiuni detaliate
        instructionsText =
"CONTROLS & GAMEPLAY INSTRUCTIONS:\n\n"
"Movement:\n"
"  - Use the ARROW keys to move your character (blue square) around the maze.\n"
"Shooting:\n"
"  - Press SPACE to shoot in the direction of the last arrow key pressed.\n"
"Interaction:\n"
"  - Press E to interact with NPCs. When you first encounter an NPC, they will assign you a quest.\n"
"  - After completing the quest, return to the NPC and press E again to collect your reward (points and an extra coin).\n"
"Questions:\n"
"  - When you enter a zone marked with a question mark, a question will appear.\n"
"    Use UP/DOWN arrow keys to select an answer and press ENTER to confirm your choice.\n"
"  - Press C to show/hide the correct answer.\n"
"Scoring & Health:\n"
"  - Correct answer: +10 points and earn 1 coin.\n"
"  - Wrong answer: -5 points and lose 10 HP.\n"
"  - Collect gems for +2 points each and defeat enemies to progress in quests (+5 points).\n"
"  - Colliding with enemies deducts 1 HP per contact.\n"
"Quests:\n"
"  - Complete quests by collecting a required number of gems or by defeating enemies.\n"
"  - Once a quest is completed, revisit the corresponding NPC and press E to claim your reward (points and an extra coin).\n"
"Additional:\n"
"  - Press P to pause the game at any time.\n"
"(Use UP/DOWN keys to scroll through these instructions.)\n";

        // Credite actualizate
        credText =
"PROJECT CREDITS:\n\n"
"Project: Gender, Digitalization, Green - Ensuring a Sustainable Future for all in Europe\n"
"Reference Project Code: 2023-1-RO01-KA220-HED-000154433\n\n"
"PARTNERSHIP:\n"
" - Universitatea de Stiinte Agricole si Medicina Veterinara, Bucuresti, Romania\n"
" - Universitatea Nationala de Stiinta si Tehnologie POLITEHNICA Bucuresti, Romania\n"
" - Universitat Autonoma de Barcelona, Espana\n"
" - Universidade do Porto, Republica Portuguesa\n"
" - Uzhgorodskyi Nacionalnyi Universitet, Ukraina\n\n"
"STUDENTS:\n"
" - Voicu Alexandru (voicu.alexandru@example.com)\n"
" - Duta George (duta.george@example.com)\n"
" - Halca Anna (halca.anna@example.com)\n"
" - Marinescu Doru (marinescu.doru@example.com)\n"
" - Oprea Ioana Antonia (oprea.ioana.antonia@example.com)\n\n"
"COORDINATING PROFESSORS:\n"
" - Mihai Caramihai\n"
" - Daniel Chis\n\n"
"(Use UP/DOWN keys to scroll through the credits.)\n";
    }

    ~Game() {}

    // ----- Scroll Methods -----
    void UpdateInstructionsScroll(){
        if(IsKeyDown(KEY_UP)) instructionsScrollY += 2.f;
        if(IsKeyDown(KEY_DOWN)) instructionsScrollY -= 2.f;
    }
    void UpdateCredentialsScroll(){
        if(IsKeyDown(KEY_UP)) credentialsScrollY += 2.f;
        if(IsKeyDown(KEY_DOWN)) credentialsScrollY -= 2.f;
    }
    void UpdateStoryScroll(){
        if(IsKeyDown(KEY_UP)) storyScrollY += 2.f;
        if(IsKeyDown(KEY_DOWN)) storyScrollY -= 2.f;
    }
    void UpdateCorrectAnswersScroll(){
        if(IsKeyDown(KEY_UP)) correctAnswersScrollY += 2.f;
        if(IsKeyDown(KEY_DOWN)) correctAnswersScrollY -= 2.f;
    }

    // ----- Pause Menu -----
    void UpdatePauseMenu(){
        if(IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_DOWN)){
            pauseMenuSelection = 1 - pauseMenuSelection;
        }
        if(IsKeyPressed(KEY_ENTER)){
            if(pauseMenuSelection == 0){
                paused = false;
            } else {
                ResetGame();
                currentScreen = SCREEN_MENU;
                paused = false;
            }
        }
    }
    void DrawPauseMenu(){
        DrawRectangle(0, 0, internalWidth, internalHeight, Color{0, 0, 0, 150});
        DrawCustomText("Game Paused", 350, 100, 30, 2, YELLOW);
        std::vector<std::string> menuOpts = {"Resume", "Exit to Menu"};
        int startY = 200, step = 40;
        for(int i = 0; i < (int)menuOpts.size(); i++){
            Color c = (i == pauseMenuSelection) ? YELLOW : WHITE;
            DrawCustomText(menuOpts[i].c_str(), 400, (float)(startY + i * step), 20, 2, c);
        }
    }

    // ----- Helper text with outline -----
    void DrawCustomText(const char* text, float x, float y,
                        float fontSize, float spacing, Color color)
    {
        for(int dx = -1; dx <= 1; dx++){
            for(int dy = -1; dy <= 1; dy++){
                if(dx != 0 || dy != 0){
                    DrawTextEx(customFont, text, {x + (float)dx, y + (float)dy},
                               fontSize, spacing, BLACK);
                }
            }
        }
        DrawTextEx(customFont, text, {x, y}, fontSize, spacing, color);
    }

    // ----- Score system -----
    void AddScore(int amt){
        player.score += amt;
        if(player.score < 0) player.score = 0;
        if(player.score > highScore) highScore = player.score;
    }
    void SubScore(int amt){
        player.score -= amt;
        if(player.score < 0) player.score = 0;
    }

    // ----- Load Resources -----
    void LoadResources(){
        // tile1
        Image tile1 = LoadImage("resources/tile1.png");
        if(!tile1.data) tile1 = GenImageColor(64, 64, DARKGRAY);
        tileTex1 = LoadTextureFromImage(tile1);
        UnloadImage(tile1);

        // tile2
        Image tile2 = LoadImage("resources/tile2.png");
        if(!tile2.data) tile2 = GenImageColor(64, 64, GRAY);
        tileTex2 = LoadTextureFromImage(tile2);
        UnloadImage(tile2);

        // wall
        Image wallImg = LoadImage("resources/obstacle_wall.png");
        if(!wallImg.data) wallImg = GenImageColor(64, 64, BROWN);
        wallTexture = LoadTextureFromImage(wallImg);
        UnloadImage(wallImg);

        // potion => 48x48
        Image potImg = LoadImage("resources/potion.png");
        if(!potImg.data){
            potImg = GenImageColor(48, 48, RED);
        } else {
            ImageResize(&potImg, 48, 48);
        }
        potionTexture = LoadTextureFromImage(potImg);
        UnloadImage(potImg);

        // gem => 48x48
        Image gm = LoadImage("resources/gem.png");
        if(!gm.data){
            gm = GenImageColor(48, 48, PURPLE);
        } else {
            ImageResize(&gm, 48, 48);
        }
        gemTexture = LoadTextureFromImage(gm);
        UnloadImage(gm);

        // speed => 48x48
        Image spd = LoadImage("resources/boot.png");
        if(!spd.data){
            spd = GenImageColor(48, 48, GREEN);
        } else {
            ImageResize(&spd, 48, 48);
        }
        speedTexture = LoadTextureFromImage(spd);
        UnloadImage(spd);

        // coin => 24x24
        Image cImg = LoadImage("resources/coin.png");
        if(!cImg.data) cImg = GenImageColor(24, 24, GOLD);
        coinTexture = LoadTextureFromImage(cImg);
        UnloadImage(cImg);

        // wizard => 48x48
        Image wImg = LoadImage("resources/wizard.png");
        if(!wImg.data){
            wImg = GenImageColor(48, 48, PURPLE);
        } else {
            ImageResize(&wImg, 48, 48);
        }
        npcWizardTexture = LoadTextureFromImage(wImg);
        UnloadImage(wImg);

        // font => calibri
        customFont = LoadFontEx("resources/calibri.ttf", 40, 0, 0);
        Zone::questionFont = customFont;

        // harta
        map = new TileMap(internalWidth, internalHeight, 64, tileTex1, tileTex2);

        // Întrebări
        questions = {
            Question(
                "1. What is the EU target regarding climate neutrality?",
                { "A) 55% by 2050", "B) Zero net by 2040", "C) Gradual end century", "D) Zero net by 2050" },
                3
            ),
            Question(
                "2. Which are the GHG and the most abundant among them?",
                { "A) Only CO2 & ozone", "B) CO2, CH4, N2O, CFCs, O3, H2O; CO2 ~80%", "C) No precise info", "D) Variation too large" },
                1
            ),
            Question(
                "3. Transition: does it bring economic benefits or only costs?",
                { "A) Opportunities for growth", "B) Only higher costs", "C) No costs, only benefits", "D) No consequences" },
                0
            ),
            Question(
                "4. CO2 utilization by chemical / biochemical transformation?",
                { "A) No research", "B) Many studies, not fully developed", "C) Already new industrial dev.", "D) No interest" },
                1
            ),
            Question(
                "5. EU Climate Law?",
                { "A) Adopted in 2021", "B) There's no EU law", "C) Only a proposal", "D) There's a law, replaced soon" },
                0
            ),
            Question(
                "6. Important characteristics of renewable energy?",
                { "A) Limited source, big pollution", "B) High carbon footprint", "C) Less GHG, energy security, high cost", "D) Exhaustible, less GHG" },
                2
            ),
            Question(
                "7. What are the limitations of wind energy?",
                { "A) None", "B) High cost, not tech ready", "C) Reliability, noise, collisions", "D) Limited by snow/rain" },
                2
            ),
            Question(
                "8. Main characteristics of a biofuel?",
                { "A) Derived from biomass, renewable, benign", "B) Natural => always green", "C) Less GHG from non replenish", "D) Cost-effective, any source" },
                0
            )
        };

        // zone(8)
        zones = {
            Zone(80, 80, 64, 64, 0),
            Zone(700, 60, 64, 64, 1),
            Zone(100, 500, 64, 64, 2),
            Zone(600, 400, 64, 64, 3),
            Zone(400, 300, 64, 64, 4),
            Zone(720, 500, 64, 64, 5),
            Zone(200, 550, 64, 64, 6),
            Zone(520, 220, 64, 64, 7)
        };

        // obstacole
        obstacles = {
            Obstacle(200, 200, 64, 64, wallTexture),
            Obstacle(500, 500, 64, 64, wallTexture),
            Obstacle(300, 400, 64, 64, wallTexture),
            Obstacle(600, 200, 64, 64, wallTexture),
            Obstacle(320, 220, 64, 64, wallTexture),
            Obstacle(400, 100, 64, 64, wallTexture),
            Obstacle(600, 550, 64, 64, wallTexture)
        };

        // item
        items = {
            Item(120, 150, 48, 48, potionTexture, "Potion"),
            Item(460, 120, 48, 48, gemTexture, "Gem"),
            Item(320, 320, 48, 48, speedTexture, "SpeedBoost"),
            Item(200, 500, 48, 48, potionTexture, "Potion"),
            Item(340, 260, 48, 48, gemTexture, "Gem"),
            Item(530, 350, 48, 48, speedTexture, "SpeedBoost"),
            Item(700, 300, 48, 48, gemTexture, "Gem"),
            Item(100, 300, 48, 48, gemTexture, "Gem"),
            Item(750, 200, 48, 48, gemTexture, "Gem")
        };

        // enemies (4)
        enemies = {
            Enemy(100, 100, 2.f, MOVE_HORIZONTAL),
            Enemy(600, 200, 2.f, MOVE_VERTICAL),
            Enemy(400, 450, 3.f, MOVE_RANDOM),
            Enemy(220, 100, 2.5f, MOVE_PATROL)
        };

        // 1) Repoziționare dacă e creat în interiorul unui obstacol
        auto CollidesWithObstacle = [&](float ex, float ey) {
            Rectangle eRect = {ex, ey, (float)PLAYER_SIZE, (float)PLAYER_SIZE};
            for (auto &obs : obstacles) {
                if (obs.active) {
                    Rectangle obsRect = {obs.x, obs.y, obs.width, obs.height};
                    if (CheckCollisionRecs(eRect, obsRect)) {
                        return true;
                    }
                }
            }
            return false;
        };

        for (auto &enemy : enemies) {
            int tries = 0;
            while (CollidesWithObstacle(enemy.x, enemy.y) && tries < 50) {
                float rx = (float)GetRandomValue(0, internalWidth - PLAYER_SIZE);
                float ry = (float)GetRandomValue(0, internalHeight - PLAYER_SIZE);
                enemy.x = rx;
                enemy.y = ry;
                tries++;
            }
        }

        // quest #0 => collect 5 gems => +5 points
        Quest q1;
        q1.title = "Collect Gems";
        q1.description = "Collect 5 gems to earn +5 points.";
        q1.requiredGemCount = 5;
        q1.requiredEnemyKillCount = 0;
        q1.rewardPoints = 5;

        // quest #1 => defeat 4 enemies => +5 points
        Quest q2;
        q2.title = "Defeat Enemies";
        q2.description = "Defeat 4 enemies to earn +5 points.";
        q2.requiredGemCount = 0;
        q2.requiredEnemyKillCount = 4;
        q2.rewardPoints = 5;

        quests.push_back(q1);
        quests.push_back(q2);

        // NPC(3)
        npcs.push_back(NPC(800, 20, 1));
        npcs.push_back(NPC(20, 600, 0));
        npcs.push_back(NPC(800, 500, 1));
    }

    void ResetGame(){
        player.x = internalWidth / 2.f - PLAYER_SIZE / 2;
        player.y = internalHeight / 2.f - PLAYER_SIZE / 2;
        player.score = 0;
        player.health = 100;
        player.totalGemsCollected = 0;
        player.totalEnemiesKilled = 0;
        player.coins = 0;

        for(auto &z: zones) z.active = true;
        for(auto &it: items) it.active = true;
        for(auto &e: enemies) {
            e.active = true;
            e.health = 30;
            e.randDirectionTime = 0;
            e.direction = 0;
            e.patrolTimer = 0;
            e.stuckTime = 0;
        }
        for(auto &b: bullets) b.active = false;
        for(auto &q: quests) {
            q.isActive = false;
            q.isCompleted = false;
            q.currentGemCount = 0;
            q.currentEnemyKillCount = 0;
        }
        for(auto &n: npcs) {
            n.questGiven = false;
        }

        inQuestion = false;
        questionAnswered = false;
        answerCorrect = false;
        questionMessage.clear();
        selectedAnswer = 0;
        showCorrectAnswer = false;
        credentialsScrollY = 0;
        instructionsScrollY = 0;
        storyScrollY = 0;
        correctAnswersScrollY = 0;
    }

    // Interacțiunea cu NPC-ul
    void CheckNPCInteraction(){
        Rectangle plRect = { player.x, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
        for(auto &npc: npcs) {
            Rectangle npcRect = { npc.x, npc.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
            if(CheckCollisionRecs(plRect, npcRect)){
                if(IsKeyPressed(KEY_E)){
                    if(npc.questIndex >= 0 && npc.questIndex < (int)quests.size()){
                        Quest &q = quests[npc.questIndex];
                        if(!npc.questGiven){
                            q.isActive = true;
                            npc.questGiven = true;
                        } else {
                            if(q.isCompleted && q.rewardPoints > 0){
                                AddScore(q.rewardPoints);
                                player.coins++;
                                q.rewardPoints = 0;
                            }
                        }
                    }
                }
            }
        }
    }

    void UpdateQuestProgress(){
        for(auto &q: quests){
            if(q.isActive && !q.isCompleted){
                q.UpdateProgress(player.totalGemsCollected, player.totalEnemiesKilled);
            }
        }
    }

    void DrawQuestStatus(){
        int startY = 130;
        for(auto &q: quests){
            if(q.isActive){
                std::string line = q.title + " : ";
                if(!q.isCompleted) line += "In Progress";
                else line += "Completed!";
                DrawCustomText(line.c_str(), 10, (float)startY, 18, 2, WHITE);
                startY += 25;
            }
        }
    }

    bool CheckCollisionZone(const Zone &z){
        Rectangle plRect = { player.x, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
        Rectangle zRect = { z.x, z.y, z.width, z.height };
        return CheckCollisionRecs(plRect, zRect);
    }
    bool CheckCollisionItem(const Item &it){
        Rectangle plRect = { player.x, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
        Rectangle itRect = { it.x, it.y, it.width * ITEM_SCALE, it.height * ITEM_SCALE };
        return CheckCollisionRecs(plRect, itRect);
    }

    bool CheckCollisionEnemy(const Enemy &e){
        Rectangle plRect = { player.x, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
        Rectangle enRect = { e.x, e.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
        return CheckCollisionRecs(plRect, enRect);
    }
    bool CheckCollisionBulletEnemy(const Bullet &b, const Enemy &e){
        Rectangle bRect = { b.x, b.y, (float)b.width, (float)b.height };
        Rectangle enRect = { e.x, e.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
        return CheckCollisionRecs(bRect, enRect);
    }

    void DrawHealthBar(){
        float barWidth = 120.f;
        float ratio = (float)player.health / 100.f;
        DrawRectangle(10, 50, (int)barWidth, 15, RED);
        DrawRectangle(10, 50, (int)(barWidth * ratio), 15, GREEN);
        DrawRectangleLines(10, 50, (int)barWidth, 15, BLACK);
        DrawCustomText(TextFormat("%d HP", player.health),
                       10, 70, 18, 2, WHITE);
    }

    // Desenează monedele și scorul numeric
    void DrawScore(){
        for(int i = 0; i < player.coins; i++){
            DrawTextureEx(coinTexture,
                          { (float)(10 + i * (24 * COIN_SCALE + 4)), 10 },
                          0.0f, COIN_SCALE, WHITE);
        }
        DrawCustomText(TextFormat("Score: %d", player.score),
                       10, 100, 18, 2, GOLD);
    }

    void DrawMiniMap(){
        int mapX = internalWidth - 180;
        int mapY = 20;
        int mapW = 140, mapH = 120;
        DrawRectangle(mapX, mapY, mapW, mapH, DARKGRAY);
        DrawRectangleLines(mapX, mapY, mapW, mapH, WHITE);

        float px = mapX + (player.x / (float)internalWidth) * mapW;
        float py = mapY + (player.y / (float)internalHeight) * mapH;
        DrawCircle((int)px, (int)py, 3, BLUE);

        for(auto &z: zones){
            if(z.active){
                float zx = mapX + (z.x / (float)internalWidth) * mapW;
                float zy = mapY + (z.y / (float)internalHeight) * mapH;
                DrawCircle((int)zx, (int)zy, 2, GOLD);
            }
        }
        for(auto &o: obstacles){
            if(o.active){
                float ox = mapX + (o.x / (float)internalWidth) * mapW;
                float oy = mapY + (o.y / (float)internalHeight) * mapW;
                DrawRectangle((int)ox, (int)oy, 3, 3, BROWN);
            }
        }
        for(auto &e: enemies){
            if(e.active){
                float ex = mapX + (e.x / (float)internalWidth) * mapW;
                float ey = mapY + (e.y / (float)internalHeight) * mapH;
                DrawCircle((int)ex, (int)ey, 2, RED);
            }
        }
        for(auto &n: npcs){
            float nx = mapX + (n.x / (float)internalWidth) * mapW;
            float ny = mapY + (n.y / (float)internalHeight) * mapH;
            DrawCircle((int)nx, (int)ny, 2, PURPLE);
        }
    }

    void HandleInput(float dt){
        float oldX = player.x;
        float oldY = player.y;

        if(IsKeyDown(KEY_RIGHT)) player.x += player.speed;
        if(IsKeyDown(KEY_LEFT))  player.x -= player.speed;
        if(IsKeyDown(KEY_UP))    player.y -= player.speed;
        if(IsKeyDown(KEY_DOWN))  player.y += player.speed;

        player.KeepInBounds(internalWidth, internalHeight);

        for(auto &obs: obstacles){
            if(!obs.active) continue;
            Rectangle pRect = { player.x, player.y, (float)PLAYER_SIZE, (float)PLAYER_SIZE };
            Rectangle obsRect= { obs.x, obs.y, obs.width, obs.height };
            if(CheckCollisionRecs(pRect, obsRect)){
                player.x = oldX;
                player.y = oldY;
                break;
            }
        }

        for(auto &it: items){
            if(it.active && CheckCollisionItem(it)){
                it.active = false;
                if(it.itemName == "Potion"){
                    player.health += 20;
                    if(player.health > 100) player.health = 100;
                } else if(it.itemName == "Gem"){
                    AddScore(2);
                    player.totalGemsCollected++;
                } else if(it.itemName == "SpeedBoost"){
                    player.speed += 1.f;
                }
            }
        }

        if(IsKeyPressed(KEY_SPACE)){
            for(auto &b: bullets){
                if(!b.active){
                    b.active = true;
                    b.x = player.x + PLAYER_SIZE / 2 - b.width / 2;
                    b.y = player.y + PLAYER_SIZE / 2 - b.height / 2;
                    if(IsKeyDown(KEY_UP)) b.direction = 0;
                    else if(IsKeyDown(KEY_DOWN)) b.direction = 1;
                    else if(IsKeyDown(KEY_LEFT)) b.direction = 2;
                    else b.direction = 3;
                    break;
                }
            }
        }
    }

    void UpdateGame(float dt){
        if(!inQuestion && !paused){
            HandleInput(dt);

            for(auto &z: zones){
                if(z.active && CheckCollisionZone(z)){
                    z.active = false;
                    inQuestion = true;
                    questionAnswered = false;
                    answerCorrect = false;
                    questionMessage.clear();
                    currentQuestion = questions[z.questionIndex];
                    selectedAnswer = 0;
                    showCorrectAnswer = false;
                    break;
                }
            }

            for(auto &e: enemies){
                if(e.active && e.health > 0){
                    e.Update(dt, internalWidth, internalHeight, obstacles);
                    // Dacă inamicul se atinge de jucător, scade HP
                    if(CheckCollisionEnemy(e)){
                        player.health--;
                        if(player.health < 0) player.health = 0;
                    }
                }
            }

            for(auto &b: bullets){
                if(b.active){
                    b.Update(dt, internalWidth, internalHeight, obstacles);
                    for(auto &e: enemies){
                        if(e.active && e.health > 0){
                            if(CheckCollisionBulletEnemy(b, e)){
                                e.health -= 10;
                                b.active = false;
                                if(e.health <= 0){
                                    e.active = false;
                                    player.totalEnemiesKilled++;
                                }
                                break;
                            }
                        }
                    }
                }
            }

            CheckNPCInteraction();
            UpdateQuestProgress();

        } else if(inQuestion){
            // Tasta C pentru a afișa/ascunde răspunsul corect
            if(IsKeyPressed(KEY_C)){
                showCorrectAnswer = !showCorrectAnswer;
            }
            if(!questionAnswered){
                if(IsKeyPressed(KEY_UP))   selectedAnswer = (selectedAnswer - 1 + 4) % 4;
                if(IsKeyPressed(KEY_DOWN)) selectedAnswer = (selectedAnswer + 1) % 4;
                if(IsKeyPressed(KEY_ENTER)){
                    questionAnswered = true;
                    answerCorrect = (selectedAnswer == currentQuestion.correctIndex);
                    if(answerCorrect){
                        AddScore(10);
                        player.coins++;
                        questionMessage = "Correct answer! +10 points (+1 coin)";
                    } else {
                        SubScore(5);
                        questionMessage = "Wrong answer! -5 points";
                        player.health -= 10;
                        if(player.health < 0) player.health = 0;
                    }
                }
            } else {
                if(IsKeyPressed(KEY_ENTER)){
                    inQuestion = false;
                }
            }
        }

        if(currentScreen == SCREEN_GAME && IsKeyPressed(KEY_P)){
            paused = !paused;
        }
        if(paused){
            UpdatePauseMenu();
        }

        if(player.health <= 0){
            currentScreen = SCREEN_END;
        }

        bool anyZone = false;
        for(auto &z: zones){
            if(z.active){ anyZone = true; break; }
        }
        if(!anyZone && !inQuestion){
            if(player.health > 0) currentScreen = SCREEN_WIN;
            else currentScreen = SCREEN_END;
        }
    }

    void DrawQuestionOverlay(){
        int ox = 50, oy = 50;
        int w = internalWidth - 100, h = internalHeight - 100;
        DrawRectangle(ox, oy, w, h, Color{0, 0, 0, 220});
        DrawRectangleLines(ox, oy, w, h, RAYWHITE);

        int textY = oy + 10;
        DrawCustomText("Scavenger Hunt - Question", (float)(ox + 10), (float)textY,
                       28, 2, GOLD);
        textY += 40;

        DrawCustomText(currentQuestion.prompt.c_str(), (float)(ox + 10), (float)textY, 20, 2, RAYWHITE);
        textY += 40;

        for(int i = 0; i < 4; i++){
            Color c = (i == selectedAnswer) ? YELLOW : WHITE;
            std::string answerText = currentQuestion.answers[i];
            if(showCorrectAnswer && i == currentQuestion.correctIndex) {
                answerText += " (Correct)";
            }
            DrawCustomText(answerText.c_str(), (float)(ox + 30), (float)textY, 20, 2, c);
            textY += 30;
        }
        DrawCustomText("Press C to show/hide correct answer", (float)(ox + 10), (float)(oy + h - 110), 20, 2, YELLOW);
        if(questionAnswered){
            Color msgColor = answerCorrect ? GREEN : RED;
            DrawCustomText(questionMessage.c_str(), (float)(ox + 10), (float)(oy + h - 80), 24, 2, msgColor);
            DrawCustomText("Press ENTER to continue.", (float)(ox + 10), (float)(oy + h - 50), 20, 2, YELLOW);
        }
    }

    void DrawGameScreen(){
        BeginTextureMode(target);
        ClearBackground(BLACK);

        switch(currentScreen){
            case SCREEN_MENU:{
                DrawCustomText("SCAVENGER HUNT - MAIN MENU",
                               100, 50, (float)bigFontSize, 2, WHITE);
                DrawCustomText("1) START GAME", 100, 120, (float)normalFontSize, 2, YELLOW);
                DrawCustomText("2) INSTRUCTIONS", 100, 160, (float)normalFontSize, 2, YELLOW);
                DrawCustomText("3) CREDENTIALS", 100, 200, (float)normalFontSize, 2, YELLOW);
                DrawCustomText("4) STORY", 100, 240, (float)normalFontSize, 2, YELLOW);
                DrawCustomText("5) CORRECT ANSWERS", 100, 280, (float)normalFontSize, 2, YELLOW);
                DrawCustomText("6) EXIT", 100, 320, (float)normalFontSize, 2, YELLOW);
                DrawCustomText(TextFormat("High Score: %d", highScore), 100, 360, (float)normalFontSize, 2, GOLD);
            } break;

            case SCREEN_INSTRUCTIONS:{
                DrawCustomText("INSTRUCTIONS:", 50, 50, (float)contentBigFontSize, 2, WHITE);
                int sY = 100;
                int sH = internalHeight - 120;
                BeginScissorMode(50, sY, internalWidth - 100, sH + 200);
                Rectangle rec = {
                    50.0f, (float)sY + instructionsScrollY,
                    (float)(internalWidth - 100), (float)(sH + 200)
                };
                DrawTextRecCustom(customFont, instructionsText.c_str(),
                                  rec, (float)contentFontSize, 2.0f, true, WHITE);
                EndScissorMode();
                DrawCustomText("Press ENTER or BACKSPACE to return to menu.",
                               50, (float)internalHeight - 40, (float)contentFontSize, 2, YELLOW);
            } break;

            case SCREEN_CREDENTIALS:{
                DrawCustomText("PROJECT CREDITS:", 50, 20, (float)contentBigFontSize, 2, WHITE);
                int sY = 70;
                int sH = internalHeight - 120;
                BeginScissorMode(50, sY, internalWidth - 100, sH + 200);
                Rectangle rec = {
                    50.0f, (float)sY + credentialsScrollY,
                    (float)(internalWidth - 100), (float)(sH + 200)
                };
                DrawTextRecCustom(customFont, credText.c_str(),
                                  rec, (float)contentFontSize, 2.0f, true, RAYWHITE);
                EndScissorMode();
                DrawCustomText("Press ENTER to return to menu.",
                               50, (float)internalHeight - 40, (float)contentFontSize, 2, YELLOW);
            } break;

            case SCREEN_STORY:{
                DrawCustomText("STORY:", 50, 50, (float)contentBigFontSize, 2, WHITE);
                int sY = 100;
                int sH = internalHeight - 120;
                BeginScissorMode(50, sY, internalWidth - 100, sH + 200);
                Rectangle rec = {
                    50.0f, (float)sY + storyScrollY,
                    (float)(internalWidth - 100), (float)(sH + 200)
                };
                DrawTextRecCustom(customFont, storyText.c_str(),
                                  rec, (float)contentFontSize, 2.0f, true, WHITE);
                EndScissorMode();
                DrawCustomText("Press ENTER or BACKSPACE to return to menu.",
                               50, (float)internalHeight - 40, (float)contentFontSize, 2, YELLOW);
            } break;

            case SCREEN_GAME:{
                map->Draw();
                for(auto &o: obstacles) o.Draw();
                for(auto &it: items) it.Draw();
                for(auto &z: zones) z.Draw();
                for(auto &e: enemies) e.Draw();
                for(auto &n: npcs) n.Draw(npcWizardTexture);
                for(auto &b: bullets) b.Draw();
                player.Draw();
                DrawScore();
                DrawHealthBar();
                DrawMiniMap();
                DrawQuestStatus();
                DrawCustomText("Press E to interact with NPCs", 10, internalHeight - 30, 18, 2, LIGHTGRAY);
                if(inQuestion){
                    DrawQuestionOverlay();
                }
                if(paused){
                    DrawPauseMenu();
                }
            } break;

            case SCREEN_WIN:{
                DrawCustomText("YOU WIN!", 100, 200, 40, 2, GREEN);
                DrawCustomText(TextFormat("Final Score: %d", player.score),
                               100, 260, 25, 2, GREEN);
                DrawCustomText(TextFormat("Remaining HP: %d", player.health),
                               100, 310, 25, 2, GREEN);
                DrawCustomText(TextFormat("Coins collected: %d", player.coins),
                               100, 360, 25, 2, GREEN);
                DrawCustomText("Press ENTER to return to MENU.",
                               100, 410, 25, 2, YELLOW);
            } break;

            case SCREEN_END:{
                DrawCustomText("GAME OVER!", 100, 200, 40, 2, RED);
                DrawCustomText("You have finished or HP = 0.",
                               100, 260, 25, 2, RED);
                DrawCustomText(TextFormat("Final Score: %d", player.score),
                               100, 310, 25, 2, RED);
                DrawCustomText(TextFormat("Final HP: %d", player.health),
                               100, 360, 25, 2, RED);
                DrawCustomText(TextFormat("Coins collected: %d", player.coins),
                               100, 410, 25, 2, RED);
                DrawCustomText("Press ENTER to return to MENU, or ESC to EXIT.",
                               100, 460, 25, 2, YELLOW);
            } break;

            case SCREEN_CORRECT_ANSWERS:{
                UpdateCorrectAnswersScroll();
                int sY = 70;
                int sH = internalHeight - 120;
                BeginScissorMode(50, sY, internalWidth - 100, sH + 200);
                int yPos = sY + correctAnswersScrollY;
                int questionFontSize = 28;
                int answerFontSize = 24;
                for(int i = 0; i < (int)questions.size(); i++){
                    std::string qLine = questions[i].prompt;
                    std::string aLine = TextFormat("Correct Answer: %s", questions[i].answers[questions[i].correctIndex].c_str());
                    DrawCustomText(qLine.c_str(), 50, (float)yPos, questionFontSize, 2, WHITE);
                    yPos += questionFontSize + 10;
                    DrawCustomText(aLine.c_str(), 70, (float)yPos, answerFontSize, 2, GREEN);
                    yPos += answerFontSize + 20;
                }
                EndScissorMode();
                DrawCustomText("Press ENTER or BACKSPACE to return to menu.", 50, (float)(internalHeight - 40), 18, 2, YELLOW);
            } break;
        }

        EndTextureMode();
    }

    // -------------------------------------------------------------------------
    // UpdateScreenLogic
    // -------------------------------------------------------------------------
    void UpdateScreenLogic(){
        if(currentScreen == SCREEN_MENU){
            if(IsKeyPressed(KEY_ONE)){
                ResetGame();
                currentScreen = SCREEN_GAME;
            }
            else if(IsKeyPressed(KEY_TWO)){
                currentScreen = SCREEN_INSTRUCTIONS;
            }
            else if(IsKeyPressed(KEY_THREE)){
                currentScreen = SCREEN_CREDENTIALS;
                credentialsScrollY = 0;
            }
            else if(IsKeyPressed(KEY_FOUR)){
                currentScreen = SCREEN_STORY;
                storyScrollY = 0;
            }
            else if(IsKeyPressed(KEY_FIVE)){
                currentScreen = SCREEN_CORRECT_ANSWERS;
                correctAnswersScrollY = 0;
            }
            else if(IsKeyPressed(KEY_SIX) || IsKeyPressed(KEY_ESCAPE)){
                CloseWindow();
            }
        }
        else if(currentScreen == SCREEN_INSTRUCTIONS){
            UpdateInstructionsScroll();
            if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)){
                currentScreen = SCREEN_MENU;
            }
        }
        else if(currentScreen == SCREEN_CREDENTIALS){
            UpdateCredentialsScroll();
            if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)){
                currentScreen = SCREEN_MENU;
            }
        }
        else if(currentScreen == SCREEN_STORY){
            UpdateStoryScroll();
            if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)){
                currentScreen = SCREEN_MENU;
            }
        }
        else if(currentScreen == SCREEN_CORRECT_ANSWERS){
            if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)){
                currentScreen = SCREEN_MENU;
            }
        }
        else if(currentScreen == SCREEN_WIN){
            if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_BACKSPACE)){
                ResetGame();
                currentScreen = SCREEN_MENU;
            }
        }
        else if(currentScreen == SCREEN_END){
            if(IsKeyPressed(KEY_ENTER)){
                ResetGame();
                currentScreen = SCREEN_MENU;
            }
            if(IsKeyPressed(KEY_ESCAPE)){
                CloseWindow();
            }
        }
        else if(currentScreen == SCREEN_GAME){
            UpdateGame(GetFrameTime());
        }
    }

    // -------------------------------------------------------------------------
    // Run
    // -------------------------------------------------------------------------
    void Run(){
        SetConfigFlags(FLAG_WINDOW_RESIZABLE);
        InitWindow(1280, 720, "Scavenger Hunt");
        InitAudioDevice();

        target = LoadRenderTexture(internalWidth, internalHeight);
        srand((unsigned int)time(nullptr));

        LoadResources();

        SetTargetFPS(60);
        while(!WindowShouldClose()){
            UpdateScreenLogic();
            DrawGameScreen();

            BeginDrawing();
            ClearBackground(BLACK);

            int screenW = GetScreenWidth();
            int screenH = GetScreenHeight();
            Rectangle src = { 0, 0, (float)target.texture.width, (float)-target.texture.height };
            Rectangle dst = { 0, 0, (float)screenW, (float)screenH };
            DrawTexturePro(target.texture, src, dst, { 0, 0 }, 0.0f, WHITE);

            EndDrawing();
        }

        UnloadRenderTexture(target);
        UnloadTexture(tileTex1);
        UnloadTexture(tileTex2);
        UnloadTexture(wallTexture);
        UnloadTexture(potionTexture);
        UnloadTexture(gemTexture);
        UnloadTexture(speedTexture);
        UnloadTexture(coinTexture);
        UnloadTexture(npcWizardTexture);
        UnloadFont(customFont);

        CloseAudioDevice();
        CloseWindow();
    }
};

// --------------------------------------------------
// MAIN
// --------------------------------------------------
int main(){
    Game game;
    game.Run();
    return 0;
}
