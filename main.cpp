#define GL_SILENCE_DEPRECATION
#include <GLUT/glut.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>

// Window dimensions
int windowWidth = 1000;
int windowHeight = 650;

// Game State
enum GameState {
    STATE_SERVE,
    STATE_RALLY,
    STATE_POINT_SCORED,
    STATE_GAME_OVER
};
GameState gameState = STATE_SERVE;

// Table Dimensions (in OpenGL 3D units)
const float TABLE_WIDTH  = 2.40f;   // X-axis (-1.20 to +1.20)
const float TABLE_LENGTH = 4.40f;   // Z-axis (-2.20 to +2.20)
const float TABLE_SURFACE_Y = 0.0f; // Table top surface height
const float FLOOR_Y = -1.25f;       // Wooden floor height
const float NET_HEIGHT = 0.26f;     // Height of net above table
const float NET_OVERHANG = 0.18f;   // Net extends past table edges
const float BALL_RADIUS = 0.045f;
const float PADDLE_RADIUS = 0.23f;
const float GRAVITY = 0.0034f;

// Speed Control (multiplier for fast, snappy table tennis action)
float ballSpeedMultiplier = 1.65f;

// Score and Match
int playerScore = 0;
int computerScore = 0;
const int WIN_SCORE = 11;
int server = 1; // 1 = player serves, 2 = computer serves
int rallyCount = 0;
std::string statusMessage = "CLICK OR PRESS SPACE TO SERVE";
int messageTimer = 0;

// Ball Physics
float ballX = 0.0f;
float ballY = TABLE_SURFACE_Y + 0.18f;
float ballZ = 1.6f;
float ballVX = 0.0f;
float ballVY = 0.0f;
float ballVZ = 0.0f;
float prevBallZ = 1.6f;
bool ballBouncedPlayerSide = false;
bool ballBouncedCompSide = false;
int lastHitter = 0; // 1 = player, 2 = computer

// Player Paddle
float playerX = 0.0f;
float playerY = TABLE_SURFACE_Y + 0.22f;
float playerZ = 2.05f;
float playerPrevX = 0.0f;
float playerVelocityX = 0.0f;
float playerTargetX = 0.0f;
float playerTargetY = TABLE_SURFACE_Y + 0.22f;
float playerTilt = -0.15f;
float playerSwingAnim = 0.0f;

// Computer Paddle (Realistic Human-like AI)
float computerX = 0.0f;
float computerY = TABLE_SURFACE_Y + 0.22f;
float computerZ = -2.05f;
float computerTargetX = 0.0f;
float computerTargetY = TABLE_SURFACE_Y + 0.22f;
float computerSwingAnim = 0.0f;
int aiReactionCountdown = 0;     // Human reaction delay
float compErrorNoiseX = 0.0f;     // Subtle human positioning imperfection

// Input tracking
bool keys[256] = {false};
bool specialKeys[256] = {false};
bool mouseControlActive = true;

// Floor Texture ID
GLuint floorTextureId = 0;

// Function prototypes
void setupFloorTexture();
void resetServe(int nextServer);
void triggerPointScored(int winnerId, const std::string& reason);
void launchShotToTarget(float fromX, float fromY, float fromZ,
                        float targetX, float targetZ,
                        float apexClearance, float desiredDurationFrames);

// Generate procedural realistic wood plank texture
void setupFloorTexture() {
    const int TEX_SIZE = 512;
    std::vector<unsigned char> texData(TEX_SIZE * TEX_SIZE * 3);

    for (int y = 0; y < TEX_SIZE; ++y) {
        for (int x = 0; x < TEX_SIZE; ++x) {
            int plankWidth = TEX_SIZE / 8;
            int plankIndex = x / plankWidth;
            int xInPlank = x % plankWidth;

            float plankR = 0.82f + 0.05f * std::sin(plankIndex * 2.3f);
            float plankG = 0.68f + 0.04f * std::sin(plankIndex * 3.7f);
            float plankB = 0.48f + 0.03f * std::sin(plankIndex * 1.9f);

            float grain1 = std::sin(x * 1.2f + std::sin(y * 0.06f) * 6.0f);
            float grain2 = std::sin(x * 3.5f + std::cos(y * 0.03f) * 12.0f);
            float grain = 0.92f + 0.08f * (0.6f * grain1 + 0.4f * grain2);

            float rings = 0.96f + 0.04f * std::sin(std::sqrt((float)((x % plankWidth - plankWidth/2)*(x % plankWidth - plankWidth/2) + (y % 128 - 64)*(y % 128 - 64))) * 0.4f);

            float seam = 1.0f;
            if (xInPlank <= 2 || xInPlank >= plankWidth - 2) {
                seam = 0.45f;
            }

            float finalR = std::min(1.0f, std::max(0.0f, plankR * grain * rings * seam));
            float finalG = std::min(1.0f, std::max(0.0f, plankG * grain * rings * seam));
            float finalB = std::min(1.0f, std::max(0.0f, plankB * grain * rings * seam));

            int pixelIdx = (y * TEX_SIZE + x) * 3;
            texData[pixelIdx + 0] = static_cast<unsigned char>(finalR * 255.0f);
            texData[pixelIdx + 1] = static_cast<unsigned char>(finalG * 255.0f);
            texData[pixelIdx + 2] = static_cast<unsigned char>(finalB * 255.0f);
        }
    }

    glGenTextures(1, &floorTextureId);
    glBindTexture(GL_TEXTURE_2D, floorTextureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, TEX_SIZE, TEX_SIZE, 0, GL_RGB, GL_UNSIGNED_BYTE, texData.data());
}

// Fast Trajectory Physics Engine
void launchShotToTarget(float fromX, float fromY, float fromZ,
                        float targetX, float targetZ,
                        float apexClearance, float desiredDurationFrames) {
    ballX = fromX;
    ballY = fromY;
    ballZ = fromZ;
    prevBallZ = fromZ;

    float deltaZ = targetZ - fromZ;
    float effectiveDuration = desiredDurationFrames / ballSpeedMultiplier;
    float timeLanding = std::max(15.0f, effectiveDuration);

    float ratioToNet = (0.0f - fromZ) / deltaZ;
    float timeNet = std::max(5.0f, ratioToNet * timeLanding);

    float desiredNetY = TABLE_SURFACE_Y + NET_HEIGHT + apexClearance;
    float requiredVY = (desiredNetY - fromY + 0.5f * GRAVITY * (timeNet * timeNet)) / timeNet;

    float discriminant = requiredVY * requiredVY + 2.0f * GRAVITY * fromY;
    if (discriminant < 0.0f) discriminant = 0.0f;
    float exactLandTime = (requiredVY + std::sqrt(discriminant)) / GRAVITY;
    if (exactLandTime < 13.0f) exactLandTime = 13.0f;

    ballVY = requiredVY;
    ballVZ = deltaZ / exactLandTime;
    ballVX = (targetX - fromX) / exactLandTime;
}

// Reset ball for serve
void resetServe(int nextServer) {
    server = nextServer;
    lastHitter = 0;
    ballBouncedPlayerSide = false;
    ballBouncedCompSide = false;
    ballVX = 0.0f;
    ballVY = 0.0f;
    ballVZ = 0.0f;
    rallyCount = 0;
    playerSwingAnim = 0.0f;
    computerSwingAnim = 0.0f;
    aiReactionCountdown = 0;

    if (server == 1) {
        ballX = playerX * 0.6f;
        ballY = TABLE_SURFACE_Y + 0.20f;
        ballZ = 1.60f;
        statusMessage = "CLICK OR PRESS SPACE TO SERVE";
    } else {
        ballX = computerX * 0.6f;
        ballY = TABLE_SURFACE_Y + 0.20f;
        ballZ = -1.60f;
        statusMessage = "COMPUTER IS SERVING...";
    }
    prevBallZ = ballZ;
    gameState = STATE_SERVE;
}

void triggerPointScored(int winnerId, const std::string& reason) {
    if (gameState == STATE_POINT_SCORED || gameState == STATE_GAME_OVER) return;

    if (winnerId == 1) {
        playerScore++;
        statusMessage = "POINT FOR YOU! " + reason;
    } else {
        computerScore++;
        statusMessage = "POINT FOR COMPUTER! " + reason;
    }

    if (playerScore >= WIN_SCORE && (playerScore - computerScore) >= 2) {
        gameState = STATE_GAME_OVER;
        statusMessage = "YOU WIN THE MATCH! PRESS SPACE TO PLAY AGAIN";
        return;
    } else if (computerScore >= WIN_SCORE && (computerScore - playerScore) >= 2) {
        gameState = STATE_GAME_OVER;
        statusMessage = "COMPUTER WINS THE MATCH! PRESS SPACE TO PLAY AGAIN";
        return;
    }

    gameState = STATE_POINT_SCORED;
    messageTimer = 75; // ~1.2s delay

    int totalPoints = playerScore + computerScore;
    server = ((totalPoints / 2) % 2 == 0) ? 1 : 2;
}

// Draw a smooth shaded 3D Box / Cuboid
void draw3DBox(float x, float y, float z, float sx, float sy, float sz,
               float r, float g, float b) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(sx, sy, sz);
    glColor3f(r, g, b);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);

    glNormal3f(0.0f, 0.0f, -1.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);

    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);

    glNormal3f(0.0f, -1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);

    glNormal3f(1.0f, 0.0f, 0.0f);
    glVertex3f( 0.5f, -0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f, -0.5f);
    glVertex3f( 0.5f,  0.5f,  0.5f);
    glVertex3f( 0.5f, -0.5f,  0.5f);

    glNormal3f(-1.0f, 0.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, -0.5f);
    glVertex3f(-0.5f, -0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f,  0.5f);
    glVertex3f(-0.5f,  0.5f, -0.5f);
    glEnd();

    glPopMatrix();
}

// Render the spotlit wood floor matching the screenshot
void drawFloor() {
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, floorTextureId);

    const int GRID = 40;
    const float FLOOR_SIZE_X = 14.0f;
    const float FLOOR_SIZE_Z = 16.0f;
    const float SPOT_RADIUS_X = 4.2f;
    const float SPOT_RADIUS_Z = 5.2f;

    for (int iz = 0; iz < GRID; ++iz) {
        float z0 = -FLOOR_SIZE_Z * 0.5f + (float)iz / GRID * FLOOR_SIZE_Z;
        float z1 = -FLOOR_SIZE_Z * 0.5f + (float)(iz + 1) / GRID * FLOOR_SIZE_Z;

        glBegin(GL_QUAD_STRIP);
        glNormal3f(0.0f, 1.0f, 0.0f);

        for (int ix = 0; ix <= GRID; ++ix) {
            float x = -FLOOR_SIZE_X * 0.5f + (float)ix / GRID * FLOOR_SIZE_X;

            float dist0 = (x * x) / (SPOT_RADIUS_X * SPOT_RADIUS_X) + (z0 * z0) / (SPOT_RADIUS_Z * SPOT_RADIUS_Z);
            float intensity0 = std::exp(-dist0 * 1.5f);
            if (intensity0 < 0.01f) intensity0 = 0.0f;
            glColor3f(intensity0 * 0.95f, intensity0 * 0.90f, intensity0 * 0.85f);
            glTexCoord2f(x * 1.2f, z0 * 1.2f);
            glVertex3f(x, FLOOR_Y, z0);

            float dist1 = (x * x) / (SPOT_RADIUS_X * SPOT_RADIUS_X) + (z1 * z1) / (SPOT_RADIUS_Z * SPOT_RADIUS_Z);
            float intensity1 = std::exp(-dist1 * 1.5f);
            if (intensity1 < 0.01f) intensity1 = 0.0f;
            glColor3f(intensity1 * 0.95f, intensity1 * 0.90f, intensity1 * 0.85f);
            glTexCoord2f(x * 1.2f, z1 * 1.2f);
            glVertex3f(x, FLOOR_Y, z1);
        }
        glEnd();
    }

    glDisable(GL_TEXTURE_2D);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.0f, 0.0f, 0.0f, 0.45f);
    glBegin(GL_QUADS);
    glVertex3f(-TABLE_WIDTH * 0.46f, FLOOR_Y + 0.001f, -TABLE_LENGTH * 0.46f);
    glVertex3f( TABLE_WIDTH * 0.46f, FLOOR_Y + 0.001f, -TABLE_LENGTH * 0.46f);
    glVertex3f( TABLE_WIDTH * 0.46f, FLOOR_Y + 0.001f,  TABLE_LENGTH * 0.46f);
    glVertex3f(-TABLE_WIDTH * 0.46f, FLOOR_Y + 0.001f,  TABLE_LENGTH * 0.46f);
    glEnd();
    glDisable(GL_BLEND);
}

// Render the complete realistic table tennis table
void drawTable() {
    float halfW = TABLE_WIDTH * 0.5f;
    float halfL = TABLE_LENGTH * 0.5f;
    float topThick = 0.05f;

    draw3DBox(0.0f, TABLE_SURFACE_Y - topThick - 0.04f, 0.0f,
              TABLE_WIDTH - 0.08f, 0.08f, TABLE_LENGTH - 0.08f,
              0.28f, 0.31f, 0.34f);

    draw3DBox(0.0f, TABLE_SURFACE_Y - topThick * 0.5f, 0.0f,
              TABLE_WIDTH, topThick, TABLE_LENGTH,
              0.13f, 0.43f, 0.22f);

    float lineY = TABLE_SURFACE_Y + 0.001f;
    float lineThick = 0.024f;
    glColor3f(0.96f, 0.98f, 1.0f);

    glBegin(GL_QUADS);
    glNormal3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-halfW,             lineY, -halfL);
    glVertex3f(-halfW + lineThick, lineY, -halfL);
    glVertex3f(-halfW + lineThick, lineY,  halfL);
    glVertex3f(-halfW,             lineY,  halfL);

    glVertex3f(halfW - lineThick, lineY, -halfL);
    glVertex3f(halfW,             lineY, -halfL);
    glVertex3f(halfW,             lineY,  halfL);
    glVertex3f(halfW - lineThick, lineY,  halfL);

    glVertex3f(-halfW, lineY, halfL - lineThick);
    glVertex3f( halfW, lineY, halfL - lineThick);
    glVertex3f( halfW, lineY, halfL);
    glVertex3f(-halfW, lineY, halfL);

    glVertex3f(-halfW, lineY, -halfL);
    glVertex3f( halfW, lineY, -halfL);
    glVertex3f( halfW, lineY, -halfL + lineThick);
    glVertex3f(-halfW, lineY, -halfL + lineThick);

    float centerLineHalfW = 0.006f;
    glVertex3f(-centerLineHalfW, lineY, -halfL);
    glVertex3f( centerLineHalfW, lineY, -halfL);
    glVertex3f( centerLineHalfW, lineY,  halfL);
    glVertex3f(-centerLineHalfW, lineY,  halfL);
    glEnd();

    float legX = halfW * 0.72f;
    float legZ = halfL * 0.68f;
    float legW = 0.08f;
    float legH = TABLE_SURFACE_Y - FLOOR_Y - topThick;
    float legCenterY = FLOOR_Y + legH * 0.5f;

    draw3DBox(-legX, legCenterY, -legZ, legW, legH, legW, 0.22f, 0.25f, 0.28f);
    draw3DBox( legX, legCenterY, -legZ, legW, legH, legW, 0.22f, 0.25f, 0.28f);
    draw3DBox(-legX, legCenterY,  legZ, legW, legH, legW, 0.22f, 0.25f, 0.28f);
    draw3DBox( legX, legCenterY,  legZ, legW, legH, legW, 0.22f, 0.25f, 0.28f);

    float beamH = 0.06f;
    float beamW = 0.05f;
    draw3DBox(0.0f, legCenterY - 0.15f,  legZ, legX * 2.0f, beamH, beamW, 0.20f, 0.22f, 0.25f);
    draw3DBox(0.0f, legCenterY - 0.15f, -legZ, legX * 2.0f, beamH, beamW, 0.20f, 0.22f, 0.25f);
    draw3DBox(-legX, legCenterY - 0.15f, 0.0f, beamW, beamH, legZ * 2.0f, 0.20f, 0.22f, 0.25f);
    draw3DBox( legX, legCenterY - 0.15f, 0.0f, beamW, beamH, legZ * 2.0f, 0.20f, 0.22f, 0.25f);
}

// Draw realistic net
void drawNet() {
    float netHalfW = TABLE_WIDTH * 0.5f + NET_OVERHANG;
    float postX = netHalfW;
    float postY = TABLE_SURFACE_Y + NET_HEIGHT * 0.5f;
    float postW = 0.05f;

    draw3DBox(-postX, postY, 0.0f, postW, NET_HEIGHT + 0.05f, 0.06f, 0.65f, 0.68f, 0.72f);
    draw3DBox( postX, postY, 0.0f, postW, NET_HEIGHT + 0.05f, 0.06f, 0.65f, 0.68f, 0.72f);

    draw3DBox(-postX + 0.04f, TABLE_SURFACE_Y - 0.04f, 0.0f, 0.12f, 0.05f, 0.06f, 0.45f, 0.48f, 0.52f);
    draw3DBox( postX - 0.04f, TABLE_SURFACE_Y - 0.04f, 0.0f, 0.12f, 0.05f, 0.06f, 0.45f, 0.48f, 0.52f);

    float tapeHeight = 0.022f;
    draw3DBox(0.0f, TABLE_SURFACE_Y + NET_HEIGHT - tapeHeight * 0.5f, 0.0f,
              netHalfW * 2.0f, tapeHeight, 0.015f,
              0.96f, 0.96f, 0.98f);

    draw3DBox(0.0f, TABLE_SURFACE_Y + tapeHeight * 0.5f, 0.0f,
              netHalfW * 2.0f, tapeHeight, 0.015f,
              0.90f, 0.90f, 0.92f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glColor4f(0.12f, 0.16f, 0.14f, 0.60f);
    glBegin(GL_QUADS);
    glVertex3f(-netHalfW, TABLE_SURFACE_Y + tapeHeight, 0.0f);
    glVertex3f( netHalfW, TABLE_SURFACE_Y + tapeHeight, 0.0f);
    glVertex3f( netHalfW, TABLE_SURFACE_Y + NET_HEIGHT - tapeHeight, 0.0f);
    glVertex3f(-netHalfW, TABLE_SURFACE_Y + NET_HEIGHT - tapeHeight, 0.0f);
    glEnd();

    glColor4f(0.85f, 0.88f, 0.85f, 0.35f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for (float y = TABLE_SURFACE_Y + 0.04f; y < TABLE_SURFACE_Y + NET_HEIGHT - 0.02f; y += 0.03f) {
        glVertex3f(-netHalfW, y, 0.001f);
        glVertex3f( netHalfW, y, 0.001f);
    }
    for (float x = -netHalfW + 0.04f; x < netHalfW; x += 0.04f) {
        glVertex3f(x, TABLE_SURFACE_Y + tapeHeight, 0.001f);
        glVertex3f(x, TABLE_SURFACE_Y + NET_HEIGHT - tapeHeight, 0.001f);
    }
    glEnd();

    glDisable(GL_BLEND);
}

// Draw a realistic Table Tennis Racket (Paddle)
void drawPaddleModel(float x, float y, float z, float tiltZ, float swingAnim, bool isPlayer) {
    glPushMatrix();
    glTranslatef(x, y, z - (isPlayer ? swingAnim * 0.15f : -swingAnim * 0.15f));

    float pitchAngle = (isPlayer ? -swingAnim * 25.0f : swingAnim * 25.0f);
    glRotatef(pitchAngle, 1.0f, 0.0f, 0.0f);
    glRotatef(tiltZ * 57.295f, 0.0f, 0.0f, 1.0f);

    if (y > TABLE_SURFACE_Y) {
        glPushMatrix();
        glLoadIdentity();
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.0f, 0.0f, 0.35f);

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(x, TABLE_SURFACE_Y + 0.002f, z);
        const int SEG = 18;
        for (int i = 0; i <= SEG; ++i) {
            float angle = (float)i / SEG * 2.0f * 3.14159f;
            float sx = x + std::cos(angle) * (PADDLE_RADIUS * 0.9f);
            float sz = z + std::sin(angle) * (PADDLE_RADIUS * 0.7f);
            glVertex3f(sx, TABLE_SURFACE_Y + 0.002f, sz);
        }
        glEnd();
        glDisable(GL_BLEND);
        glPopMatrix();
    }

    const int SEGMENTS = 26;
    float bladeRadius = PADDLE_RADIUS;
    float rubberThick = 0.016f;

    glColor3f(0.88f, 0.20f, 0.18f);

    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 0.0f, isPlayer ? -1.0f : 1.0f);
    glVertex3f(0.0f, 0.0f, rubberThick * 0.5f);
    for (int i = 0; i <= SEGMENTS; ++i) {
        float angle = (float)i / SEGMENTS * 2.0f * 3.14159f;
        glVertex3f(std::cos(angle) * bladeRadius, std::sin(angle) * bladeRadius, rubberThick * 0.5f);
    }
    glEnd();

    glColor3f(0.85f, 0.18f, 0.16f);
    glBegin(GL_TRIANGLE_FAN);
    glNormal3f(0.0f, 0.0f, isPlayer ? 1.0f : -1.0f);
    glVertex3f(0.0f, 0.0f, -rubberThick * 0.5f);
    for (int i = SEGMENTS; i >= 0; --i) {
        float angle = (float)i / SEGMENTS * 2.0f * 3.14159f;
        glVertex3f(std::cos(angle) * bladeRadius, std::sin(angle) * bladeRadius, -rubberThick * 0.5f);
    }
    glEnd();

    glColor3f(0.18f, 0.18f, 0.20f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= SEGMENTS; ++i) {
        float angle = (float)i / SEGMENTS * 2.0f * 3.14159f;
        float cosA = std::cos(angle);
        float sinA = std::sin(angle);
        glNormal3f(cosA, sinA, 0.0f);
        glVertex3f(cosA * bladeRadius, sinA * bladeRadius, -rubberThick * 0.5f);
        glVertex3f(cosA * bladeRadius, sinA * bladeRadius,  rubberThick * 0.5f);
    }
    glEnd();

    float handleLength = 0.22f;
    float handleWidthBottom = 0.075f;
    float handleThick = 0.032f;
    float handleY = -bladeRadius - handleLength * 0.5f + 0.04f;

    glColor3f(0.86f, 0.72f, 0.48f);
    draw3DBox(0.0f, handleY, 0.0f, handleWidthBottom, handleLength, handleThick, 0.86f, 0.72f, 0.48f);

    glColor3f(0.68f, 0.48f, 0.28f);
    draw3DBox(0.0f, handleY, handleThick * 0.51f, 0.016f, handleLength * 0.85f, 0.005f, 0.68f, 0.48f, 0.28f);

    glPopMatrix();
}

// Draw ping pong ball and dynamic shadow
void drawBall() {
    float shadowY = (ballY > TABLE_SURFACE_Y) ? (TABLE_SURFACE_Y + 0.002f) : (FLOOR_Y + 0.002f);
    float heightAboveSurface = ballY - shadowY;

    if (heightAboveSurface >= 0.0f) {
        float shadowRadius = BALL_RADIUS * (1.0f + heightAboveSurface * 0.6f);
        float shadowAlpha = std::max(0.1f, 0.55f - heightAboveSurface * 0.4f);

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.0f, 0.0f, shadowAlpha);

        glBegin(GL_TRIANGLE_FAN);
        glVertex3f(ballX, shadowY, ballZ);
        const int SEG = 20;
        for (int i = 0; i <= SEG; ++i) {
            float angle = (float)i / SEG * 2.0f * 3.14159f;
            glVertex3f(ballX + std::cos(angle) * shadowRadius,
                       shadowY,
                       ballZ + std::sin(angle) * (shadowRadius * 0.8f));
        }
        glEnd();
        glDisable(GL_BLEND);
    }

    glPushMatrix();
    glTranslatef(ballX, ballY, ballZ);

    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);

    GLfloat ballMatDiffuse[] = { 0.98f, 0.98f, 0.98f, 1.0f };
    GLfloat ballMatSpecular[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    GLfloat ballMatShininess[] = { 80.0f };
    glMaterialfv(GL_FRONT, GL_DIFFUSE, ballMatDiffuse);
    glMaterialfv(GL_FRONT, GL_SPECULAR, ballMatSpecular);
    glMaterialfv(GL_FRONT, GL_SHININESS, ballMatShininess);

    glColor3f(0.98f, 0.98f, 0.98f);
    glutSolidSphere(BALL_RADIUS, 24, 24);

    glDisable(GL_LIGHTING);
    glPopMatrix();
}

// Vector Segment Digital/Modern Number Renderer
void drawThickSegment(float x0, float y0, float x1, float y1, float thickness) {
    float dx = x1 - x0;
    float dy = y1 - y0;
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 0.0001f) return;
    float nx = -dy / len * (thickness * 0.5f);
    float ny =  dx / len * (thickness * 0.5f);

    glBegin(GL_QUADS);
    glVertex2f(x0 - nx, y0 - ny);
    glVertex2f(x0 + nx, y0 + ny);
    glVertex2f(x1 + nx, y1 + ny);
    glVertex2f(x1 - nx, y1 - ny);
    glEnd();
}

// Draw a large bold digit matching the exact style in screenshot
void drawBigDigit(float cx, float cy, float w, float h, int digit) {
    float halfW = w * 0.5f;
    float halfH = h * 0.5f;
    float thick = w * 0.22f;

    float left   = cx - halfW;
    float right  = cx + halfW;
    float top    = cy + halfH;
    float mid    = cy;
    float bottom = cy - halfH;

    bool segs[10][7] = {
        {1, 1, 1, 1, 1, 1, 0}, // 0
        {0, 1, 1, 0, 0, 0, 0}, // 1
        {1, 1, 0, 1, 1, 0, 1}, // 2
        {1, 1, 1, 1, 0, 0, 1}, // 3
        {0, 1, 1, 0, 0, 1, 1}, // 4
        {1, 0, 1, 1, 0, 1, 1}, // 5
        {1, 0, 1, 1, 1, 1, 1}, // 6
        {1, 1, 1, 0, 0, 0, 0}, // 7
        {1, 1, 1, 1, 1, 1, 1}, // 8
        {1, 1, 1, 1, 0, 1, 1}  // 9
    };

    if (digit < 0 || digit > 9) return;

    glColor3f(1.0f, 1.0f, 1.0f);

    if (digit == 0) {
        drawThickSegment(left + thick*0.5f, top, right - thick*0.5f, top, thick);
        drawThickSegment(left + thick*0.5f, bottom, right - thick*0.5f, bottom, thick);
        drawThickSegment(left, top - thick*0.5f, left, bottom + thick*0.5f, thick);
        drawThickSegment(right, top - thick*0.5f, right, bottom + thick*0.5f, thick);
        return;
    }

    if (segs[digit][0]) drawThickSegment(left, top, right, top, thick);
    if (segs[digit][1]) drawThickSegment(right, top, right, mid, thick);
    if (segs[digit][2]) drawThickSegment(right, mid, right, bottom, thick);
    if (segs[digit][3]) drawThickSegment(left, bottom, right, bottom, thick);
    if (segs[digit][4]) drawThickSegment(left, mid, left, bottom, thick);
    if (segs[digit][5]) drawThickSegment(left, top, left, mid, thick);
    if (segs[digit][6]) drawThickSegment(left, mid, right, mid, thick);
}

// Draw entire score number (0 to 99)
void drawBigScore(float cx, float cy, int score) {
    float digitW = 38.0f;
    float digitH = 65.0f;

    if (score < 10) {
        drawBigDigit(cx, cy, digitW, digitH, score);
    } else {
        int tens = score / 10;
        int ones = score % 10;
        drawBigDigit(cx - digitW * 0.65f, cy, digitW * 0.85f, digitH, tens);
        drawBigDigit(cx + digitW * 0.65f, cy, digitW * 0.85f, digitH, ones);
    }
}

// Helper to draw clean bitmap text strings
void drawBitmapString(float x, float y, void* font, const std::string& str) {
    glRasterPos2f(x, y);
    for (char c : str) {
        glutBitmapCharacter(font, c);
    }
}

// 2D HUD Overlay
void drawHUD() {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0, windowWidth, 0, windowHeight);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // Bold Big White Scores on Left and Right exactly matching screenshot
    float scoreY = windowHeight * 0.65f;
    float scoreLeftX = windowWidth * 0.08f;
    float scoreRightX = windowWidth * 0.92f;

    drawBigScore(scoreLeftX, scoreY, playerScore);
    drawBigScore(scoreRightX, scoreY, computerScore);

    // Status message in center
    if (!statusMessage.empty()) {
        int textWidth = 0;
        for (char c : statusMessage) {
            textWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_18, c);
        }
        float msgX = (windowWidth - textWidth) * 0.5f;
        float msgY = windowHeight * 0.88f;

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glColor4f(0.0f, 0.0f, 0.0f, 0.55f);
        glBegin(GL_QUADS);
        glVertex2f(msgX - 18.0f, msgY - 8.0f);
        glVertex2f(msgX + textWidth + 18.0f, msgY - 8.0f);
        glVertex2f(msgX + textWidth + 18.0f, msgY + 24.0f);
        glVertex2f(msgX - 18.0f, msgY + 24.0f);
        glEnd();
        glDisable(GL_BLEND);

        if (gameState == STATE_GAME_OVER) {
            glColor3f(1.0f, 0.85f, 0.20f);
        } else {
            glColor3f(0.95f, 0.95f, 0.98f);
        }
        drawBitmapString(msgX, msgY, GLUT_BITMAP_HELVETICA_18, statusMessage);
    }

    // Bottom controls info
    glColor3f(0.65f, 0.70f, 0.75f);
    std::string helpText = "MOUSE / ARROWS: Move   |   SPACE / CLICK: Serve / Smash   |   +/-: Speed   |   R: Restart";
    int helpWidth = 0;
    for (char c : helpText) {
        helpWidth += glutBitmapWidth(GLUT_BITMAP_HELVETICA_12, c);
    }
    drawBitmapString((windowWidth - helpWidth) * 0.5f, 22.0f, GLUT_BITMAP_HELVETICA_12, helpText);

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

// Trigger player swing & shot
void handlePlayerHitAttempt() {
    playerSwingAnim = 1.0f;

    // If serving, launch serve with fast, crisp arc
    if (gameState == STATE_SERVE && server == 1) {
        gameState = STATE_RALLY;
        lastHitter = 1;
        statusMessage = "";
        ballBouncedPlayerSide = false;
        ballBouncedCompSide = false;

        float targetX = playerX * 0.45f;
        float targetZ = -1.30f;
        launchShotToTarget(playerX, playerY + 0.04f, playerZ - 0.18f,
                           targetX, targetZ, 0.10f, 25.0f);

        // Alert computer AI with a human reaction delay
        aiReactionCountdown = 6 + (std::rand() % 8);
        compErrorNoiseX = ((std::rand() % 100) / 100.0f - 0.5f) * 0.18f;
        return;
    }

    // If ball is approaching player's hitting zone
    if (gameState == STATE_RALLY && ballVZ > 0.0f && ballZ >= 1.40f && ballZ <= (playerZ + 0.40f)) {
        float dx = ballX - playerX;
        float distH = std::abs(dx);

        if (distH <= (PADDLE_RADIUS * 1.60f) && ballY >= (TABLE_SURFACE_Y - 0.05f)) {
            lastHitter = 1;
            ballBouncedPlayerSide = false;
            ballBouncedCompSide = false;
            rallyCount++;

            float contactOffset = dx / PADDLE_RADIUS;
            float targetX = (contactOffset * 0.65f) + (playerVelocityX * 3.2f);
            targetX = std::max(-TABLE_WIDTH * 0.46f, std::min(TABLE_WIDTH * 0.46f, targetX));
            float targetZ = -1.35f - (std::abs(contactOffset) * 0.35f);

            float duration = std::max(18.0f, 23.0f - rallyCount * 0.5f);
            launchShotToTarget(ballX, ballY, playerZ - 0.15f,
                               targetX, targetZ, 0.09f, duration);

            // Give AI realistic human reaction delay on this shot
            aiReactionCountdown = 6 + (std::rand() % 8);
            compErrorNoiseX = ((std::rand() % 100) / 100.0f - 0.5f) * 0.18f;
        }
    }
}

// Realistic Computer Return with Human Errors
void handleComputerHit() {
    computerSwingAnim = 1.0f;
    lastHitter = 2;
    ballBouncedPlayerSide = false;
    ballBouncedCompSide = false;
    rallyCount++;

    // Calculate how centered the contact was
    float offCenter = std::abs(ballX - computerX) / PADDLE_RADIUS;

    // Realistic error chance: baseline 12% + up to 25% when stretching + pressure during long rally
    float errorChance = 0.12f + (offCenter * 0.25f) + std::min(0.16f, rallyCount * 0.02f);
    float roll = (std::rand() % 1000) / 1000.0f;

    if (roll < errorChance) {
        // Computer makes a human mistake!
        int errorType = std::rand() % 3;
        if (errorType == 0) {
            // Hit into the net
            float targetX = ((std::rand() % 100) / 100.0f - 0.5f) * (TABLE_WIDTH * 0.45f);
            float targetZ = 0.10f;
            launchShotToTarget(ballX, ballY, computerZ + 0.15f,
                               targetX, targetZ, -0.06f, 23.0f);
        } else if (errorType == 1) {
            // Overhit / Long out of bounds
            float targetX = ((std::rand() % 100) / 100.0f - 0.5f) * (TABLE_WIDTH * 0.65f);
            float targetZ = 2.45f; // Past table end
            launchShotToTarget(ballX, ballY, computerZ + 0.15f,
                               targetX, targetZ, 0.14f, 24.0f);
        } else {
            // Sliced wide off the side of the table
            float targetX = (ballX >= 0.0f) ? (TABLE_WIDTH * 0.58f) : (-TABLE_WIDTH * 0.58f);
            float targetZ = 1.15f;
            launchShotToTarget(ballX, ballY, computerZ + 0.15f,
                               targetX, targetZ, 0.11f, 24.0f);
        }
        return;
    }

    // Clean Return
    float targetX = ((std::rand() % 100) / 100.0f - 0.5f) * (TABLE_WIDTH * 0.66f);
    float targetZ = 1.25f + ((std::rand() % 40) / 100.0f) * 0.35f;

    float duration = std::max(19.0f, 24.0f - rallyCount * 0.4f);
    launchShotToTarget(ballX, ballY, computerZ + 0.15f,
                       targetX, targetZ, 0.10f, duration);
}

// Computer serve
void doComputerServe() {
    if (gameState != STATE_SERVE || server != 2) return;

    gameState = STATE_RALLY;
    lastHitter = 2;
    statusMessage = "";
    ballBouncedPlayerSide = false;
    ballBouncedCompSide = false;
    computerSwingAnim = 1.0f;

    float targetX = ((std::rand() % 100) / 100.0f - 0.5f) * (TABLE_WIDTH * 0.50f);
    float targetZ = 1.25f;
    launchShotToTarget(computerX, computerY + 0.04f, computerZ + 0.18f,
                       targetX, targetZ, 0.10f, 26.0f);
}

// Physics & Game Loop Update
void update(int) {
    // 1. Player Paddle Updates
    playerPrevX = playerX;

    if (keys['a'] || keys['A'] || specialKeys[GLUT_KEY_LEFT])  playerTargetX -= 0.10f;
    if (keys['d'] || keys['D'] || specialKeys[GLUT_KEY_RIGHT]) playerTargetX += 0.10f;
    if (keys['w'] || keys['W'] || specialKeys[GLUT_KEY_UP])    playerTargetY += 0.06f;
    if (keys['s'] || keys['S'] || specialKeys[GLUT_KEY_DOWN])  playerTargetY -= 0.06f;

    float maxPaddleX = TABLE_WIDTH * 0.5f + 0.20f;
    playerTargetX = std::max(-maxPaddleX, std::min(maxPaddleX, playerTargetX));
    playerTargetY = std::max(TABLE_SURFACE_Y + 0.08f, std::min(TABLE_SURFACE_Y + 0.55f, playerTargetY));

    playerX += (playerTargetX - playerX) * 0.45f;
    playerY += (playerTargetY - playerY) * 0.45f;
    playerVelocityX = playerX - playerPrevX;

    playerTilt = -0.15f + (playerX * 0.08f);

    if (playerSwingAnim > 0.0f) playerSwingAnim = std::max(0.0f, playerSwingAnim - 0.12f);
    if (computerSwingAnim > 0.0f) computerSwingAnim = std::max(0.0f, computerSwingAnim - 0.12f);

    // 2. State & Delays
    if (gameState == STATE_POINT_SCORED) {
        messageTimer--;
        if (messageTimer <= 0) {
            resetServe(server);
        }
    } else if (gameState == STATE_SERVE) {
        if (server == 2) {
            messageTimer++;
            if (messageTimer > 40) {
                messageTimer = 0;
                doComputerServe();
            }
        }
    }

    // 3. Realistic Human-like AI Paddle Tracking
    float compTargetX = 0.0f;
    float compTargetY = TABLE_SURFACE_Y + 0.22f;

    if (gameState == STATE_RALLY) {
        if (ballVZ < 0.0f) {
            // Ball heading toward computer
            if (aiReactionCountdown > 0) {
                aiReactionCountdown--;
                // During human reaction delay, paddle holds or drifts slightly
                compTargetX = computerX;
            } else {
                compTargetX = ballX + compErrorNoiseX;
                compTargetY = std::max(TABLE_SURFACE_Y + 0.12f, ballY);
            }
        } else {
            // Re-center after shot
            compTargetX = 0.0f;
            compTargetY = TABLE_SURFACE_Y + 0.22f;
        }
    }

    // Realistic human maximum movement speed
    float compSpeed = 0.068f * std::sqrt(ballSpeedMultiplier);
    if (std::abs(compTargetX - computerX) > 0.02f) {
        computerX += (compTargetX > computerX ? compSpeed : -compSpeed);
    }
    computerY += (compTargetY - computerY) * 0.20f;
    computerX = std::max(-TABLE_WIDTH * 0.48f, std::min(TABLE_WIDTH * 0.48f, computerX));

    // 4. Ball Simulation during Rally
    if (gameState == STATE_RALLY) {
        prevBallZ = ballZ;

        ballVY -= GRAVITY;

        ballX += ballVX;
        ballY += ballVY;
        ballZ += ballVZ;

        float halfW = TABLE_WIDTH * 0.5f;
        float halfL = TABLE_LENGTH * 0.5f;
        bool overTableX = (ballX >= -halfW && ballX <= halfW);
        bool overTableZ = (ballZ >= -halfL && ballZ <= halfL);

        if (ballY <= (TABLE_SURFACE_Y + BALL_RADIUS)) {
            if (overTableX && overTableZ) {
                ballY = TABLE_SURFACE_Y + BALL_RADIUS;
                ballVY = -ballVY * 0.82f;
                if (ballVY < 0.046f) ballVY = 0.046f;

                if (ballZ > 0.0f) {
                    if (ballBouncedPlayerSide) {
                        triggerPointScored(2, "BALL BOUNCED TWICE!");
                    }
                    ballBouncedPlayerSide = true;
                } else {
                    if (ballBouncedCompSide) {
                        triggerPointScored(1, "BALL BOUNCED TWICE!");
                    }
                    ballBouncedCompSide = true;
                }
            } else if (ballY <= FLOOR_Y + BALL_RADIUS) {
                if (lastHitter == 1) {
                    if (ballBouncedCompSide) {
                        triggerPointScored(1, "COMPUTER MISSED!");
                    } else {
                        triggerPointScored(2, "OUT OF BOUNDS!");
                    }
                } else if (lastHitter == 2) {
                    if (ballBouncedPlayerSide) {
                        triggerPointScored(2, "YOU MISSED!");
                    } else {
                        triggerPointScored(1, "COMPUTER HIT OUT OF BOUNDS!");
                    }
                }
            }
        }

        // Net collision check
        if (std::abs(ballZ) < 0.07f) {
            float netMaxW = TABLE_WIDTH * 0.5f + NET_OVERHANG;
            if (ballY < (TABLE_SURFACE_Y + NET_HEIGHT) && std::abs(ballX) <= netMaxW) {
                ballVZ = -ballVZ * 0.35f;
                ballVY *= 0.5f;
                ballVX *= 0.6f;

                if (lastHitter == 2) {
                    triggerPointScored(1, "COMPUTER HIT THE NET!");
                } else if (lastHitter == 1) {
                    triggerPointScored(2, "YOU HIT THE NET!");
                }
            }
        }

        // Automatic Player Paddle Hit
        if (ballVZ > 0.0f && ballZ >= (playerZ - 0.32f) && ballZ <= (playerZ + 0.22f)) {
            float dx = ballX - playerX;
            float distH = std::abs(dx);

            if (distH <= (PADDLE_RADIUS * 1.38f) && ballY >= (TABLE_SURFACE_Y - 0.02f) && ballY <= (playerY + PADDLE_RADIUS * 1.6f)) {
                lastHitter = 1;
                ballBouncedPlayerSide = false;
                ballBouncedCompSide = false;
                rallyCount++;
                playerSwingAnim = 0.8f;

                float contactOffset = dx / PADDLE_RADIUS;
                float targetX = (contactOffset * 0.65f) + (playerVelocityX * 3.2f);
                targetX = std::max(-TABLE_WIDTH * 0.46f, std::min(TABLE_WIDTH * 0.46f, targetX));
                float targetZ = -1.35f - (std::abs(contactOffset) * 0.35f);

                float duration = std::max(19.0f, 25.0f - rallyCount * 0.5f);
                launchShotToTarget(ballX, ballY, playerZ - 0.15f,
                                   targetX, targetZ, 0.10f, duration);

                aiReactionCountdown = 6 + (std::rand() % 8);
                compErrorNoiseX = ((std::rand() % 100) / 100.0f - 0.5f) * 0.18f;
            }
        }

        // Computer Paddle Hit / Miss Check
        if (ballVZ < 0.0f && ballZ <= (computerZ + 0.28f) && ballZ >= (computerZ - 0.18f)) {
            float dx = ballX - computerX;
            float reach = PADDLE_RADIUS * 1.05f;

            if (std::abs(dx) <= reach && ballY >= (TABLE_SURFACE_Y - 0.02f)) {
                handleComputerHit();
            } else {
                // Ball was outside computer's reach: computer swings and misses!
                computerSwingAnim = 1.0f;
            }
        }

        // Ball passed player or computer
        if (ballZ > (playerZ + 0.9f)) {
            if (ballBouncedPlayerSide) {
                triggerPointScored(2, "COMPUTER SCORED!");
            } else {
                triggerPointScored(1, "OUT OF BOUNDS!");
            }
        } else if (ballZ < (computerZ - 0.9f)) {
            if (ballBouncedCompSide) {
                triggerPointScored(1, "YOU SCORED! CLEAN WINNER!");
            } else {
                triggerPointScored(2, "OUT OF BOUNDS!");
            }
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, update, 0);
}

// Main 3D Display function
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    gluLookAt(0.0, 1.05, 3.45,
              0.0, -0.05, -0.40,
              0.0, 1.0, 0.0);

    drawFloor();
    drawTable();
    drawNet();
    drawPaddleModel(computerX, computerY, computerZ, 0.0f, computerSwingAnim, false);
    drawBall();
    drawPaddleModel(playerX, playerY, playerZ, playerTilt, playerSwingAnim, true);
    drawHUD();

    glutSwapBuffers();
}

void mousePassiveMotion(int x, int y) {
    if (!mouseControlActive) return;

    float normX = ((float)x / (float)windowWidth) * 2.0f - 1.0f;
    playerTargetX = normX * (TABLE_WIDTH * 0.65f);

    float normY = 1.0f - ((float)y / (float)windowHeight);
    playerTargetY = TABLE_SURFACE_Y + 0.10f + normY * 0.45f;
}

void mouseClick(int button, int state, int, int) {
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        if (gameState == STATE_GAME_OVER) {
            playerScore = 0;
            computerScore = 0;
            resetServe(1);
        } else {
            handlePlayerHitAttempt();
        }
    }
}

void keyboardDown(unsigned char key, int, int) {
    keys[key] = true;

    if (key == 27) { // ESC
        std::exit(0);
    }

    if (key == ' ') {
        if (gameState == STATE_GAME_OVER) {
            playerScore = 0;
            computerScore = 0;
            resetServe(1);
        } else {
            handlePlayerHitAttempt();
        }
    }

    if (key == 'r' || key == 'R') {
        playerScore = 0;
        computerScore = 0;
        resetServe(1);
    }

    if (key == '+' || key == '=') {
        ballSpeedMultiplier = std::min(2.8f, ballSpeedMultiplier + 0.20f);
        statusMessage = "SPEED INCREASED!";
    }
    if (key == '-' || key == '_') {
        ballSpeedMultiplier = std::max(1.0f, ballSpeedMultiplier - 0.20f);
        statusMessage = "SPEED DECREASED!";
    }
}

void keyboardUp(unsigned char key, int, int) {
    keys[key] = false;
}

void specialKeyDown(int key, int, int) {
    specialKeys[key] = true;
}

void specialKeyUp(int key, int, int) {
    specialKeys[key] = false;
}

void reshape(int w, int h) {
    windowWidth = w;
    windowHeight = (h == 0) ? 1 : h;
    glViewport(0, 0, windowWidth, windowHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(48.0, (double)windowWidth / (double)windowHeight, 0.1, 50.0);
    glMatrixMode(GL_MODELVIEW);
}

void initOpenGL() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_LIGHT0);
    GLfloat lightPos[] = { 0.0f, 4.5f, 0.2f, 1.0f };
    GLfloat lightAmbient[] = { 0.35f, 0.35f, 0.35f, 1.0f };
    GLfloat lightDiffuse[] = { 0.90f, 0.90f, 0.88f, 1.0f };
    GLfloat lightSpecular[] = { 0.80f, 0.80f, 0.80f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, lightAmbient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);

    setupFloorTexture();
}

int main(int argc, char** argv) {
    std::srand(static_cast<unsigned int>(std::time(NULL)));

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(windowWidth, windowHeight);
    glutCreateWindow("Table Tennis 3D");

    initOpenGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialKeyDown);
    glutSpecialUpFunc(specialKeyUp);
    glutPassiveMotionFunc(mousePassiveMotion);
    glutMotionFunc(mousePassiveMotion);
    glutMouseFunc(mouseClick);

    resetServe(1);
    glutTimerFunc(16, update, 0);

    glutMainLoop();
    return 0;
}
