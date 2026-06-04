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
    float r, g, b; // 색상
};

// --------------------------------------------------------
// 2. 전역 변수 및 설정
// --------------------------------------------------------

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 400; // 당구대 비율 2:1
const float FRICTION = 0.99f;  // 바닥 마찰력

std::vector<Ball> balls; // 당구공들을 관리하는 벡터

// --------------------------------------------------------
// 3. 초기화 및 유틸리티 함수
// --------------------------------------------------------

void initBalls() {
    // 수구 (하얀 공) 생성 - 인덱스 0
    Ball whiteBall;
    whiteBall.pos = Vec2(200.0f, 200.0f);
    whiteBall.vel = Vec2(0.0f, 0.0f);
    whiteBall.radius = 15.0f;
    whiteBall.mass = 1.0f;
    whiteBall.r = 1.0f; whiteBall.g = 1.0f; whiteBall.b = 1.0f;
    balls.push_back(whiteBall);

    // 적구들 (빨간 공) 생성
    float targetX = 500.0f;
    float targetY = 200.0f;
    for (int i = 0; i < 3; i++) {
        Ball redBall;
        // 공들이 겹치지 않게 삼각형 형태로 배치
        redBall.pos = Vec2(targetX + (i * 30.0f), targetY + (i % 2 == 0 ? 0 : 20.0f));
        redBall.vel = Vec2(0.0f, 0.0f);
        redBall.radius = 15.0f;
        redBall.mass = 1.0f;
        redBall.r = 1.0f; redBall.g = 0.0f; redBall.b = 0.0f;
        balls.push_back(redBall);
    }
}

void drawCircle(float cx, float cy, float r, int num_segments) {
    glBegin(GL_POLYGON);
    for (int i = 0; i < num_segments; i++) {
        float theta = 2.0f * 3.1415926f * float(i) / float(num_segments);
        float x = r * cosf(theta);
        float y = r * sinf(theta);
        glVertex2f(x + cx, y + cy);
    }
    glEnd();
}

// --------------------------------------------------------
// 4. 물리 엔진 핵심 로직
// --------------------------------------------------------

// 공과 공 사이의 충돌 처리 (위치 보정 + 속도 반사)
void resolveCollision(Ball& b1, Ball& b2) {
    Vec2 delta = b1.pos - b2.pos;
    float distance = delta.length();
    float minDistance = b1.radius + b2.radius;

    if (distance < minDistance) {
        if (distance == 0.0f) {
            delta = Vec2(1.0f, 0.0f);
            distance = 1.0f;
        }

        // [1] 위치 보정 (겹침 방지)
        float overlap = minDistance - distance;
        Vec2 normal = delta.normalize();
        float totalMass = b1.mass + b2.mass;

        b1.pos = b1.pos + normal * (overlap * (b2.mass / totalMass));
        b2.pos = b2.pos - normal * (overlap * (b1.mass / totalMass));

        // [2] 속도 반사 계산 (탄성 충돌)
        Vec2 relativeVelocity = b1.vel - b2.vel;
        float velocityAlongNormal = relativeVelocity.dot(normal);

        // 이미 멀어지고 있다면 무시
        if (velocityAlongNormal > 0) return;

        float e = 0.95f; // 반발 계수 (당구공 느낌)
        float j = -(1.0f + e) * velocityAlongNormal;
        j /= (1.0f / b1.mass + 1.0f / b2.mass);

        Vec2 impulse = normal * j;
        b1.vel = b1.vel + impulse * (1.0f / b1.mass);
        b2.vel = b2.vel - impulse * (1.0f / b2.mass);
    }
}

void updatePhysics(int value) {
    // 1. 모든 공의 위치 이동 및 벽면 충돌 처리
    for (size_t i = 0; i < balls.size(); i++) {
        // 마찰력 적용
        balls[i].vel.x *= FRICTION;
        balls[i].vel.y *= FRICTION;

        // 속도가 매우 낮아지면 정지
        if (std::abs(balls[i].vel.x) < 0.05f) balls[i].vel.x = 0;
        if (std::abs(balls[i].vel.y) < 0.05f) balls[i].vel.y = 0;

        balls[i].pos.x += balls[i].vel.x;
        balls[i].pos.y += balls[i].vel.y;

        // 벽면 충돌
        if (balls[i].pos.x - balls[i].radius < 0) {
            balls[i].pos.x = balls[i].radius;
            balls[i].vel.x = -balls[i].vel.x;
        }
        else if (balls[i].pos.x + balls[i].radius > WINDOW_WIDTH) {
            balls[i].pos.x = WINDOW_WIDTH - balls[i].radius;
            balls[i].vel.x = -balls[i].vel.x;
        }

        if (balls[i].pos.y - balls[i].radius < 0) {
            balls[i].pos.y = balls[i].radius;
            balls[i].vel.y = -balls[i].vel.y;
        }
        else if (balls[i].pos.y + balls[i].radius > WINDOW_HEIGHT) {
            balls[i].pos.y = WINDOW_HEIGHT - balls[i].radius;
            balls[i].vel.y = -balls[i].vel.y;
        }
    }

    // 2. 공과 공 사이의 충돌 체크 (모든 쌍을 비교)
    for (size_t i = 0; i < balls.size(); i++) {
        for (size_t j = i + 1; j < balls.size(); j++) {
            resolveCollision(balls[i], balls[j]);
        }
    }

    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0); // 약 60 FPS로 재귀 호출
}

// --------------------------------------------------------
// 5. 렌더링 및 입력 처리
// --------------------------------------------------------

void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // 바닥 (초록색 당구대)
    glColor3f(0.0f, 0.5f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(WINDOW_WIDTH, 0.0f);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(0.0f, WINDOW_HEIGHT);
    glEnd();

    // 모든 공 그리기
    for (size_t i = 0; i < balls.size(); i++) {
        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        drawCircle(balls[i].pos.x, balls[i].pos.y, balls[i].radius, 30);
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
}

void keyboard(unsigned char key, int x, int y) {
    // 스페이스바를 누르면 하얀 공(인덱스 0)을 타격
    if (key == ' ') {
        balls[0].vel.x = 20.0f; // 우측 방향 힘
        balls[0].vel.y = 5.0f;  // 살짝 위쪽 방향 힘
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("3D Billiard Game - Physics Engine");

    initBalls(); // 공 초기화

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, updatePhysics, 0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glutMainLoop();
    return 0;
}