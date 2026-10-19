# Realistic 3D Table Tennis — OpenGL / GLUT (C++)

A realistic 3D table-tennis game with authentic visuals, physics, and gameplay matching competitive table tennis:
- **Perspective**: 3D third-person camera looking down the length of the table.
- **Visuals**:
  - Dark arena spotlight lighting with soft radial vignette onto the floor.
  - Realistic wooden parquet gym floor with plank fibers and grain.
  - Tournament-grade green table with white perimeter lines and centerline.
  - Sturdy dark undercarriage with legs, crossbeams, and floor shadows.
  - Detailed net with metal clamp posts, top/bottom tape, and mesh grid.
  - Authentic red-rubber paddles with flared wood handles, neck inlay, and dynamic shadows.
  - White 3D ping pong ball with specular highlights and dynamic table shadows.
  - Big, bold vector scores ("0" on left and right) directly matching the reference visual style.
- **Physics & Gameplay**:
  - 3D ball trajectory with gravity, air resistance, and table rebound elasticity.
  - Paddle hit calculation based on contact position and paddle velocity.
  - Net collisions and edge detection.
  - Double-bounce and out-of-bounds detection.
  - Computer AI opponent with smooth tracking and realistic returns.
  - Official scoring (first to 11 points, win by 2, alternating serves every 2 points).

## Controls
- **Mouse Movement**: Move your paddle intuitively in horizontal and vertical directions
- **A / D** or **Left / Right Arrow**: Move paddle left / right
- **W / S** or **Up / Down Arrow**: Adjust paddle height
- **SPACE** or **Left Mouse Click**: Serve the ball / Smash / Restart after match
- **R**: Restart game
- **ESC**: Quit

## Build & Run (macOS)

Compile with `clang++`:
```bash
clang++ -std=c++17 main.cpp -o table_tennis -framework OpenGL -framework GLUT
```

Run:
```bash
./table_tennis
```
