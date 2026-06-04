#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <iostream>

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
};

// --------------------------------------------------------
// 2. 전역 변수 설정 (카메라 및 큐대 제어)
// --------------------------------------------------------
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 400;
const float FRICTION = 0.99f;

std::vector<Ball> balls;

// 카메라 제어 변수
float camEyeX = 400.0f; float camEyeY = -150.0f; float camEyeZ = 500.0f;
float camCenterX = 400.0f; float camCenterY = 200.0f; float camCenterZ = 0.0f;
bool isTopView = true;

// ★ 큐대 제어 변수 추가
bool isCueVisible = true;    // 공이 모두 멈췄을 때만 큐대 표시
float cueAngle = 0.0f;       // 타격 각도 (도 단위)
float cuePower = 5.0f;       // 타격 힘 (당기는 정도)
float hitOffsetX = 0.0f;     // 좌우 당점 (-1.0 ~ 1.0)
float hitOffsetY = 0.0f;     // 상하 당점 (-1.0 ~ 1.0)

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
    }
}

void updatePhysics(int value) {
    bool isAnyMoving = false;

    for (size_t i = 0; i < balls.size(); i++) {
        balls[i].vel.x *= FRICTION; balls[i].vel.y *= FRICTION;

        if (std::abs(balls[i].vel.x) < 0.05f) balls[i].vel.x = 0;
        if (std::abs(balls[i].vel.y) < 0.05f) balls[i].vel.y = 0;

        if (balls[i].vel.length() > 0.1f) isAnyMoving = true;

        balls[i].pos.x += balls[i].vel.x; balls[i].pos.y += balls[i].vel.y;

        // 벽면 충돌
        if (balls[i].pos.x - balls[i].radius < 0) { balls[i].pos.x = balls[i].radius; balls[i].vel.x = -balls[i].vel.x; }
        else if (balls[i].pos.x + balls[i].radius > WINDOW_WIDTH) { balls[i].pos.x = WINDOW_WIDTH - balls[i].radius; balls[i].vel.x = -balls[i].vel.x; }
        if (balls[i].pos.y - balls[i].radius < 0) { balls[i].pos.y = balls[i].radius; balls[i].vel.y = -balls[i].vel.y; }
        else if (balls[i].pos.y + balls[i].radius > WINDOW_HEIGHT) { balls[i].pos.y = WINDOW_HEIGHT - balls[i].radius; balls[i].vel.y = -balls[i].vel.y; }
    }

    for (size_t i = 0; i < balls.size(); i++) {
        for (size_t j = i + 1; j < balls.size(); j++) {
            resolveCollision(balls[i], balls[j]);
        }
    }

    // ★ 모든 공이 멈췄을 때만 큐대를 다시 표시
    isCueVisible = !isAnyMoving;

    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0);
}

// --------------------------------------------------------
// 5. 디스플레이 (큐대 및 당점 렌더링 추가)
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

    // 당구대 바닥 그리기
    glDisable(GL_LIGHTING);
    glColor3f(0.0f, 0.4f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(WINDOW_WIDTH, 0.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f); glVertex3f(0.0f, WINDOW_HEIGHT, 0.0f);
    glEnd();

    // ★ 큐대 조준선(가이드라인) 그리기
    if (isCueVisible) {
        float rad = cueAngle * 3.141592f / 180.0f;
        glColor3f(1.0f, 1.0f, 1.0f);
        glBegin(GL_LINES);
        glVertex3f(balls[0].pos.x, balls[0].pos.y, isTopView ? 0.0f : balls[0].radius);
        glVertex3f(balls[0].pos.x + cos(rad) * 1000.0f, balls[0].pos.y + sin(rad) * 1000.0f, isTopView ? 0.0f : balls[0].radius);
        glEnd();
    }

    if (!isTopView) glEnable(GL_LIGHTING);

    // 공 그리기
    for (size_t i = 0; i < balls.size(); i++) {
        glPushMatrix();
        glTranslatef(balls[i].pos.x, balls[i].pos.y, isTopView ? 0.0f : balls[i].radius);
        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        glutSolidSphere(balls[i].radius, 32, 32);
        glPopMatrix();
    }

    // ★ 큐대 및 당점(마커) 그리기
    if (isCueVisible) {
        glPushMatrix();
        glTranslatef(balls[0].pos.x, balls[0].pos.y, isTopView ? 0.0f : balls[0].radius);
        glRotatef(cueAngle, 0, 0, 1); // 큐대 조준 방향으로 회전

        // 1. 당점 표시기 (공 껍질에 맺히는 빨간 점)
        // 큐대는 -X축 방향에 있으므로, 마커는 -X축 껍질 쪽에 위치시킴
        glPushMatrix();
        glTranslatef(-balls[0].radius, hitOffsetX * 10.0f, hitOffsetY * 10.0f);
        glDisable(GL_LIGHTING);
        glColor3f(1.0f, 0.0f, 0.0f); // 빨간색 마커
        glutSolidSphere(2.0f, 10, 10);
        if (!isTopView) glEnable(GL_LIGHTING);
        glPopMatrix();

        // 2. 큐대 본체 그리기 (당기는 힘에 따라 뒤로 이동)
        // 기본 거리 + 당기는 힘(cuePower)에 비례해서 멀어짐
        float pullBackDist = balls[0].radius + 5.0f + (cuePower * 2.0f);
        glTranslatef(-pullBackDist - 75.0f, 0.0f, 0.0f); // 75.0f는 큐대 길이의 절반

        glColor3f(0.6f, 0.3f, 0.1f); // 나무색
        glScalef(150.0f, 3.0f, 3.0f); // 길쭉한 막대기 형태
        glutSolidCube(1.0f);
        glPopMatrix();
    }

    glutSwapBuffers();
}

void reshape(int w, int h) { glViewport(0, 0, w, h); }

// --------------------------------------------------------
// 6. 사용자 입력 처리 (큐대 전용 조작 추가)
// --------------------------------------------------------
void keyboard(unsigned char key, int x, int y) {
    if (key == 'v' || key == 'V') isTopView = !isTopView;

    if (isCueVisible) {
        switch (key) {
            // [조준 각도] A/D 키로 좌우 회전
        case 'a': case 'A': cueAngle += 3.0f; break;
        case 'd': case 'D': cueAngle -= 3.0f; break;

            // [파워 조절] W/S 키로 큐대 당기기 / 밀기
        case 'w': case 'W': if (cuePower < 30.0f) cuePower += 1.5f; break;
        case 's': case 'S': if (cuePower > 2.0f) cuePower -= 1.5f; break;

            // [당점 조절] I/K/J/L 키로 공의 타격 부위 미세 조절
        case 'j': case 'J': if (hitOffsetX > -1.0f) hitOffsetX -= 0.1f; break;
        case 'l': case 'L': if (hitOffsetX < 1.0f) hitOffsetX += 0.1f; break;
        case 'i': case 'I': if (hitOffsetY < 1.0f) hitOffsetY += 0.1f; break;
        case 'k': case 'K': if (hitOffsetY > -1.0f) hitOffsetY -= 0.1f; break;

            // [타격 실행] 스페이스바
        case ' ':
            // 1. 조준 각도를 라디안으로 변환
            float rad = cueAngle * 3.141592f / 180.0f;

            // 2. 당점에 따른 미세 움직임 보정 (Simplified Physics)
            // 좌우 당점(hitOffsetX)을 주면 공이 미세하게 휘어 나가는 스쿼트(Squirt) 현상 시뮬레이션
            float actualAngle = rad + (hitOffsetX * 0.15f);

            // 상하 당점(hitOffsetY)을 주면 굴러가는 속도가 미세하게 가속/감속됨
            float actualForce = cuePower * (1.0f + (hitOffsetY * 0.2f));

            // 3. 수구(하얀 공)에 최종 속도 적용
            balls[0].vel.x = cos(actualAngle) * actualForce;
            balls[0].vel.y = sin(actualAngle) * actualForce;

            // 4. 파워 및 당점 초기화
            cuePower = 5.0f;
            hitOffsetX = 0.0f;
            hitOffsetY = 0.0f;
            break;
        }
    }
    glutPostRedisplay();
}

// --------------------------------------------------------
// 6. 사용자 입력 처리 (특수 키 - 방향키)
// --------------------------------------------------------
void specialKeys(int key, int x, int y) {
    float speed = 15.0f;

    // 3D 뷰 모드일 때만 카메라 방향키 조작 허용
    if (!isTopView) {
        switch (key) {
            // 기존 Y축 이동을 제거하고, 기존 W/S의 Z축(높낮이) 조절 기능으로 교체
        case GLUT_KEY_UP:    camEyeZ += speed; break;
        case GLUT_KEY_DOWN:  camEyeZ -= speed; break;

            // 좌우 이동은 그대로 유지
        case GLUT_KEY_LEFT:  camEyeX -= speed; break;
        case GLUT_KEY_RIGHT: camEyeX += speed; break;
        }
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("3D Billiard - Cue Stick Interaction");

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