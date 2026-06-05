#define _CRT_SECURE_NO_WARNINGS
#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <iostream>
#include <cstdio>

// --------------------------------------------------------
// 1. 수학 및 물리 구조체 정의
// --------------------------------------------------------
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }

    float dot(const Vec2& other) const { return x * other.x + y * other.y; }
    float length() const { return std::sqrt(x * x + y * y); }

    Vec2 normalize() const {
        float len = length();
        if (len > 0) return Vec2(x / len, y / len);
        return Vec2(0, 0);
    }
};

struct Ball {
    Vec2 pos;
    Vec2 vel;
    float radius = 15.0f;
    float mass = 1.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f;

    // ★ 회전(스핀) 관련 변수 추가
    float spinX = 0.0f;   // 사이드 스핀 (좌/우 당점): 진행방향 수직으로 커브 유발
    float spinY = 0.0f;   // 탑/백 스핀 (위/아래 당점): 속도 증폭 또는 역회전 유발
    float rotAngle = 0.0f; // 렌더링용 공 자전 각도
};

// --------------------------------------------------------
// 2. 전역 변수 설정
// --------------------------------------------------------
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 400;
const float FRICTION = 0.99f;
// ★ 스핀 감쇠: 스핀은 속도보다 더 빠르게 소멸
const float SPIN_FRICTION = 0.97f;

std::vector<Ball> balls;

// 카메라 제어 변수
float camEyeX = 400.0f; float camEyeY = -150.0f; float camEyeZ = 500.0f;
float camCenterX = 400.0f; float camCenterY = 200.0f; float camCenterZ = 0.0f;
bool isTopView = true;

// 큐대 및 타격 애니메이션 제어 변수
bool isCueVisible = true;
float cueAngle = 0.0f;
float cuePower = 5.0f;
float hitOffsetX = 0.0f;  // 좌우 당점 (-1 ~ +1): 사이드 스핀
float hitOffsetY = 0.0f;  // 상하 당점 (-1 ~ +1): 백스핀(-) / 탑스핀(+)
bool isStriking = false;
float strikeAnimationOffset = 0.0f;

// HUD 메시지
char hudMsg[128] = "";
int hudMsgTimer = 0;

// --------------------------------------------------------
// 3. 초기화 및 조명 설정
// --------------------------------------------------------
void initLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);

    GLfloat lightPos[] = { 400.0f, 200.0f, 600.0f, 1.0f };
    GLfloat ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
}

void initBalls() {
    balls.clear();
    Ball whiteBall;
    whiteBall.pos = Vec2(200.0f, 200.0f);
    balls.push_back(whiteBall);

    float targetX = 500.0f;
    float targetY = 200.0f;
    for (int i = 0; i < 3; i++) {
        Ball redBall;
        redBall.pos = Vec2(targetX + (i * 32.0f), targetY + (i % 2 == 0 ? 0 : 20.0f));
        redBall.r = 1.0f; redBall.g = 0.2f; redBall.b = 0.2f;
        balls.push_back(redBall);
    }
}

// --------------------------------------------------------
// 4. 물리 엔진 로직
// --------------------------------------------------------
void resolveCollision(Ball& b1, Ball& b2) {
    Vec2 delta = b1.pos - b2.pos;
    float distance = delta.length();
    float minDistance = b1.radius + b2.radius;

    if (distance < minDistance) {
        if (distance == 0.0f) { delta = Vec2(1.0f, 0.0f); distance = 1.0f; }
        float overlap = minDistance - distance;
        Vec2 normal = delta.normalize();
        float totalMass = b1.mass + b2.mass;

        b1.pos = b1.pos + normal * (overlap * (b2.mass / totalMass));
        b2.pos = b2.pos - normal * (overlap * (b1.mass / totalMass));

        Vec2 relativeVelocity = b1.vel - b2.vel;
        float velocityAlongNormal = relativeVelocity.dot(normal);
        if (velocityAlongNormal > 0) return;

        float e = 0.95f;
        float j = -(1.0f + e) * velocityAlongNormal;
        j /= (1.0f / b1.mass + 1.0f / b2.mass);

        Vec2 impulse = normal * j;
        b1.vel = b1.vel + impulse * (1.0f / b1.mass);
        b2.vel = b2.vel - impulse * (1.0f / b2.mass);

        // ★ 충돌 시 b1의 사이드 스핀 일부를 b2에 전달 (당구 마세 효과 근사)
        float spinTransfer = 0.3f;
        b2.spinX += b1.spinX * spinTransfer;
        b1.spinX *= (1.0f - spinTransfer);
    }
}

// ★ 핵심 함수: 당점에 따른 스핀 초기값 계산 후 공 속도에 적용
void applyHitSpinPhysics(Ball& ball, float angle_rad, float power,
    float offsetX, float offsetY) {
    // 기본 발사 속도
    float vx = cos(angle_rad) * power;
    float vy = sin(angle_rad) * power;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // [좌/우 당점 → 사이드 스핀]
    //   offsetX > 0 : 오른쪽 당점 → 우측 커브 (sinX > 0)
    //   offsetX < 0 : 왼쪽 당점  → 좌측 커브 (sinX < 0)
    //
    //   스핀 세기는 당점 오프셋 * 파워에 비례
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    ball.spinX = offsetX * power * 0.4f;

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // [위/아래 당점 → 탑스핀 / 백스핀]
    //   offsetY > 0 : 위쪽 당점 → 탑스핀 (속도 유지 / 증폭)
    //   offsetY < 0 : 아래쪽    → 백스핀 (진행방향 반대 회전)
    //
    //   spinY > 0 : 탑스핀 → 마찰과 스핀이 같은 방향 → 감속 완화
    //   spinY < 0 : 백스핀 → 마찰과 스핀이 반대 방향 → 빠른 감속, 역행 가능
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    ball.spinY = offsetY * power * 0.5f;

    ball.vel.x = vx;
    ball.vel.y = vy;
}

void updatePhysics(int value) {
    // 1. 타격 애니메이션 처리
    if (isStriking) {
        strikeAnimationOffset -= 4.0f;
        if (strikeAnimationOffset <= -2.0f) {
            float rad = cueAngle * 3.141592f / 180.0f;
            // ★ 스핀 물리 적용 함수 호출 (기존 단순 각도/힘 보정 대체)
            applyHitSpinPhysics(balls[0], rad, cuePower, hitOffsetX, hitOffsetY);

            isStriking = false;
            cuePower = 5.0f;
            hitOffsetX = 0.0f;
            hitOffsetY = 0.0f;
        }
    }

    bool isAnyMoving = false;

    // 2. 공 이동, 스핀 효과 적용, 벽면 충돌 처리
    for (size_t i = 0; i < balls.size(); i++) {
        Ball& b = balls[i];

        float speed = b.vel.length();
        if (speed > 0.1f) {
            Vec2 dir = b.vel.normalize();

            // ★ [사이드 스핀 효과]
            //   진행 방향의 수직 벡터에 spinX 크기로 횡방향 힘 추가
            //   spinX가 클수록 옆으로 휘어지는 커브 발생
            if (std::abs(b.spinX) > 0.01f) {
                // 진행방향 수직: (-dir.y, dir.x)
                float curveForce = b.spinX * 0.06f;
                b.vel.x += (-dir.y) * curveForce;
                b.vel.y += (dir.x) * curveForce;
            }

            // ★ [탑/백 스핀 효과]
            //   탑스핀(spinY > 0): 속도 감쇠를 늦춤 (굴러가는 관성)
            //   백스핀(spinY < 0): 속도에 반대 방향 힘 추가 → 빠른 감속, 역행 가능
            if (std::abs(b.spinY) > 0.01f) {
                float spinEffect = b.spinY * 0.05f;
                b.vel.x += dir.x * spinEffect;
                b.vel.y += dir.y * spinEffect;
            }
        }

        // 스핀 자체를 시간에 따라 감쇠 (회전에너지 소산)
        b.spinX *= SPIN_FRICTION;
        b.spinY *= SPIN_FRICTION;
        if (std::abs(b.spinX) < 0.001f) b.spinX = 0.0f;
        if (std::abs(b.spinY) < 0.001f) b.spinY = 0.0f;

        // 속도 감쇠
        b.vel.x *= FRICTION;
        b.vel.y *= FRICTION;

        if (std::abs(b.vel.x) < 0.05f) b.vel.x = 0;
        if (std::abs(b.vel.y) < 0.05f) b.vel.y = 0;

        if (b.vel.length() > 0.1f) isAnyMoving = true;

        // 렌더링용 자전 각도 업데이트
        b.rotAngle += b.vel.length() * 2.0f;

        b.pos.x += b.vel.x;
        b.pos.y += b.vel.y;

        // 벽면 충돌
        if (b.pos.x - b.radius < 0) {
            b.pos.x = b.radius;
            b.vel.x = -b.vel.x;
            b.spinX = -b.spinX * 0.7f; // 벽 반사 시 사이드 스핀 방향 반전+감쇠
        }
        else if (b.pos.x + b.radius > WINDOW_WIDTH) {
            b.pos.x = WINDOW_WIDTH - b.radius;
            b.vel.x = -b.vel.x;
            b.spinX = -b.spinX * 0.7f;
        }
        if (b.pos.y - b.radius < 0) {
            b.pos.y = b.radius;
            b.vel.y = -b.vel.y;
            b.spinX = -b.spinX * 0.7f;
        }
        else if (b.pos.y + b.radius > WINDOW_HEIGHT) {
            b.pos.y = WINDOW_HEIGHT - b.radius;
            b.vel.y = -b.vel.y;
            b.spinX = -b.spinX * 0.7f;
        }
    }

    // 3. 공 간 충돌 체크
    for (size_t i = 0; i < balls.size(); i++) {
        for (size_t j = i + 1; j < balls.size(); j++) {
            resolveCollision(balls[i], balls[j]);
        }
    }

    isCueVisible = !isAnyMoving && !isStriking;

    if (hudMsgTimer > 0) hudMsgTimer--;

    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0);
}

// --------------------------------------------------------
// 5. HUD 텍스트 렌더링
// --------------------------------------------------------
void drawText2D(float x, float y, const char* text) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST); // ★ 추가: UI가 당구대에 파묻히지 않도록 깊이 검사 비활성화

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glRasterPos2f(x, y);
    for (const char* c = text; *c; c++)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, *c);
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST); // ★ 추가: 렌더링 완료 후 깊이 검사 원상복구
}

// ★ 당점 시각화: 공 단면 원 + 빨간 점
void drawHitPointIndicator(float screenX, float screenY, float offsetX, float offsetY) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST); // ★ 추가: 깊이 검사 비활성화

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    float cx = screenX;
    float cy = screenY;
    float displayR = 22.0f;

    // 배경 원 (공 단면)
    glColor3f(0.85f, 0.85f, 0.85f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int k = 0; k <= 36; k++) {
        float a = k * 2.0f * 3.141592f / 36.0f;
        glVertex2f(cx + cos(a) * displayR, cy + sin(a) * displayR);
    }
    glEnd();

    // 원 테두리
    glColor3f(0.3f, 0.3f, 0.3f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 36; k++) {
        float a = k * 2.0f * 3.141592f / 36.0f;
        glVertex2f(cx + cos(a) * displayR, cy + sin(a) * displayR);
    }
    glEnd();

    // 십자선
    glColor3f(0.6f, 0.6f, 0.6f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - displayR, cy); glVertex2f(cx + displayR, cy);
    glVertex2f(cx, cy - displayR); glVertex2f(cx, cy + displayR);
    glEnd();

    // 당점 빨간 점
    float dotX = cx + offsetX * (displayR * 0.85f);
    float dotY = cy + offsetY * (displayR * 0.85f);
    glColor3f(1.0f, 0.1f, 0.1f);
    glPointSize(7.0f);
    glBegin(GL_POINTS);
    glVertex2f(dotX, dotY);
    glEnd();
    glPointSize(1.0f);
    glLineWidth(1.0f);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glEnable(GL_DEPTH_TEST); // ★ 추가: 렌더링 완료 후 깊이 검사 원상복구
}
// --------------------------------------------------------
// 6. 디스플레이
// --------------------------------------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (isTopView) glOrtho(0.0, WINDOW_WIDTH, 0.0, WINDOW_HEIGHT, -100.0, 100.0);
    else gluPerspective(45.0, (double)w / (double)h, 10.0, 2000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (!isTopView) gluLookAt(camEyeX, camEyeY, camEyeZ, camCenterX, camCenterY, camCenterZ, 0.0f, 0.0f, 1.0f);

    // 당구대 바닥
    glDisable(GL_LIGHTING);
    glColor3f(0.0f, 0.4f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(WINDOW_WIDTH, 0.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f); glVertex3f(0.0f, WINDOW_HEIGHT, 0.0f);
    glEnd();

    if (!isTopView) {
        glEnable(GL_LIGHTING);
        glPushMatrix(); glTranslatef(400.0f, 200.0f, -11.0f); glScalef(800.0f, 400.0f, 20.0f);
        glColor3f(0.2f, 0.1f, 0.05f); glutSolidCube(1.0f); glPopMatrix();

        glColor3f(0.4f, 0.2f, 0.1f);
        glPushMatrix(); glTranslatef(400.0f, 415.0f, 5.0f); glScalef(860.0f, 30.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(400.0f, -15.0f, 5.0f); glScalef(860.0f, 30.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(-15.0f, 200.0f, 5.0f); glScalef(30.0f, 400.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(815.0f, 200.0f, 5.0f); glScalef(30.0f, 400.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();

        glColor3f(0.15f, 0.07f, 0.03f);
        float legZ = -70.0f; float legH = 100.0f;
        glPushMatrix(); glTranslatef(20.0f, 20.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(780.0f, 20.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(20.0f, 380.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(780.0f, 380.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
    }

    // 조준선
    if (isCueVisible) {
        float rad = cueAngle * 3.141592f / 180.0f;
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex3f(balls[0].pos.x, balls[0].pos.y, isTopView ? 0.0f : balls[0].radius);
        glVertex3f(balls[0].pos.x + cos(rad) * 1000.0f, balls[0].pos.y + sin(rad) * 1000.0f, isTopView ? 0.0f : balls[0].radius);
        glEnd();
    }

    if (!isTopView) glEnable(GL_LIGHTING);

    // 공 렌더링 (스핀 시각화: 자전 회전 적용)
    for (size_t i = 0; i < balls.size(); i++) {
        glPushMatrix();
        glTranslatef(balls[i].pos.x, balls[i].pos.y, isTopView ? 0.0f : balls[i].radius);

        // ★ 진행 방향 축으로 자전 (스핀 시각화)
        float speed = balls[i].vel.length();
        if (speed > 0.1f) {
            Vec2 dir = balls[i].vel.normalize();
            // 진행방향 수직축 기준으로 회전
            glRotatef(balls[i].rotAngle, -dir.y, dir.x, 0.0f);
        }

        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        glutSolidSphere(balls[i].radius, 32, 32);
        glPopMatrix();
    }

    // 큐대 렌더링
    if (isCueVisible) {
        glPushMatrix();
        glTranslatef(balls[0].pos.x, balls[0].pos.y, isTopView ? 0.0f : balls[0].radius);
        glRotatef(cueAngle, 0, 0, 1);

        // 당점 표시 빨간 점 (큐 끝)
        glPushMatrix();
        glTranslatef(-balls[0].radius, hitOffsetX * 10.0f, hitOffsetY * 10.0f);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.0f, 0.0f);
        glutSolidSphere(2.0f, 10, 10);
        if (!isTopView) glEnable(GL_LIGHTING);
        glPopMatrix();

        float pullBackDist = balls[0].radius + 5.0f;
        if (isStriking) pullBackDist += strikeAnimationOffset;
        else pullBackDist += (cuePower * 2.0f);

        glTranslatef(-pullBackDist - 75.0f, 0.0f, 0.0f);
        glColor3f(0.6f, 0.3f, 0.1f);
        glScalef(150.0f, 3.0f, 3.0f);
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // ★ HUD: 당점 인디케이터 + 조작 가이드
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    if (isCueVisible) {
        // 당점 인디케이터 (우상단)
        drawHitPointIndicator(w - 50, h - 50, hitOffsetX, hitOffsetY);

        // 레이블
        glColor3f(1.0f, 1.0f, 1.0f);
        drawText2D(w - 90, h - 20, "Hit Point");

        // 스핀 상태 텍스트
        char spinInfo[64];
        if (std::abs(hitOffsetY) < 0.05f && std::abs(hitOffsetX) < 0.05f)
            sprintf(spinInfo, "Center");
        else if (hitOffsetY > 0.3f)
            sprintf(spinInfo, "Top Spin");
        else if (hitOffsetY < -0.3f)
            sprintf(spinInfo, "Back Spin");
        else if (hitOffsetX > 0.3f)
            sprintf(spinInfo, "Right Spin");
        else if (hitOffsetX < -0.3f)
            sprintf(spinInfo, "Left Spin");
        else
            sprintf(spinInfo, "Mixed");

        glColor3f(1.0f, 1.0f, 0.2f);
        drawText2D(w - 82, h - 90, spinInfo);

        // 파워 바
        char powerStr[32];
        sprintf(powerStr, "Power: %.0f%%", (cuePower / 30.0f) * 100.0f);
        glColor3f(1.0f, 0.6f, 0.2f);
        drawText2D(10, h - 20, powerStr);

        // 조작 가이드
        glColor3f(0.8f, 0.8f, 0.8f);
        drawText2D(10, h - 38, "A/D: Angle  W/S: Power  J/L: Side Spin  I/K: Top/Back Spin");
        drawText2D(10, h - 52, "SPACE: Shoot  V: View  R: Reset");

        // 스핀 설명
        if (hitOffsetY > 0.3f) {
            glColor3f(0.4f, 1.0f, 0.4f);
            drawText2D(10, 12, "Top Spin: rolls further after impact");
        }
        else if (hitOffsetY < -0.3f) {
            glColor3f(1.0f, 0.4f, 0.4f);
            drawText2D(10, 12, "Back Spin: stops or reverses after impact");
        }
        else if (std::abs(hitOffsetX) > 0.3f) {
            glColor3f(0.4f, 0.8f, 1.0f);
            drawText2D(10, 12, "Side Spin: curves the path");
        }
    }

    glutSwapBuffers();
}

void reshape(int w, int h) { glViewport(0, 0, w, h); }

// --------------------------------------------------------
// 7. 입력 처리
// --------------------------------------------------------
void keyboard(unsigned char key, int x, int y) {
    if (key == 'v' || key == 'V') isTopView = !isTopView;
    if (key == 'r' || key == 'R') {
        initBalls();
        cueAngle = 0.0f; cuePower = 5.0f;
        hitOffsetX = 0.0f; hitOffsetY = 0.0f;
        isStriking = false;
    }

    if (isCueVisible && !isStriking) {
        switch (key) {
        case 'a': case 'A': cueAngle += 3.0f; break;
        case 'd': case 'D': cueAngle -= 3.0f; break;
        case 'w': case 'W': if (cuePower < 30.0f) cuePower += 1.5f; break;
        case 's': case 'S': if (cuePower > 2.0f) cuePower -= 1.5f; break;
            // ★ J/L: 좌우 당점 (사이드 스핀)
        case 'j': case 'J': if (hitOffsetX > -1.0f) hitOffsetX -= 0.1f; break;
        case 'l': case 'L': if (hitOffsetX < 1.0f) hitOffsetX += 0.1f; break;
            // ★ I/K: 상하 당점 (탑/백 스핀)
        case 'i': case 'I': if (hitOffsetY < 1.0f) hitOffsetY += 0.1f; break;
        case 'k': case 'K': if (hitOffsetY > -1.0f) hitOffsetY -= 0.1f; break;
        case ' ':
            isStriking = true;
            strikeAnimationOffset = cuePower * 2.0f;
            break;
        }
    }

    if (!isTopView) {
        float zoomSpeed = 25.0f;
        float dx = camCenterX - camEyeX;
        float dy = camCenterY - camEyeY;
        float dz = camCenterZ - camEyeZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        switch (key) {
        case '+': case '=':
            if (dist > 100.0f) {
                camEyeX += (dx / dist) * zoomSpeed;
                camEyeY += (dy / dist) * zoomSpeed;
                camEyeZ += (dz / dist) * zoomSpeed;
            }
            break;
        case '-': case '_':
            if (dist < 2000.0f) {
                camEyeX -= (dx / dist) * zoomSpeed;
                camEyeY -= (dy / dist) * zoomSpeed;
                camEyeZ -= (dz / dist) * zoomSpeed;
            }
            break;
        }
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    float zSpeed = 15.0f;
    float angleSpeed = 0.05f;

    if (!isTopView) {
        float dx = camEyeX - camCenterX;
        float dy = camEyeY - camCenterY;
        float radius = std::sqrt(dx * dx + dy * dy);
        float currentAngle = std::atan2(dy, dx);

        switch (key) {
        case GLUT_KEY_UP:    camEyeZ += zSpeed; break;
        case GLUT_KEY_DOWN:  camEyeZ -= zSpeed; break;
        case GLUT_KEY_LEFT:
            currentAngle -= angleSpeed;
            camEyeX = camCenterX + radius * std::cos(currentAngle);
            camEyeY = camCenterY + radius * std::sin(currentAngle);
            break;
        case GLUT_KEY_RIGHT:
            currentAngle += angleSpeed;
            camEyeX = camCenterX + radius * std::cos(currentAngle);
            camEyeY = camCenterY + radius * std::sin(currentAngle);
            break;
        }
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("3D Billiard - Spin Physics");

    glEnable(GL_DEPTH_TEST);
    initBalls();
    initLighting();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16, updatePhysics, 0);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glutMainLoop();
    return 0;
}