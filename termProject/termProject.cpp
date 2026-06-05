#define _CRT_SECURE_NO_WARNINGS
#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <iostream>
#include <cstdio>

// --------------------------------------------------------
// 1. 수학 및 물리 구조체 정의
// --------------------------------------------------------

// 2D 벡터 구조체 - 위치, 속도, 방향 등 모든 2차원 값에 사용
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator-(const Vec2& other) const { return Vec2(x - other.x, y - other.y); }
    Vec2 operator+(const Vec2& other) const { return Vec2(x + other.x, y + other.y); }
    Vec2 operator*(float scalar) const { return Vec2(x * scalar, y * scalar); }

    // 내적으로 충돌 시 법선 방향의 상대 속도 성분 계산에 사용
    float dot(const Vec2& other) const { return x * other.x + y * other.y; }

    // 벡터의 크기(길이) 계산  
    float length() const { return std::sqrt(x * x + y * y); }

    // 단위 벡터 반환 - 방향만 남기고 크기를 1로 정규화
    Vec2 normalize() const {
        float len = length();
        if (len > 0) return Vec2(x / len, y / len);
        return Vec2(0, 0);
    }
};

// 공 하나의 물리 및 렌더링 상태를 담는 구조체
struct Ball {
    Vec2 pos;           // 현재 위치
    Vec2 vel;           // 현재 속도 벡터
    float radius = 15.0f;
    float mass = 1.0f;
    float r = 1.0f, g = 1.0f, b = 1.0f; // 공 색상

    float spinX = 0.0f;    // 사이드 스핀 (좌우 회전)
    float spinY = 0.0f;    // 탑/백 스핀 (전후 회전)
    float rotAngle = 0.0f; // 렌더링용 누적 자전 각도

    // 공이 정지했을 때도 마지막 이동 방향을 유지해 올바른 자전 표현에 사용
    Vec2 lastDir = Vec2(1.0f, 0.0f);
};

// --------------------------------------------------------
// 2. 전역 변수 설정
// --------------------------------------------------------
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 400;
const float FRICTION = 0.99f;       // 매 프레임 속도에 곱해지는 감속 계수 (1.0이면 마찰 없음)
const float SPIN_FRICTION = 0.97f;  // 스핀에 적용되는 별도 감속 계수 (속도보다 더 빨리 감소)

std::vector<Ball> balls;

// 카메라 위치와 바라보는 지점 - 3D 시점 전환 시 gluLookAt에 사용
float camEyeX = 400.0f; float camEyeY = -150.0f; float camEyeZ = 500.0f;
float camCenterX = 400.0f; float camCenterY = 200.0f; float camCenterZ = 0.0f;
bool isTopView = true; // 탑뷰(직교 투영) / 3D 원근 투영 전환 플래그

// 큐대 상태 변수
bool isCueVisible = true;          // 공이 모두 멈춰야 큐대 표시
float cueAngle = 0.0f;             // 큐대 조준 각도 (도 단위)
float cuePower = 5.0f;             // 타격 세기
float hitOffsetX = 0.0f;           // 당점 수평 오프셋 (-1 ~ 1, 사이드 스핀 결정)
float hitOffsetY = 0.0f;           // 당점 수직 오프셋 (-1 ~ 1, 탑/백 스핀 결정)
bool isStriking = false;           // 타격 애니메이션 진행 중 플래그
float strikeAnimationOffset = 0.0f; // 큐대 전진 애니메이션 거리

// 화면 상단에 표시할 메시지 및 표시 지속 시간
char hudMsg[128] = "";
int hudMsgTimer = 0;

// --------------------------------------------------------
// 3. 초기화 및 조명 설정
// --------------------------------------------------------

// OpenGL 고정 파이프라인 조명 초기화
// GL_LIGHT0 하나를 사용하며, 위치/주변광/확산광을 설정
void initLighting() {
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL); // glColor 호출이 재질 색상에도 반영되도록 활성화

    GLfloat lightPos[] = { 400.0f, 200.0f, 600.0f, 1.0f }; // w=1: 위치 광원
    GLfloat ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };        // 주변광 강도
    GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };        // 확산광 강도

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
}

// 공 초기 배치 - 흰 공 1개(좌측)와 빨간 공 3개(우측 삼각 배열)
void initBalls() {
    balls.clear();

    Ball whiteBall;
    whiteBall.pos = Vec2(200.0f, 200.0f);
    balls.push_back(whiteBall);

    float targetX = 500.0f;
    float targetY = 200.0f;
    for (int i = 0; i < 3; i++) {
        Ball redBall;
        // 홀수/짝수 인덱스에 따라 Y 오프셋을 주어 삼각형 형태로 배치
        redBall.pos = Vec2(targetX + (i * 32.0f), targetY + (i % 2 == 0 ? 0 : 20.0f));
        redBall.r = 1.0f; redBall.g = 0.2f; redBall.b = 0.2f;
        balls.push_back(redBall);
    }
}

// --------------------------------------------------------
// 4. 물리 엔진 로직
// --------------------------------------------------------

// 두 공의 충돌 감지 및 충격량 기반 속도 분배 처리
// 물리 기반 탄성 충돌 공식 적용: j = -(1+e) * (v_rel · n) / (1/m1 + 1/m2)
void resolveCollision(Ball& b1, Ball& b2) {
    Vec2 delta = b1.pos - b2.pos;     // 두 공 중심 간 벡터
    float distance = delta.length();
    float minDistance = b1.radius + b2.radius;

    // 두 공의 반지름 합보다 거리가 작으면 충돌로 판정
    if (distance < minDistance) {
        // 거리가 0인 경우(완전히 겹침) 수치 오류 방지
        if (distance == 0.0f) { delta = Vec2(1.0f, 0.0f); distance = 1.0f; }

        float overlap = minDistance - distance; // 겹친 깊이
        Vec2 normal = delta.normalize();        // 충돌 법선 방향 (b2 -> b1)
        float totalMass = b1.mass + b2.mass;

        // 질량 비율에 따라 겹침 보정 - 가벼운 공이 더 많이 밀려남
        b1.pos = b1.pos + normal * (overlap * (b2.mass / totalMass));
        b2.pos = b2.pos - normal * (overlap * (b1.mass / totalMass));

        Vec2 relativeVelocity = b1.vel - b2.vel;
        float velocityAlongNormal = relativeVelocity.dot(normal);

        // 두 공이 이미 멀어지는 방향이면 충격량 계산 불필요
        if (velocityAlongNormal > 0) return;

        float e = 0.95f; // 반발 계수 (1.0이면 완전 탄성, 0이면 완전 비탄성)

        // 충격량 크기 계산: j = -(1+e) * (v_rel · n) / (1/m1 + 1/m2)
        // 질량 역수의 합은 충돌 시 각 공의 속도 변화량 배분 비율을 결정
        float j = -(1.0f + e) * velocityAlongNormal;
        j /= (1.0f / b1.mass + 1.0f / b2.mass);

        // 충격량을 질량에 반비례하여 각 공의 속도에 반영
        Vec2 impulse = normal * j;
        b1.vel = b1.vel + impulse * (1.0f / b1.mass);
        b2.vel = b2.vel - impulse * (1.0f / b2.mass);

        // 충돌 시 스핀의 일부를 맞은 공에 전달 (실제 당구의 전달 스핀 효과 모사)
        float spinTransfer = 0.3f;
        b2.spinX += b1.spinX * spinTransfer;
        b1.spinX *= (1.0f - spinTransfer);
    }
}

// 큐 타격 시 당점 오프셋에 따라 초기 속도 및 스핀 설정
// offsetX: 사이드 스핀 양 (-1 ~ 1), offsetY: 탑/백 스핀 양 (-1 ~ 1)
void applyHitSpinPhysics(Ball& ball, float angle_rad, float power, float offsetX, float offsetY) {
    // 타격 각도에 따라 X/Y 방향 초기 속도 분해 (삼각함수 속도 분해)
    float vx = cos(angle_rad) * power;
    float vy = sin(angle_rad) * power;

    // 당점이 중심에서 벗어날수록 스핀 증가 - power에도 비례해 강하게 칠수록 스핀도 강해짐
    ball.spinX = offsetX * power * 0.4f;
    ball.spinY = offsetY * power * 0.5f;

    ball.vel.x = vx;
    ball.vel.y = vy;
}

// 매 프레임 호출되는 물리 업데이트 함수
// 스핀 효과 적용 -> 마찰 감속 -> 위치 갱신 -> 벽 반사 -> 공 간 충돌 처리 순으로 실행
void updatePhysics(int value) {
    // 타격 애니메이션: 큐대를 앞으로 전진시키다가 임계점 도달 시 실제 타격 적용
    if (isStriking) {
        strikeAnimationOffset -= 4.0f;
        if (strikeAnimationOffset <= -2.0f) {
            float rad = cueAngle * 3.141592f / 180.0f;
            applyHitSpinPhysics(balls[0], rad, cuePower, hitOffsetX, hitOffsetY);

            isStriking = false;
            cuePower = 5.0f;
            hitOffsetX = 0.0f;
            hitOffsetY = 0.0f;
        }
    }

    bool isAnyMoving = false;

    for (size_t i = 0; i < balls.size(); i++) {
        Ball& b = balls[i];

        float speed = b.vel.length();
        if (speed > 0.1f) {
            Vec2 dir = b.vel.normalize();

            // 사이드 스핀: 이동 방향의 수직 벡터(-dy, dx)로 횡력 적용 -> 곡선 궤적 생성
            if (std::abs(b.spinX) > 0.01f) {
                float curveForce = b.spinX * 0.06f;
                b.vel.x += (-dir.y) * curveForce;
                b.vel.y += (dir.x) * curveForce;
            }

            // 탑/백 스핀: 이동 방향과 동일/반대 방향으로 가속 또는 감속
            if (std::abs(b.spinY) > 0.01f) {
                float spinEffect = b.spinY * 0.05f;
                b.vel.x += dir.x * spinEffect;
                b.vel.y += dir.y * spinEffect;
            }
        }

        // 스핀 감쇠 - 매 프레임 SPIN_FRICTION 비율로 감소
        b.spinX *= SPIN_FRICTION;
        b.spinY *= SPIN_FRICTION;
        // 스핀이 충분히 작아지면 0으로 고정해 잔진동 방지
        if (std::abs(b.spinX) < 0.001f) b.spinX = 0.0f;
        if (std::abs(b.spinY) < 0.001f) b.spinY = 0.0f;

        // 속도 감쇠 (바닥 마찰)
        b.vel.x *= FRICTION;
        b.vel.y *= FRICTION;

        // 속도가 임계값 이하이면 완전 정지 처리해 미끄러짐 방지
        if (std::abs(b.vel.x) < 0.05f) b.vel.x = 0;
        if (std::abs(b.vel.y) < 0.05f) b.vel.y = 0;

        if (b.vel.length() > 0.1f) isAnyMoving = true;

        // 이동 중인 경우 이동 거리에 비례해 자전 각도 누적, 마지막 방향도 갱신
        if (b.vel.length() > 0.01f) {
            b.rotAngle += b.vel.length() * 2.0f;
            b.lastDir = b.vel.normalize();
        }

        // 위치 적분: 현재 속도만큼 위치 이동 (오일러 적분)
        b.pos.x += b.vel.x;
        b.pos.y += b.vel.y;

        // 벽 충돌 - 법선 방향 속도 반전, 사이드 스핀도 부분 반전 (에너지 손실 반영)
        if (b.pos.x - b.radius < 0) {
            b.pos.x = b.radius; b.vel.x = -b.vel.x; b.spinX = -b.spinX * 0.7f;
        }
        else if (b.pos.x + b.radius > WINDOW_WIDTH) {
            b.pos.x = WINDOW_WIDTH - b.radius; b.vel.x = -b.vel.x; b.spinX = -b.spinX * 0.7f;
        }
        if (b.pos.y - b.radius < 0) {
            b.pos.y = b.radius; b.vel.y = -b.vel.y; b.spinX = -b.spinX * 0.7f;
        }
        else if (b.pos.y + b.radius > WINDOW_HEIGHT) {
            b.pos.y = WINDOW_HEIGHT - b.radius; b.vel.y = -b.vel.y; b.spinX = -b.spinX * 0.7f;
        }
    }

    // 모든 공 쌍에 대해 충돌 검사 및 해소
    for (size_t i = 0; i < balls.size(); i++) {
        for (size_t j = i + 1; j < balls.size(); j++) {
            resolveCollision(balls[i], balls[j]);
        }
    }

    // 모든 공이 정지했을 때만 큐대 표시
    isCueVisible = !isAnyMoving && !isStriking;
    if (hudMsgTimer > 0) hudMsgTimer--;

    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0); // 약 60fps 타이머 재등록
}

// --------------------------------------------------------
// 5. HUD 텍스트 렌더링
// --------------------------------------------------------

// 2D 좌표(x, y)에 비트맵 문자열을 렌더링하는 함수
// 3D 장면 위에 겹쳐 그리기 위해 투영 행렬을 2D 직교 투영으로 임시 전환 후 복원
void drawText2D(float x, float y, const char* text) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluOrtho2D(0, w, 0, h); // 픽셀 단위 2D 좌표계 설정
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

    glEnable(GL_DEPTH_TEST);
}

// 화면 우측 하단에 당점 선택 UI를 렌더링하는 함수
// 원형 영역 안에 십자선과 현재 당점 위치(빨간 점)를 표시
// screenX, screenY: UI 중심 화면 좌표 / offsetX, offsetY: 당점 오프셋 (-1 ~ 1)
void drawHitPointIndicator(float screenX, float screenY, float offsetX, float offsetY) {
    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);

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
    float displayR = 22.0f; // UI 원 반지름 (픽셀)

    // 배경 흰 원 (TRIANGLE_FAN으로 채운 원)
    glColor3f(0.85f, 0.85f, 0.85f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx, cy);
    for (int k = 0; k <= 36; k++) {
        float a = k * 2.0f * 3.141592f / 36.0f;
        glVertex2f(cx + cos(a) * displayR, cy + sin(a) * displayR);
    }
    glEnd();

    // 테두리 원
    glColor3f(0.3f, 0.3f, 0.3f);
    glLineWidth(1.5f);
    glBegin(GL_LINE_LOOP);
    for (int k = 0; k < 36; k++) {
        float a = k * 2.0f * 3.141592f / 36.0f;
        glVertex2f(cx + cos(a) * displayR, cy + sin(a) * displayR);
    }
    glEnd();

    // 중심 십자선
    glColor3f(0.6f, 0.6f, 0.6f);
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    glVertex2f(cx - displayR, cy); glVertex2f(cx + displayR, cy);
    glVertex2f(cx, cy - displayR); glVertex2f(cx, cy + displayR);
    glEnd();

    // 현재 당점 위치 - 오프셋을 UI 반지름 85%로 스케일링해 원 내부에 표시
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

    glEnable(GL_DEPTH_TEST);
}

// --------------------------------------------------------
// 6. 디스플레이
// --------------------------------------------------------

// 매 프레임 장면 전체를 렌더링하는 함수
// 당구대 -> 3D 구조물(3D 시점 한정) -> 조준선 -> 공 -> 큐대 -> HUD 순서로 렌더링
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // 탑뷰: 직교 투영 / 3D 시점: 원근 투영 (FOV 45도)
    if (isTopView) glOrtho(0.0, WINDOW_WIDTH, 0.0, WINDOW_HEIGHT, -100.0, 100.0);
    else gluPerspective(45.0, (double)w / (double)h, 10.0, 2000.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    // 3D 시점일 때만 카메라 변환 적용, 탑뷰는 기본 행렬 사용
    if (!isTopView) gluLookAt(camEyeX, camEyeY, camEyeZ, camCenterX, camCenterY, camCenterZ, 0.0f, 0.0f, 1.0f);

    // 당구대 바닥면 (녹색 사각형)
    glDisable(GL_LIGHTING);
    glColor3f(0.0f, 0.4f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(WINDOW_WIDTH, 0.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f); glVertex3f(0.0f, WINDOW_HEIGHT, 0.0f);
    glEnd();

    // 3D 테이블 구조물 - 3D 시점 전용 (두께감 있는 테이블판, 쿠션 4면, 다리 4개)
    if (!isTopView) {
        glEnable(GL_LIGHTING);

        // 테이블 판 본체
        glPushMatrix(); glTranslatef(400.0f, 200.0f, -11.0f); glScalef(800.0f, 400.0f, 20.0f);
        glColor3f(0.2f, 0.1f, 0.05f); glutSolidCube(1.0f); glPopMatrix();

        // 쿠션 4면 (상/하/좌/우)
        glColor3f(0.4f, 0.2f, 0.1f);
        glPushMatrix(); glTranslatef(400.0f, 415.0f, 5.0f); glScalef(860.0f, 30.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(400.0f, -15.0f, 5.0f); glScalef(860.0f, 30.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(-15.0f, 200.0f, 5.0f); glScalef(30.0f, 400.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(815.0f, 200.0f, 5.0f); glScalef(30.0f, 400.0f, 20.0f); glutSolidCube(1.0f); glPopMatrix();

        // 테이블 다리 4개 (각 모서리)
        glColor3f(0.15f, 0.07f, 0.03f);
        float legZ = -70.0f; float legH = 100.0f;
        glPushMatrix(); glTranslatef(20.0f, 20.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(780.0f, 20.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(20.0f, 380.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
        glPushMatrix(); glTranslatef(780.0f, 380.0f, legZ); glScalef(30.0f, 30.0f, legH); glutSolidCube(1.0f); glPopMatrix();
    }

    // 큐대 조준선 - 흰 공 중심에서 cueAngle 방향으로 긴 선 표시
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

    // 공 렌더링
    // 공마다 glRotatef로 이동 방향 기준 자전을 적용하고, 납작한 점 6개를 표면에 배치
    for (size_t i = 0; i < balls.size(); i++) {
        glPushMatrix();
        glTranslatef(balls[i].pos.x, balls[i].pos.y, isTopView ? 0.0f : balls[i].radius);

        // 자전 적용: 이동 방향의 수직 벡터(-lastDir.y, lastDir.x, 0)를 축으로 rotAngle만큼 회전
        // 이렇게 하면 공이 구르는 방향으로 자연스럽게 자전
        glRotatef(balls[i].rotAngle, -balls[i].lastDir.y, balls[i].lastDir.x, 0.0f);

        // 공 본체 구 렌더링
        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        glutSolidSphere(balls[i].radius, 32, 32);

        // 공 표면 마커 - 6방향(±X, ±Y, ±Z)에 납작한 타원형 스티커 형태로 배치
        // glScalef로 한 축을 극도로 납작하게 압축해 표면에 붙은 점처럼 보이게 함
        glDisable(GL_LIGHTING);
        glColor3f(0.0f, 0.0f, 0.0f);

        float rad = balls[i].radius;
        float offset = rad + 0.15f; // 구 표면 바로 위에 위치
        float mRad = 3.0f;          // 마커 기본 반지름

        // dotParams 구조: [위치X, 위치Y, 위치Z, 스케일X, 스케일Y, 스케일Z]
        // 해당 축 방향으로 배치되는 마커는 그 축을 0.05배로 납작하게 눌러 스티커 형태 구현
        float dotParams[6][6] = {
            { offset, 0, 0,    0.05f, 1.0f, 1.0f },  // +X면
            { -offset, 0, 0,   0.05f, 1.0f, 1.0f },  // -X면
            { 0, offset, 0,    1.0f, 0.05f, 1.0f },  // +Y면
            { 0, -offset, 0,   1.0f, 0.05f, 1.0f },  // -Y면
            { 0, 0, offset,    1.0f, 1.0f, 0.05f },  // +Z면
            { 0, 0, -offset,   1.0f, 1.0f, 0.05f }   // -Z면
        };

        for (int j = 0; j < 6; j++) {
            glPushMatrix();
            glTranslatef(dotParams[j][0], dotParams[j][1], dotParams[j][2]);
            glScalef(dotParams[j][3], dotParams[j][4], dotParams[j][5]);
            glutSolidSphere(mRad, 10, 10);
            glPopMatrix();
        }
        if (!isTopView) glEnable(GL_LIGHTING);

        glPopMatrix();
    }

    // 큐대 렌더링 - 흰 공 위치에서 cueAngle 방향 반대쪽으로 막대기 배치
    if (isCueVisible) {
        glPushMatrix();
        glTranslatef(balls[0].pos.x, balls[0].pos.y, isTopView ? 0.0f : balls[0].radius);
        glRotatef(cueAngle, 0, 0, 1);

        // 당점 위치를 나타내는 빨간 작은 구 - 공 앞면에서 hitOffset만큼 이동한 위치
        glPushMatrix();
        glTranslatef(-balls[0].radius, hitOffsetX * 10.0f, hitOffsetY * 10.0f);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.0f, 0.0f);
        glutSolidSphere(2.0f, 10, 10);
        if (!isTopView) glEnable(GL_LIGHTING);
        glPopMatrix();

        // 큐대 위치: 기본 거리(radius + 5) + 파워에 비례한 풀백 거리
        // 타격 중에는 strikeAnimationOffset으로 전진 애니메이션 적용
        float pullBackDist = balls[0].radius + 5.0f;
        if (isStriking) pullBackDist += strikeAnimationOffset;
        else pullBackDist += (cuePower * 2.0f);

        glTranslatef(-pullBackDist - 75.0f, 0.0f, 0.0f);
        glColor3f(0.6f, 0.3f, 0.1f);
        glScalef(150.0f, 3.0f, 3.0f); // 얇고 긴 큐대 막대
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    // HUD 렌더링 - 당점 표시 UI, 스핀 종류, 파워, 조작 안내
    if (isCueVisible) {
        drawHitPointIndicator(w - 50, h - 50, hitOffsetX, hitOffsetY);

        glColor3f(1.0f, 1.0f, 1.0f);
        drawText2D(w - 90, h - 20, "Hit Point");

        // 당점 오프셋 크기에 따라 스핀 종류 분류 표시
        char spinInfo[64];
        if (std::abs(hitOffsetY) < 0.05f && std::abs(hitOffsetX) < 0.05f)
            sprintf(spinInfo, "Center");
        else if (hitOffsetY > 0.3f) sprintf(spinInfo, "Top Spin");
        else if (hitOffsetY < -0.3f) sprintf(spinInfo, "Back Spin");
        else if (hitOffsetX > 0.3f) sprintf(spinInfo, "Right Spin");
        else if (hitOffsetX < -0.3f) sprintf(spinInfo, "Left Spin");
        else sprintf(spinInfo, "Mixed");

        glColor3f(1.0f, 1.0f, 0.2f);
        drawText2D(w - 82, h - 90, spinInfo);

        // 파워 백분율 표시: cuePower 범위(0~30)를 0~100%로 환산
        char powerStr[32];
        sprintf(powerStr, "Power: %.0f%%", (cuePower / 30.0f) * 100.0f);
        glColor3f(1.0f, 0.6f, 0.2f);
        drawText2D(10, h - 20, powerStr);

        glColor3f(0.8f, 0.8f, 0.8f);
        drawText2D(10, h - 38, "A/D: Angle  W/S: Power  J/L: Side Spin  I/K: Top/Back Spin");
        drawText2D(10, h - 52, "SPACE: Shoot  V: View  R: Reset");
    }

    glutSwapBuffers(); // 더블 버퍼 교환으로 화면 갱신 (깜빡임 방지)
}

// 창 크기 변경 시 뷰포트를 새 창 크기에 맞춰 재설정
void reshape(int w, int h) { glViewport(0, 0, w, h); }

// --------------------------------------------------------
// 7. 입력 처리
// --------------------------------------------------------

// 일반 키보드 입력 처리
// 공 정지 상태에서만 큐대 조작 가능, 이동/줌은 3D 시점 전용
void keyboard(unsigned char key, int x, int y) {
    // V: 탑뷰와 3D 원근 시점 전환
    if (key == 'v' || key == 'V') isTopView = !isTopView;
    // R: 공과 큐대 상태 초기화
    if (key == 'r' || key == 'R') {
        initBalls();
        cueAngle = 0.0f; cuePower = 5.0f;
        hitOffsetX = 0.0f; hitOffsetY = 0.0f;
        isStriking = false;
    }

    // 큐대 조작 입력 - 공이 모두 정지하고 타격 중이 아닐 때만 유효
    if (isCueVisible && !isStriking) {
        switch (key) {
        case 'a': case 'A': cueAngle += 3.0f; break;  // 반시계 방향 조준
        case 'd': case 'D': cueAngle -= 3.0f; break;  // 시계 방향 조준
        case 'w': case 'W': if (cuePower < 30.0f) cuePower += 1.5f; break; // 파워 증가
        case 's': case 'S': if (cuePower > 2.0f) cuePower -= 1.5f; break;  // 파워 감소
        case 'j': case 'J': if (hitOffsetX > -1.0f) hitOffsetX -= 0.1f; break; // 당점 좌로
        case 'l': case 'L': if (hitOffsetX < 1.0f) hitOffsetX += 0.1f; break;  // 당점 우로
        case 'i': case 'I': if (hitOffsetY < 1.0f) hitOffsetY += 0.1f; break;  // 당점 위로 (탑스핀)
        case 'k': case 'K': if (hitOffsetY > -1.0f) hitOffsetY -= 0.1f; break; // 당점 아래로 (백스핀)
        case ' ':
            // SPACE: 타격 애니메이션 시작 - 초기 오프셋은 파워에 비례
            isStriking = true;
            strikeAnimationOffset = cuePower * 2.0f;
            break;
        }
    }

    // 3D 시점 전용 줌 조작 - 카메라와 타겟 사이 방향 벡터를 따라 이동
    if (!isTopView) {
        float zoomSpeed = 25.0f;
        float dx = camCenterX - camEyeX;
        float dy = camCenterY - camEyeY;
        float dz = camCenterZ - camEyeZ;
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        switch (key) {
        case '+': case '=':
            // 줌 인: 최소 거리 100 이상일 때만 전진
            if (dist > 100.0f) {
                camEyeX += (dx / dist) * zoomSpeed;
                camEyeY += (dy / dist) * zoomSpeed;
                camEyeZ += (dz / dist) * zoomSpeed;
            }
            break;
        case '-': case '_':
            // 줌 아웃: 최대 거리 2000 이하일 때만 후진
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

// 방향키 등 특수 키 입력 처리 - 3D 시점에서 카메라 궤도 회전 및 높이 조절
// 카메라가 테이블 중심을 중심점으로 원형 궤도를 이동하는 구면 좌표계 방식 적용
void specialKeys(int key, int x, int y) {
    float zSpeed = 15.0f;
    float angleSpeed = 0.05f; // 라디안 단위 회전 속도

    if (!isTopView) {
        float dx = camEyeX - camCenterX;
        float dy = camEyeY - camCenterY;
        float radius = std::sqrt(dx * dx + dy * dy); // 카메라의 수평 궤도 반지름
        float currentAngle = std::atan2(dy, dx);     // 현재 카메라의 수평 각도 (atan2: 4사분면 처리)

        switch (key) {
        case GLUT_KEY_UP:    camEyeZ += zSpeed; break;  // 카메라 높이 증가
        case GLUT_KEY_DOWN:  camEyeZ -= zSpeed; break;  // 카메라 높이 감소
        case GLUT_KEY_LEFT:
            // 카메라를 반시계 방향으로 궤도 이동 - 극좌표 -> 직교 좌표 변환
            currentAngle -= angleSpeed;
            camEyeX = camCenterX + radius * std::cos(currentAngle);
            camEyeY = camCenterY + radius * std::sin(currentAngle);
            break;
        case GLUT_KEY_RIGHT:
            // 카메라를 시계 방향으로 궤도 이동
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
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // 더블 버퍼, RGB, 깊이 버퍼 사용
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("202112246_term_project");

    glEnable(GL_DEPTH_TEST); // 깊이 테스트 활성화 (원근감 있는 가림 처리)
    initBalls();
    initLighting();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys);
    glutTimerFunc(16, updatePhysics, 0);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // 배경색 (어두운 회색)
    glutMainLoop();
    return 0;
}