# Scavenger Hunt — 2D Educational Game

A 2D educational game built in C++ using the Raylib library. The player navigates a tile-based map, collects items, and answers educational questions to progress through the game.

---

## Tech Stack

| Component | Technology |
|---|---|
| Language | C++ |
| Graphics Library | Raylib |
| Rendering | OpenGL 2.1 |

---

## Features

- **Character movement** — smooth player navigation across a tile-based map
- **Sprite animations** — animated character and environment sprites
- **Collision system** — tile-based collision detection
- **Educational Q&A** — question-and-answer mechanism integrated into gameplay
- **Highscore tracking** — persistent highscore saved to `highscore.txt`
- **Custom text rendering** — word-wrap function for in-game text display

---

## How to Run

### Prerequisites
- [Raylib](https://www.raylib.com/) installed
- C++ compiler (g++ or MSVC)

### Compile
```bash
g++ main.cpp -o scavenger -lraylib -lopengl32 -lgdi32 -lwinmm
```

### Run
```bash
./scavenger
```
