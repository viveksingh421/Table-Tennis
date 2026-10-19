# Realistic 3D Table Tennis — OpenGL / GLUT (C++)

A realistic 3D table tennis game built in modern C++ using OpenGL and GLUT, featuring realistic 3D perspective, authentic physics, spotlit arena aesthetics, and human-like AI.

---

## Features

### 1. Visuals & Environment
- **Perspective Camera**: Third-person over-the-shoulder view looking down the length of the table.
- **Arena Spotlight**: Pure black background with a central warm spotlight pool casting smooth radial falloff on the table and floor.
- **Procedural Wood Parquet Floor**: Realistic gym wooden plank texture with plank seams, growth rings, and wood grain fibers.
- **Tournament Green Table**: 
  - Rich tournament green surface with 2cm white outer perimeter lines and a center dividing line.
  - Sturdy under-apron frame and steel legs with crossbeams and cast shadows.
- **Realistic Net**: Metal clamp posts, white top/bottom tape bands, and semi-transparent dark mesh grid.
- **Paddles**:
  - Detailed circular blades with red rubber face and rim edge tape.
  - Flared wooden handles with center grip inlays.
  - Dynamic paddle tilting and forward swing animations.
  - Realistic drop shadows projected on the table surface.
- **Ball & Dynamic Shadows**:
  - White 3D sphere with specular shine.
  - Dynamic table shadow that expands and fades based on height for depth perception.
- **Bold Modern HUD**: Large, bold vector digits on the left (Player) and right (Computer) matching tournament scoreboards.

### 2. Physics Engine
- **Trajectory Calculation**: Mathematically solves 3D parabolic arcs under gravity, ensuring realistic clearances over the net.
- **Paddle Dynamics**: Contact point on the paddle and horizontal paddle velocity influence the return angle and speed.
- **Table Rebound**: Realistic vertical bounce restitution and forward momentum retention.
- **Net & Boundary Collisions**: Net tape clipping, double-bounce detection, and off-table out-of-bounds detection.

### 3. Human-like Computer AI
- **Human Reaction Delay**: 100–200 ms reaction threshold before adjusting to the player's shot.
- **Physical Reach Limit**: Moves with human lateral speed; sharp corner shots and fast smashes can beat the AI for clean winners.
- **Unforced Errors**:
  - Net faults (clipping the tape and falling short).
  - Long out-of-bounds (overhitting past the baseline).
  - Wide out-of-bounds (slicing off the side).
  - Error probability scales with rally pressure and when stretching for wide shots.

### 4. Official Match Rules
- **Game to 11**: First player to reach 11 points wins (must win by at least 2 points).
- **Service Alternation**: Serves alternate between Player and Computer every 2 points.
- **Scoring**: Points awarded on double-bounces, net failures, or out-of-bounds shots.

---

## Controls

| Action | Control |
| :--- | :--- |
| **Move Paddle** | **Mouse** (Smooth horizontal & vertical tracking) |
| **Move Paddle (Keyboard)** | **A / D** or **Left / Right Arrow** (Horizontal) |
| **Adjust Paddle Height** | **W / S** or **Up / Down Arrow** (Vertical) |
| **Serve / Smash** | **SPACE** or **Left Mouse Click** |
| **Increase Ball Speed** | **+** or **=** |
| **Decrease Ball Speed** | **-** or **_** |
| **Restart Match** | **R** |
| **Quit** | **ESC** |

---

## Build & Run (macOS)

### Prerequisites
Make sure Xcode Command Line Tools are installed:
```bash
xcode-select --install
```

### Compile
Compile using Apple's Clang compiler with OpenGL and GLUT frameworks:
```bash
clang++ -std=c++17 main.cpp -o table_tennis -framework OpenGL -framework GLUT
```

### Run
Launch the game binary:
```bash
./table_tennis
```

---

## Project Structure

```
TableTennis_OpenGL/
├── main.cpp         # Complete 3D game logic, physics solver, AI, and OpenGL rendering
├── README.md        # Documentation, setup guide, and controls
├── .gitignore       # Build output and system files filter
└── table_tennis     # Compiled executable
```
