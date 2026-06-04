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

// 큐대 제어 변수 추가
bool isCueVisible = true;    // 공이 모두 멈췄을 때만 큐대 표시
float cueAngle = 0.0f;       // 타격 각도 (도 단위)
float cuePower = 5.0f;       // 타격 힘 (당기는 정도)
float hitOffsetX = 0.0f;     // 좌우 당점 (-1.0 ~ 1.0)
float hitOffsetY = 0.0f;     // 상하 당점 (-1.0 ~ 1.0)
bool isStriking = false;          // 현재 타격 애니메이션 중인지 여부
float strikeAnimationOffset = 0.0f; // 타격 시 큐대가 이동하는 거리

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
    // 1. 타격 애니메이션 처리 
    if (isStriking) {
        // 프레임마다 큐대가 앞으로 4.0f씩 빠르게 전진
        strikeAnimationOffset -= 4.0f;

        // 큐대가 공 위치에 도달했을 때 (팔로우 스루 느낌을 위해 -2.0f까지 허용)
        if (strikeAnimationOffset <= -2.0f) {
            float rad = cueAngle * 3.141592f / 180.0f;
            float actualAngle = rad + (hitOffsetX * 0.15f);
            float actualForce = cuePower * (1.0f + (hitOffsetY * 0.2f));

            // 드디어 공에 힘 적용
            balls[0].vel.x = cos(actualAngle) * actualForce;
            balls[0].vel.y = sin(actualAngle) * actualForce;

            // 상태 초기화
            isStriking = false;
            cuePower = 5.0f;
            hitOffsetX = 0.0f;
            hitOffsetY = 0.0f;
        }
    }

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

    // 모든 공이 멈췄을 때만 큐대를 다시 표시
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

    // 큐대 조준선(가이드라인) 그리기
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

    // 큐대 및 당점(마커) 그리기
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

        // 2. 큐대 본체 그리기 
        // 기본 여백
        float pullBackDist = balls[0].radius + 5.0f;

        // 타격 중일 때는 애니메이션 오프셋 사용, 조준 중일 때는 파워 사용
        if (isStriking) {
            pullBackDist += strikeAnimationOffset;
        }
        else {
            pullBackDist += (cuePower * 2.0f);
        }

        glTranslatef(-pullBackDist - 75.0f, 0.0f, 0.0f); // 75.0f는 큐대 길이의 절반

        glColor3f(0.6f, 0.3f, 0.1f); // 나무색
        glScalef(150.0f, 3.0f, 3.0f);
        glutSolidCube(1.0f);
        glPopMatrix();
    } // if (isCueVisible) 끝부분

    glutSwapBuffers();
}

void reshape(int w, int h) { glViewport(0, 0, w, h); }

// --------------------------------------------------------
// 6. 사용자 입력 처리 (키보드 일반 키)
// --------------------------------------------------------
void keyboard(unsigned char key, int x, int y) {
    if (key == 'v' || key == 'V') isTopView = !isTopView;

    // 1. 큐대 조작 (공이 멈춰있을 때만 작동)
    if (isCueVisible) {
        switch (key) {
        case 'a': case 'A': cueAngle += 3.0f; break;
        case 'd': case 'D': cueAngle -= 3.0f; break;
        case 'w': case 'W': if (cuePower < 30.0f) cuePower += 1.5f; break;
        case 's': case 'S': if (cuePower > 2.0f) cuePower -= 1.5f; break;
        case 'j': case 'J': if (hitOffsetX > -1.0f) hitOffsetX -= 0.1f; break;
        case 'l': case 'L': if (hitOffsetX < 1.0f) hitOffsetX += 0.1f; break;
        case 'i': case 'I': if (hitOffsetY < 1.0f) hitOffsetY += 0.1f; break;
        case 'k': case 'K': if (hitOffsetY > -1.0f) hitOffsetY -= 0.1f; break;
        case ' ':
            // 스페이스바를 누르면 타격 애니메이션 시작!
            isStriking = true;
            // 애니메이션 시작 위치 = 현재 큐대를 뒤로 당긴 만큼의 거리
            strikeAnimationOffset = cuePower * 2.0f;
            break;
        }
    }

    // 2. 카메라 줌 인/아웃 (3D 뷰일 때만 작동)
    if (!isTopView) {
        float zoomSpeed = 25.0f;

        // 카메라에서 당구대 중심을 향하는 3D 방향 벡터 계산
        float dx = camCenterX - camEyeX;
        float dy = camCenterY - camEyeY;
        float dz = camCenterZ - camEyeZ;

        // 현재 카메라와 중심점 사이의 실제 3D 직선 거리 계산
        float dist = std::sqrt(dx * dx + dy * dy + dz * dz);

        switch (key) {
        case '+': case '=': // 줌 인 (시선을 따라 앞으로 직진)
            if (dist > 100.0f) { // 너무 뚫고 들어가지 않도록 제한
                camEyeX += (dx / dist) * zoomSpeed;
                camEyeY += (dy / dist) * zoomSpeed;
                camEyeZ += (dz / dist) * zoomSpeed;
            }
            break;

        case '-': case '_': // 줌 아웃 (시선을 따라 뒤로 후진)
            if (dist < 2000.0f) { // 너무 멀어지지 않도록 제한
                camEyeX -= (dx / dist) * zoomSpeed;
                camEyeY -= (dy / dist) * zoomSpeed;
                camEyeZ -= (dz / dist) * zoomSpeed;
            }
            break;
        }
    }

    glutPostRedisplay();
}

// --------------------------------------------------------
// 6. 사용자 입력 처리 (특수 키 - 궤도 회전 카메라)
// --------------------------------------------------------
void specialKeys(int key, int x, int y) {
    float zSpeed = 15.0f;     // 높낮이 조절 속도
    float angleSpeed = 0.05f; // 궤도 회전 속도 (라디안 단위)

    if (!isTopView) {
        // 1. 현재 바라보는 중심점(Target)을 기준으로 카메라의 상대 위치 계산
        float dx = camEyeX - camCenterX;
        float dy = camEyeY - camCenterY;
        
        // 2. 피타고라스 정리와 아크탄젠트로 현재 카메라의 '거리(반지름)'와 '각도' 추출
        float radius = std::sqrt(dx * dx + dy * dy);
        float currentAngle = std::atan2(dy, dx);

        switch (key) {
        case GLUT_KEY_UP:    
            camEyeZ += zSpeed; // 고도 상승
            break;
        case GLUT_KEY_DOWN:  
            camEyeZ -= zSpeed; // 고도 하강
            break;
        case GLUT_KEY_LEFT:  
            // 왼쪽 화살표: 각도를 줄여서 당구대 주위를 시계 방향(왼쪽)으로 공전
            currentAngle -= angleSpeed; 
            camEyeX = camCenterX + radius * std::cos(currentAngle);
            camEyeY = camCenterY + radius * std::sin(currentAngle);
            break;
        case GLUT_KEY_RIGHT: 
            // 오른쪽 화살표: 각도를 늘려서 당구대 주위를 반시계 방향(오른쪽)으로 공전
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