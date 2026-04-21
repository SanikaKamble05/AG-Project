#include <GL/glut.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INIT_W 1100
#define INIT_H 800

#define MAX_GRID 6
#define MAX_CARDS (MAX_GRID * MAX_GRID)
#define SCORE_FILE "score.txt"

int hintActive = 0;
float winAnim = 0.0f;

typedef enum {
    SCREEN_START = 0,
    SCREEN_PLAYING = 1,
    SCREEN_WIN = 2
} GameScreen;

typedef struct {
    char symbol[8];
    int flipped;
    int matched;
    float flipAnim;
    float targetFlipAnim;
    float hoverAnim;
    float targetHoverAnim;
    float bounceAnim;
} Card;

static Card cards[MAX_CARDS];

static int level = 2;
static int gridRows = 4;
static int gridCols = 4;
static int cardCount = 16;

static int firstCard = -1;
static int secondCard = -1;
static int lockBoard = 0;
static int matchedPairs = 0;
static int moves = 0;
static int hoverCard = -1;

static int winW = INIT_W;
static int winH = INIT_H;

static float cardW = 120.0f;
static float cardH = 120.0f;
static float gapX = 16.0f;
static float gapY = 16.0f;
static float startX = 0.0f;
static float startY = 0.0f;
static float topMargin = 130.0f;
static float bottomMargin = 95.0f;

static GameScreen currentScreen = SCREEN_START;

static time_t gameStartTime = 0;
static int elapsedSeconds = 0;
static int bestTime = 0;
static int bestMoves = 0;
static float bestAccuracy = 0.0f;
static int lastWinTime = 0;

static const char *symbolPool[] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J",
    "K", "L", "M", "N", "O", "P", "Q", "R"
};

static void drawText(float x, float y, const char *txt, void *font) {
    glRasterPos2f(x, y);
    for (const char *c = txt; *c; c++) glutBitmapCharacter(font, *c);
}

static int textWidth(const char *txt, void *font) {
    int width = 0;
    for (const char *c = txt; *c; c++) width += glutBitmapWidth(font, *c);
    return width;
}

static void drawCenteredText(float y, const char *txt, void *font) {
    drawText((float)(winW - textWidth(txt, font)) * 0.5f, y, txt, font);
}

static void rect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y - h);
    glVertex2f(x, y - h);
    glEnd();
}

static void drawShadow(float x, float y, float w, float h, float alpha) {
    glColor4f(0.0f, 0.0f, 0.0f, alpha);
    rect(x + 6.0f, y - 6.0f, w, h);
}

static void drawGradientBackground(void) {
    glBegin(GL_QUADS);
    glColor3f(0.09f, 0.11f, 0.21f); glVertex2f(0, 0);
    glColor3f(0.16f, 0.19f, 0.32f); glVertex2f((float)winW, 0);
    glColor3f(0.06f, 0.07f, 0.15f); glVertex2f((float)winW, (float)winH);
    glColor3f(0.07f, 0.09f, 0.17f); glVertex2f(0, (float)winH);
    glEnd();
}

static const char *levelName(int levelValue) {
    if (levelValue == 1) return "Easy (2x2)";
    if (levelValue == 2) return "Medium (4x4)";
    return "Hard (6x6)";
}

static void applyLevel(void) {
    if (level == 1) {
        gridRows = 2; gridCols = 2;
    } else if (level == 2) {
        gridRows = 4; gridCols = 4;
    } else {
        gridRows = 6; gridCols = 6;
    }
    cardCount = gridRows * gridCols;
}

static void computeLayout(void) {
    float usableW = (float)winW;
    float usableH = (float)winH - topMargin - bottomMargin;
    float gridAspect = (float)gridCols / (float)gridRows;
    float usableAspect = usableW / usableH;

    if (usableAspect > gridAspect) {
        cardH = (usableH - gapY * (gridRows - 1)) / gridRows;
        cardW = cardH;
    } else {
        cardW = (usableW - gapX * (gridCols - 1)) / gridCols;
        cardH = cardW;
    }

    if (cardW > 170.0f) cardW = 170.0f;
    if (cardH > 170.0f) cardH = 170.0f;
    if (cardW < 58.0f) cardW = 58.0f;
    if (cardH < 58.0f) cardH = 58.0f;

    {
        float totalW = gridCols * cardW + (gridCols - 1) * gapX;
        float totalH = gridRows * cardH + (gridRows - 1) * gapY;
        startX = ((float)winW - totalW) * 0.5f;
        startY = topMargin + ((usableH - totalH) * 0.5f) + totalH;
    }
}

static void loadBestScore(void) {
    FILE *f = fopen(SCORE_FILE, "r");
    if (!f) return;
    fscanf(f, "%d %d %f", &bestTime, &bestMoves, &bestAccuracy);
    fclose(f);
}

static void saveBestScoreIfNeeded(int winTime, int winMoves, float accuracy) {
    int shouldSave = 0;
    if (bestTime == 0 || winTime < bestTime) shouldSave = 1;
    if (bestMoves == 0 || winMoves < bestMoves) shouldSave = 1;
    if (accuracy > bestAccuracy) shouldSave = 1;

    if (!shouldSave) return;

    if (bestTime == 0 || winTime < bestTime) bestTime = winTime;
    if (bestMoves == 0 || winMoves < bestMoves) bestMoves = winMoves;
    if (accuracy > bestAccuracy) bestAccuracy = accuracy;

    {
        FILE *f = fopen(SCORE_FILE, "w");
        if (!f) return;
        fprintf(f, "%d %d %.4f\n", bestTime, bestMoves, bestAccuracy);
        fclose(f);
    }
}

static void buildDeck(void) {
    int pairCount = cardCount / 2;
    int idx = 0;

    for (int p = 0; p < pairCount; p++) {
        strcpy(cards[idx++].symbol, symbolPool[p]);
        strcpy(cards[idx++].symbol, symbolPool[p]);
    }

    for (int i = cardCount - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        Card t = cards[i];
        cards[i] = cards[j];
        cards[j] = t;
    }

    for (int i = 0; i < cardCount; i++) {
        cards[i].flipped = 0;
        cards[i].matched = 0;
        cards[i].flipAnim = 0.0f;
        cards[i].targetFlipAnim = 0.0f;
        cards[i].hoverAnim = 0.0f;
        cards[i].targetHoverAnim = 0.0f;
        cards[i].bounceAnim = 0.0f;
    }
}

static void resetTurnState(void) {
    firstCard = -1;
    secondCard = -1;
    lockBoard = 0;
}

static void startGame(void) {
    applyLevel();
    computeLayout();
    buildDeck();
    resetTurnState();
    matchedPairs = 0;
    moves = 0;
    hoverCard = -1;
    elapsedSeconds = 0;
    gameStartTime = time(NULL);
    currentScreen = SCREEN_PLAYING;
}

static void goToStartScreen(void) {
    currentScreen = SCREEN_START;
}

static void unflipTimer(int value) {
    (void)value;
    if (firstCard >= 0 && secondCard >= 0) {
        cards[firstCard].flipped = 0;
        cards[secondCard].flipped = 0;
        cards[firstCard].targetFlipAnim = 0.0f;
        cards[secondCard].targetFlipAnim = 0.0f;
    }
    resetTurnState();
}

void hintTimer(int value) {
    hintActive = 0;

    for (int i = 0; i < cardCount; i++) {
        if (!cards[i].matched) {
            cards[i].flipped = 0;
            cards[i].targetFlipAnim = 0.0f;
        }
    }
}

static void finishWin(void) {
    float accuracy = (moves > 0) ? ((float)matchedPairs / (float)moves) * 100.0f : 0.0f;
    lastWinTime = elapsedSeconds;
    saveBestScoreIfNeeded(lastWinTime, moves, accuracy);
    currentScreen = SCREEN_WIN;
}

static void checkMatch(void) {
    int isMatch = strcmp(cards[firstCard].symbol, cards[secondCard].symbol) == 0;
    if (isMatch) {
        cards[firstCard].matched = 1;
        cards[secondCard].matched = 1;
        cards[firstCard].bounceAnim = 1.0f;
        cards[secondCard].bounceAnim = 1.0f;
        matchedPairs++;
        if (matchedPairs == cardCount / 2) finishWin();
        resetTurnState();
    } else {
        lockBoard = 1;
        glutTimerFunc(750, unflipTimer, 0);
    }
}

static void flipCard(int idx) {
    if (currentScreen != SCREEN_PLAYING) return;
    if (lockBoard || idx < 0 || idx >= cardCount) return;
    if (idx == firstCard || cards[idx].matched) return;

    cards[idx].flipped = 1;
    cards[idx].targetFlipAnim = 1.0f;

    if (firstCard == -1) {
        firstCard = idx;
    } else {
        secondCard = idx;
        moves++;
        checkMatch();
    }
}

static void getCardRect(int idx, float *x, float *y, float *w, float *h) {
    int r = idx / gridCols;
    int c = idx % gridCols;
    *w = cardW;
    *h = cardH;
    *x = startX + c * (cardW + gapX);
    *y = startY - r * (cardH + gapY);
}

static void drawCardFace(float x, float y, float w, float h, int idx, float reveal) {
    if (reveal < 0.5f) {
        glColor3f(0.24f, 0.49f, 0.92f);
        rect(x, y, w, h);
        glColor3f(0.33f, 0.63f, 0.98f);
        rect(x + 6.0f, y - 6.0f, w - 12.0f, h - 12.0f);
        glColor3f(0.94f, 0.97f, 1.0f);
        drawText(x + w * 0.48f, y - h * 0.50f, "?", GLUT_BITMAP_HELVETICA_18);
    } else {
        if (cards[idx].matched) {
            glColor3f(0.29f, 0.75f, 0.47f);

            // ADD GLOW EFFECT
            glLineWidth(4.0f);
            glColor3f(0.2f, 1.0f, 0.4f);
            rect(x - 2, y + 2, w + 4, h + 4);
        }
        else glColor3f(0.96f, 0.80f, 0.32f);
        rect(x, y, w, h);
        glColor3f(0.99f, 0.95f, 0.82f);
        rect(x + 7.0f, y - 7.0f, w - 14.0f, h - 14.0f);
        glColor3f(0.14f, 0.11f, 0.08f);
        drawText(x + w * 0.45f, y - h * 0.52f, cards[idx].symbol, GLUT_BITMAP_TIMES_ROMAN_24);
    }

    if (idx == hoverCard && !cards[idx].matched && currentScreen == SCREEN_PLAYING) glColor3f(1.0f, 1.0f, 1.0f);
    else glColor3f(0.07f, 0.09f, 0.15f);

    glLineWidth(2.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y - h);
    glVertex2f(x, y - h);
    glEnd();
}

static void drawCard(int idx) {
    float x, y, w, h;
    float flipScale;
    float hoverScale;
    float bounceScale;
    getCardRect(idx, &x, &y, &w, &h);

    flipScale = fabsf(cosf(cards[idx].flipAnim * 3.1415926f));
    if (flipScale < 0.08f) flipScale = 0.08f;
    hoverScale = 1.0f + 0.08f * cards[idx].hoverAnim;
    bounceScale = 1.0f + 0.15f * sinf(cards[idx].bounceAnim * 3.1415926f);

    drawShadow(x, y, w, h, 0.19f);

    glPushMatrix();
    glTranslatef(x + w * 0.5f, y - h * 0.5f, 0.0f);
    glScalef(flipScale * hoverScale * bounceScale, hoverScale * bounceScale, 1.0f);
    glTranslatef(-(x + w * 0.5f), -(y - h * 0.5f), 0.0f);
    drawCardFace(x, y, w, h, idx, cards[idx].flipAnim);
    glPopMatrix();
}

static void drawStartScreen(void) {
    char info[128];
    drawGradientBackground();
    glColor3f(0.96f, 0.97f, 1.0f);
    drawCenteredText(180.0f, "Memory Match", GLUT_BITMAP_TIMES_ROMAN_24);
    glColor3f(0.78f, 0.86f, 0.97f);
    drawCenteredText(230.0f, "Press 1 for Easy (2x2), 2 for Medium (4x4), 3 for Hard (6x6)", GLUT_BITMAP_HELVETICA_18);
    snprintf(info, sizeof(info), "Current Level: %s", levelName(level));
    drawCenteredText(270.0f, info, GLUT_BITMAP_HELVETICA_18);
    glColor3f(1.0f, 0.95f, 0.70f);
    drawCenteredText(320.0f, "Press Enter to Start", GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.70f, 0.77f, 0.90f);
    drawCenteredText(380.0f, "Saved Best Records", GLUT_BITMAP_HELVETICA_18);
    snprintf(info, sizeof(info), "Best Time: %ds   Best Moves: %d   Best Accuracy: %.1f%%", bestTime, bestMoves, bestAccuracy);
    drawCenteredText(415.0f, info, GLUT_BITMAP_HELVETICA_18);
}

static void drawPlayingScreen(void) {
    char line[160];
    float accuracy = (moves > 0) ? ((float)matchedPairs / (float)moves) * 100.0f : 0.0f;

    drawGradientBackground();
    glColor3f(0.96f, 0.97f, 1.0f);
    drawCenteredText(46.0f, "Memory Match", GLUT_BITMAP_TIMES_ROMAN_24);

    snprintf(line, sizeof(line), "Level: %s", levelName(level));
    glColor3f(0.82f, 0.88f, 0.98f);
    drawCenteredText(76.0f, line, GLUT_BITMAP_HELVETICA_18);

    snprintf(line, sizeof(line), "Time: %ds    Moves: %d    Accuracy: %.1f%%", elapsedSeconds, moves, accuracy);
    drawCenteredText(102.0f, line, GLUT_BITMAP_HELVETICA_18);

    for (int i = 0; i < cardCount; i++) drawCard(i);

    glColor3f(0.72f, 0.78f, 0.90f);
    drawCenteredText((float)winH - 28.0f, "Click cards | R: Restart level | B: Back to start | ESC: Exit", GLUT_BITMAP_HELVETICA_18);
}

static void drawWinScreen(void) {
    char line[180];
    float accuracy = (moves > 0) ? ((float)matchedPairs / (float)moves) * 100.0f : 0.0f;

    glClearColor(0.1f + 0.2f * sinf(winAnim), 0.1f, 0.2f, 1.0f);
    
    drawGradientBackground();
    glColor3f(1.0f, 0.94f, 0.72f);
    glPushMatrix();

    float scale = 1.0f + 0.5f * sinf(winAnim * 3.14f);

    glTranslatef(winW/2, 200, 0);
    glScalef(scale, scale, 1.0f);
    glTranslatef(-winW/2, -200, 0);

    glColor3f(1.0f, 0.9f, 0.3f);
    drawCenteredText(200.0f, "YOU WIN!", GLUT_BITMAP_TIMES_ROMAN_24);

    glPopMatrix();
    glColor3f(0.84f, 0.90f, 0.99f);
    snprintf(line, sizeof(line), "Level: %s", levelName(level));
    drawCenteredText(250.0f, line, GLUT_BITMAP_HELVETICA_18);
    snprintf(line, sizeof(line), "Time: %ds    Moves: %d    Accuracy: %.1f%%", lastWinTime, moves, accuracy);
    drawCenteredText(282.0f, line, GLUT_BITMAP_HELVETICA_18);
    snprintf(line, sizeof(line), "Best Time: %ds    Best Moves: %d    Best Accuracy: %.1f%%", bestTime, bestMoves, bestAccuracy);
    drawCenteredText(318.0f, line, GLUT_BITMAP_HELVETICA_18);
    glColor3f(0.72f, 0.78f, 0.90f);
    drawCenteredText(380.0f, "Press Enter to play again | B to change level", GLUT_BITMAP_HELVETICA_18);
}

static void display(void) {
    glClear(GL_COLOR_BUFFER_BIT);
    glLoadIdentity();

    if (currentScreen == SCREEN_START) drawStartScreen();
    else if (currentScreen == SCREEN_PLAYING) drawPlayingScreen();
    else drawWinScreen();

    glutSwapBuffers();
}

static void reshape(int w, int h) {
    winW = (w <= 0) ? INIT_W : w;
    winH = (h <= 0) ? INIT_H : h;
    computeLayout();

    glViewport(0, 0, winW, winH);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, winW, winH, 0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

static int cardIndexFromMouse(int mx, int my) {
    for (int i = 0; i < cardCount; i++) {
        float x, y, w, h;
        getCardRect(i, &x, &y, &w, &h);
        if (mx >= x && mx <= x + w && my <= y && my >= y - h) return i;
    }
    return -1;
}

static void mouse(int button, int state, int x, int y) {
    if (currentScreen != SCREEN_PLAYING) return;
    if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
        int idx = cardIndexFromMouse(x, y);
        if (idx != -1) flipCard(idx);
    }
}

static void passiveMotion(int x, int y) {
    if (currentScreen != SCREEN_PLAYING) {
        hoverCard = -1;
        return;
    }
    hoverCard = cardIndexFromMouse(x, y);
}

static void keyboard(unsigned char key, int x, int y) {
    (void)x;
    (void)y;

    if (key == 27) exit(0);

    if (key == '1') level = 1;
    if (key == '2') level = 2;
    if (key == '3') level = 3;

    if (key == 13) {
        if (currentScreen == SCREEN_START || currentScreen == SCREEN_WIN) startGame();
    } else if (key == 'r' || key == 'R') {
        if (currentScreen == SCREEN_PLAYING) startGame();
    } else if (key == 'b' || key == 'B') {
        goToStartScreen();
    }
    else if (key == 'h' || key == 'H') {
        if (currentScreen == SCREEN_PLAYING && !hintActive) {
            hintActive = 1;

            for (int i = 0; i < cardCount; i++) {
                cards[i].flipped = 1;
                cards[i].targetFlipAnim = 1.0f;
            }

            glutTimerFunc(1000, hintTimer, 0);
        }
    }
}

static void tick(int value) {
    (void)value;

    if (currentScreen == SCREEN_PLAYING) {
        elapsedSeconds = (int)difftime(time(NULL), gameStartTime);
    }

    for (int i = 0; i < cardCount; i++) {
        cards[i].flipAnim += (cards[i].targetFlipAnim - cards[i].flipAnim) * 0.18f;
        if (fabsf(cards[i].targetFlipAnim - cards[i].flipAnim) < 0.01f) cards[i].flipAnim = cards[i].targetFlipAnim;

        cards[i].targetHoverAnim = (i == hoverCard) ? 1.0f : 0.0f;
        cards[i].hoverAnim += (cards[i].targetHoverAnim - cards[i].hoverAnim) * 0.22f;

        if (cards[i].bounceAnim > 0.0f) {
            cards[i].bounceAnim -= 0.06f;
            if (cards[i].bounceAnim < 0.0f) cards[i].bounceAnim = 0.0f;
        }

        if (cards[i].matched) cards[i].targetFlipAnim = 1.0f;
        if (!cards[i].flipped && !cards[i].matched) cards[i].targetFlipAnim = 0.0f;

        if (currentScreen == SCREEN_WIN) {
            winAnim += 0.05f;
            if (winAnim > 1.0f) winAnim = 1.0f;
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, tick, 0);
}

int main(int argc, char **argv) {
    srand((unsigned int)time(NULL));
    loadBestScore();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(INIT_W, INIT_H);
    glutCreateWindow("Memory Card Game");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.06f, 0.07f, 0.13f, 1.0f);

    applyLevel();
    computeLayout();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutPassiveMotionFunc(passiveMotion);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, tick, 0);
    glutMainLoop();
    return 0;
}
