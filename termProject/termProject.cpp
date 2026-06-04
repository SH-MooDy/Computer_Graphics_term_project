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
const int WINDOW_HEIGHT = 400; // 2:1 당구대 비율 고정
const float FRICTION = 0.99f;

std::vector<Ball> balls;

// 3D 카메라 제어 변수
float camEyeX = 400.0f;
float camEyeY = -150.0f;
float camEyeZ = 500.0f;
float camCenterX = 400.0f;
float camCenterY = 200.0f;
float camCenterZ = 0.0f;

bool isTopView = true; // 대다수 사용 편의를 위해 탑뷰를 디폴트(true)로 세팅

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
// 4. 물리 엔진 로직 (벽면 충돌 수치와 화면 수치 일치)
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

        // 윈도우 경계(0, WINDOW_WIDTH, WINDOW_HEIGHT)를 벽면 충돌 기준으로 적용
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
// 5. 핵심 수정: 디스플레이 함수 내 투영행렬 동적 제어
// --------------------------------------------------------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 현재 윈도우의 실시간 해상도 획득
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);

    // [Step 1] 시점 모드에 따른 투영 행렬(Projection) 설정
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    if (isTopView) {
        // ★ 핵심: 직교 투영을 사용하여 당구대 좌표계를 화면 전체 뷰포트에 강제 밀착시킴
        // 이 연산 덕분에 윈도우의 4면 테두리가 좌표상의 (0,0) ~ (WINDOW_WIDTH, WINDOW_HEIGHT)와 일치하게 됨
        glOrtho(0.0, WINDOW_WIDTH, 0.0, WINDOW_HEIGHT, -100.0, 100.0);
    }
    else {
        // 3D 카메라 뷰일 때는 기존 원근 투영 활용
        gluPerspective(45.0, (double)w / (double)h, 10.0, 2000.0);
    }

    // [Step 2] 모델뷰 행렬(ModelView) 설정
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    if (isTopView) {
        // 탑뷰일 때는 2D 스크린 공간 좌표계를 그대로 쓰므로 별도의 gluLookAt 카메라 이동이 필요 없음
    }
    else {
        // 3D 카메라 시점 배치
        gluLookAt(camEyeX, camEyeY, camEyeZ, camCenterX, camCenterY, camCenterZ, 0.0f, 0.0f, 1.0f);
    }

    // [Step 3] 렌더링 시작
    // 당구대 바닥 그리기
    glDisable(GL_LIGHTING); // 탑뷰에서 깔끔한 단색 처리를 위해 조명 오프
    glColor3f(0.0f, 0.4f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, 0.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f);
    glVertex3f(0.0f, WINDOW_HEIGHT, 0.0f);
    glEnd();

    // 3D 뷰 모드일 때만 음영 입체감을 주기 위해 조명 재활성화
    if (!isTopView) glEnable(GL_LIGHTING);

    // 당구공 그리기
    for (size_t i = 0; i < balls.size(); i++) {
        glPushMatrix();
        // 탑뷰일 때는 Z축 튀어나옴 없이 완벽한 평면 원으로 투영되도록 보정
        glTranslatef(balls[i].pos.x, balls[i].pos.y, isTopView ? 0.0f : balls[i].radius);
        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        glutSolidSphere(balls[i].radius, 32, 32);
        glPopMatrix();
    }

    glutSwapBuffers();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    // 투영 행렬은 display()에서 실시간 제어하므로 여기서는 뷰포트 크기만 갱신합니다.
}

// --------------------------------------------------------
// 6. 사용자 입력 처리
// --------------------------------------------------------
void keyboard(unsigned char key, int x, int y) {
    float speed = 15.0f;
    switch (key) {
    case ' ': // 공 타격
        balls[0].vel.x = 23.0f;
        balls[0].vel.y = 3.5f;
        break;
    case 'v': case 'V': // 'V' 키를 누르면 탑뷰 <-> 3D뷰 완벽 전환
        isTopView = !isTopView;
        break;
    case 'w': case 'W': if (!isTopView) camEyeZ += speed; break;
    case 's': case 'S': if (!isTopView) camEyeZ -= speed; break;
    }
    glutPostRedisplay();
}

void specialKeys(int key, int x, int y) {
    float speed = 15.0f;
    if (!isTopView) {
        switch (key) {
        case GLUT_KEY_UP:    camEyeY += speed; break;
        case GLUT_KEY_DOWN:  camEyeY -= speed; break;
        case GLUT_KEY_LEFT:  camEyeX -= speed; break;
        case GLUT_KEY_RIGHT: camEyeX += speed; break;
        }
        glutPostRedisplay();
    }
}

int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT); // 800x400 당구대 비율로 최초 윈도우창 생성
    glutCreateWindow("3D Billiard - Top View 4 Sides Match");

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