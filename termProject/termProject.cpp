#include <GL/glut.h>
#include <cmath>
#include <vector>
#include <iostream>

// --------------------------------------------------------
// 1. 수학 및 물리 구조체 정의 (직관적인 2D 물리)
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
const float FRICTION = 0.99f; // 시원하고 직관적인 감쇠율

std::vector<Ball> balls;

// 카메라 제어 변수
float camEyeX = 400.0f; float camEyeY = -150.0f; float camEyeZ = 500.0f;
float camCenterX = 400.0f; float camCenterY = 200.0f; float camCenterZ = 0.0f;
bool isTopView = true;

// 큐대 및 타격 애니메이션 제어 변수
bool isCueVisible = true;
float cueAngle = 0.0f;
float cuePower = 5.0f;
float hitOffsetX = 0.0f;
float hitOffsetY = 0.0f;
bool isStriking = false;
float strikeAnimationOffset = 0.0f;

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
// 4. 물리 엔진 로직 (간단하고 명확한 멈춤)
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
        strikeAnimationOffset -= 4.0f;
        if (strikeAnimationOffset <= -2.0f) {
            float rad = cueAngle * 3.141592f / 180.0f;
            float actualAngle = rad + (hitOffsetX * 0.15f);
            float actualForce = cuePower * (1.0f + (hitOffsetY * 0.2f));

            balls[0].vel.x = cos(actualAngle) * actualForce;
            balls[0].vel.y = sin(actualAngle) * actualForce;

            isStriking = false;
            cuePower = 5.0f; hitOffsetX = 0.0f; hitOffsetY = 0.0f;
        }
    }

    bool isAnyMoving = false;

    // 2. 공 이동 및 벽면 충돌 처리
    for (size_t i = 0; i < balls.size(); i++) {
        // 일정한 비율로 시원하게 감쇠
        balls[i].vel.x *= FRICTION;
        balls[i].vel.y *= FRICTION;

        // 일정 속도 이하로 떨어지면 억지스럽게 구르지 않고 즉각 정지
        if (std::abs(balls[i].vel.x) < 0.05f) balls[i].vel.x = 0;
        if (std::abs(balls[i].vel.y) < 0.05f) balls[i].vel.y = 0;

        if (balls[i].vel.length() > 0.1f) isAnyMoving = true;

        balls[i].pos.x += balls[i].vel.x; balls[i].pos.y += balls[i].vel.y;

        if (balls[i].pos.x - balls[i].radius < 0) { balls[i].pos.x = balls[i].radius; balls[i].vel.x = -balls[i].vel.x; }
        else if (balls[i].pos.x + balls[i].radius > WINDOW_WIDTH) { balls[i].pos.x = WINDOW_WIDTH - balls[i].radius; balls[i].vel.x = -balls[i].vel.x; }
        if (balls[i].pos.y - balls[i].radius < 0) { balls[i].pos.y = balls[i].radius; balls[i].vel.y = -balls[i].vel.y; }
        else if (balls[i].pos.y + balls[i].radius > WINDOW_HEIGHT) { balls[i].pos.y = WINDOW_HEIGHT - balls[i].radius; balls[i].vel.y = -balls[i].vel.y; }
    }

    // 3. 공 간 충돌 체크
    for (size_t i = 0; i < balls.size(); i++) {
        for (size_t j = i + 1; j < balls.size(); j++) {
            resolveCollision(balls[i], balls[j]);
        }
    }

    // 타격 중도 아니고 움직이는 공도 없을 때만 큐대 표시
    isCueVisible = !isAnyMoving && !isStriking;

    glutPostRedisplay();
    glutTimerFunc(16, updatePhysics, 0);
}

// --------------------------------------------------------
// 5. 디스플레이 (3D 당구대 및 큐대)
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

    glDisable(GL_LIGHTING);
    glColor3f(0.0f, 0.4f, 0.15f);
    glBegin(GL_QUADS);
    glVertex3f(0.0f, 0.0f, 0.0f); glVertex3f(WINDOW_WIDTH, 0.0f, 0.0f);
    glVertex3f(WINDOW_WIDTH, WINDOW_HEIGHT, 0.0f); glVertex3f(0.0f, WINDOW_HEIGHT, 0.0f);
    glEnd();

    // 3D 뷰일 때 테두리(쿠션)와 다리 추가
    if (!isTopView) {
        glEnable(GL_LIGHTING);
        // 깨짐(Z-fighting) 현상을 수정한 위치 (-11.0f)
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

    for (size_t i = 0; i < balls.size(); i++) {
        glPushMatrix();
        glTranslatef(balls[i].pos.x, balls[i].pos.y, isTopView ? 0.0f : balls[i].radius);
        glColor3f(balls[i].r, balls[i].g, balls[i].b);
        glutSolidSphere(balls[i].radius, 32, 32);
        glPopMatrix();
    }

    if (isCueVisible) {
        glPushMatrix();
        glTranslatef(balls[0].pos.x, balls[0].pos.y, isTopView ? 0.0f : balls[0].radius);
        glRotatef(cueAngle, 0, 0, 1);

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

    glutSwapBuffers();
}

void reshape(int w, int h) { glViewport(0, 0, w, h); }

// --------------------------------------------------------
// 6. 입력 처리 (궤도 회전 + 직진 줌인아웃 적용 완료)
// --------------------------------------------------------
void keyboard(unsigned char key, int x, int y) {
    if (key == 'v' || key == 'V') isTopView = !isTopView;

    if (isCueVisible && !isStriking) {
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
    glutCreateWindow("3D Billiard - Arcade Physics & 3D Table");

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