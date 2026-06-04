#include <GL/glut.h>
#include <cmath>
#include <iostream>

// 2D 벡터 구조체
struct Vec2 {
    float x, y;
    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}
};

// 공 구조체
struct Ball {
    Vec2 pos;
    Vec2 vel;
    float radius;
    float mass;
    float r, g, b; // 색상
};

// 전역 변수 설정
const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 400; // 당구대 비율 2:1 
const float FRICTION = 0.99f;  // 마찰력 계수 

Ball whiteBall;

// 공 초기화
void initBall() {
    whiteBall.pos = Vec2(200.0f, 200.0f);
    whiteBall.vel = Vec2(0.0f, 0.0f);
    whiteBall.radius = 15.0f;
    whiteBall.mass = 1.0f;
    whiteBall.r = 1.0f; whiteBall.g = 1.0f; whiteBall.b = 1.0f;
}

// 원 그리기 함수
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

// 물리 업데이트 함수
void updatePhysics(int value) {
    // 1. 마찰력 적용 (속도 감쇠) 
    whiteBall.vel.x *= FRICTION;
    whiteBall.vel.y *= FRICTION;

    // 속도가 너무 작아지면 완전 정지
    if (std::abs(whiteBall.vel.x) < 0.05f) whiteBall.vel.x = 0;
    if (std::abs(whiteBall.vel.y) < 0.05f) whiteBall.vel.y = 0;

    // 2. 위치 업데이트
    whiteBall.pos.x += whiteBall.vel.x;
    whiteBall.pos.y += whiteBall.vel.y;

    // 3. 벽면 충돌 처리 (윈도우 4면이 당구대 벽면) 
    if (whiteBall.pos.x - whiteBall.radius < 0) {
        whiteBall.pos.x = whiteBall.radius;
        whiteBall.vel.x = -whiteBall.vel.x; // 반사
    }
    else if (whiteBall.pos.x + whiteBall.radius > WINDOW_WIDTH) {
        whiteBall.pos.x = WINDOW_WIDTH - whiteBall.radius;
        whiteBall.vel.x = -whiteBall.vel.x;
    }

    if (whiteBall.pos.y - whiteBall.radius < 0) {
        whiteBall.pos.y = whiteBall.radius;
        whiteBall.vel.y = -whiteBall.vel.y;
    }
    else if (whiteBall.pos.y + whiteBall.radius > WINDOW_HEIGHT) {
        whiteBall.pos.y = WINDOW_HEIGHT - whiteBall.radius;
        whiteBall.vel.y = -whiteBall.vel.y;
    }

    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0); // 약 60 FPS
}

// 렌더링 함수 (Top View) 
void display() {
    glClear(GL_COLOR_BUFFER_BIT);

    // 당구대 바닥 그리기 (초록색)
    glColor3f(0.0f, 0.5f, 0.0f);
    glBegin(GL_QUADS);
    glVertex2f(0.0f, 0.0f);
    glVertex2f(WINDOW_WIDTH, 0.0f);
    glVertex2f(WINDOW_WIDTH, WINDOW_HEIGHT);
    glVertex2f(0.0f, WINDOW_HEIGHT);
    glEnd();

    // 공 그리기
    glColor3f(whiteBall.r, whiteBall.g, whiteBall.b);
    drawCircle(whiteBall.pos.x, whiteBall.pos.y, whiteBall.radius, 30);

    glutSwapBuffers();
}

// 화면 투영 설정
void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, WINDOW_WIDTH, 0, WINDOW_HEIGHT);
    glMatrixMode(GL_MODELVIEW);
}

// 키보드 입력 (임시 큐대 타격 기능)
void keyboard(unsigned char key, int x, int y) {
    if (key == ' ') {
        // 스페이스바를 누르면 우측 상단으로 힘을 가함
        whiteBall.vel.x = 15.0f;
        whiteBall.vel.y = 10.0f;
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
    glutCreateWindow("3D Billiard Game - Top View");

    initBall();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutTimerFunc(16, updatePhysics, 0);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glutMainLoop();
    return 0;
}