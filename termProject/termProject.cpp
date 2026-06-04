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
    float radius;
    float mass;
    float r, g, b;
};

// --------------------------------------------------------
// 2. 전역 변수 및 카메라 설정
// --------------------------------------------------------
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 400;
const float FRICTION = 0.99f;

std::vector<Ball> balls;

// 카메라 제어 변수 (초기값: 당구대를 비스듬히 내려다보는 시점)
float camEyeX = 400.0f;
float camEyeY = -150.0f;
float camEyeZ = 500.0f;

// 카메라가 바라보는 중심점 (당구대 정중앙)
float camCenterX = 400.0f;
float camCenterY = 200.0f;
float camCenterZ = 0.0f;

bool isTopView = false; // 탑뷰/3D뷰 전환 플래그

// --------------------------------------------------------
// 3. 초기화 및 조명 설정
// --------------------------------------------------------
void initLighting() {
    glEnable(GL_LIGHTING);     // 조명 활성화
    glEnable(GL_LIGHT0);       // 0번 광원 사용
    glEnable(GL_COLOR_MATERIAL); // 오브젝트 고유 색상 유지

    // 당구대 위쪽에 위치하는 점광원
    GLfloat lightPos[] = { 400.0f, 200.0f, 600.0f, 1.0f };
    GLfloat ambient[] = { 0.3f, 0.3f, 0.3f, 1.0f };
    GLfloat diffuse[] = { 0.8f, 0.8f, 0.8f, 1.0f };

    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
}

void initBalls() {
    balls.clear();
    // 수구 (하얀 공)
    Ball whiteBall;
    whiteBall.pos = Vec2(200.0f, 200.0f);
    whiteBall.vel = Vec2(0.0f, 0.0f);
    whiteBall.radius = 15.0f;
    whiteBall.mass = 1.0f;
    whiteBall.r = 1.0f; whiteBall.g = 1.0f; whiteBall.b = 1.0f;
    balls.push_back(whiteBall);

    // 적구들 (빨간 공)
    float targetX = 500.0f;
    float targetY = 200.0f;
    for (int i = 0; i < 3; i++) {
        Ball redBall;
        redBall.pos = Vec2(targetX + (i * 32.0f), targetY + (i % 2 == 0 ? 0 : 20.0f));
        redBall.vel = Vec2(0.0f, 0.0f);
        redBall.radius = 15.0f;
        redBall.mass = 1.0f;
        redBall.r = 1.0f; redBall.g = 0.2f; redBall.b = 0.2f;
        balls.push_back(redBall);
    }
}

// --------------------------------------------------------
// 4. 물리 엔진 로직 (기존 충돌 유지)
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
    for (size_t i = 0; i < balls.size(); i++) {
        balls[i].vel.x *= FRICTION; balls[i].vel.y *= FRICTION;
        if (std::abs(balls[i].vel.x) < 0.05f) balls[i].vel.x = 0;
        if (std::abs(balls[i].vel.y) < 0.05f) balls[i].vel.y = 0;

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
    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0);
}

// --------------------------------------------------------
// 5. 3D 렌더링 및 카메라 변환
// --------------------------------------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    // 카메라 뷰 설정
    if (isTopView) {
        // 수직 정방향 탑뷰 (Z축 위에서 정중앙을 내려다봄)
        gluLookAt(400.0f, 200.0f, 600.0f, 400.0f, 200.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    }
    else {
        // 사용자가 제어하는 3D 원근 카메라 시점
        gluLookAt(camEyeX, camEyeY, camEyeZ, camCenterX, camCenterY, camCenterZ, 0.0f, 0.0f, 1.0f);
    }

    // [1] 3D 당구대 바닥 렌더링
    glDisable(GL_LIGHTING); // 바닥은 단순 색상 지정을 위해 조명 잠시 오프
    glColor3f(0.0f, 0.4f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, -1.0f);
    glVertex3f(WINDOW_WIDTH, 0.0f, -1.0f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, -1.0f);
    glVertex3f(0.0f, WINDOW_HEIGHT, -1.0f);
    glEnd();
    glEnable(GL_LIGHTING);

    // [2] 3D 입체 공 렌더링
    for (size_t i = 0; i < balls.size(); i++) {
        glPushMatrix();
        // 공의 2D 평면 위치(X, Y)를 3D 공간에 매핑 (Z축은 반지름만큼 띄움)
        glTranslatef(balls[i].pos.x, balls[i].pos.y, balls[i].radius);
        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        glutSolidSphere(balls[i].radius, 32, 32); // 입체 구체 그리기
        glPopMatrix();
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // 3차원 원근 투영 정의 (시야각 45도)
    gluPerspective(45.0, (double)w / (double)h, 10.0, 2000.0);
    glMatrixMode(GL_MODELVIEW);
}

// --------------------------------------------------------
// 6. 사용자 입력 처리 (카메라 제어 및 타격)
// --------------------------------------------------------
void keyboard(unsigned char key, int x, int y) {
    float speed = 15.0f;
    switch (key) {
    case ' ': // 공 타격
        balls[0].vel.x = 22.0f;
        balls[0].vel.y = 4.0f;
        break;
    case 'v': case 'V': // 'V' 키로 탑뷰 / 3D 카메라뷰 전환
        isTopView = !isTopView;
        break;
        // 카메라 높낮이 제어
    case 'w': case 'W': camEyeZ += speed; break;
    case 's': case 'S': camEyeZ -= speed; break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    float speed = 15.0f;
    // 방향키를 이용한 카메라 평면 위치 이동
    switch (key) {
    case GLUT_KEY_UP:    camEyeY += speed; break;
    case GLUT_KEY_DOWN:  camEyeY -= speed; break;
    case GLUT_KEY_LEFT:  camEyeX -= speed; break;
    case GLUT_KEY_RIGHT: camEyeX += speed; break;
    }
    glutPostRedisplay();
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH); // 3D를 위한 깊이 버퍼(DEPTH) 추가
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("3D Billiard Game - Interactive Camera View");

    glEnable(GL_DEPTH_TEST); // 은면 제거 기능 활성화
    initBalls();
    initLighting();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKeys); // 방향키 입력을 위한 콜백
    glutTimerFunc(16, updatePhysics, 0);

    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glutMainLoop();
    return 0;
}