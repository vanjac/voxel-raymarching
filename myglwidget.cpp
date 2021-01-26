#include "myglwidget.h"
#include <QOpenGLContext>
#include <QFile>
#include "opengllog.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const GLsizei NUM_FRAME_VERTS = 6;
const GLuint VERT_POSITION_LOC = 0;
const GLuint VERT_UV_LOC = 1;

const float FLY_SPEED = 0.2;

const glm::vec3 CAM_FORWARD(1, 0, 0);
const glm::vec3 CAM_RIGHT(0, 0, 1);
const glm::vec3 CAM_UP(0, 1, 0);

MyGLWidget::MyGLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      logger(this)
{
    // simpler behavior
    setUpdateBehavior(UpdateBehavior::PartialUpdate);
    // get mouse move events even if button isn't pressed
    //setMouseTracking(true);
    setFocusPolicy(Qt::FocusPolicy::ClickFocus);
}

MyGLWidget::~MyGLWidget()
{
    makeCurrent();

    logger.stopLogging();
    // etc

    doneCurrent();
}

void MyGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    if (!logger.initialize())
        qWarning() << "OpenGL logging not available! (requires extension KHR_debug)";
    else {
        connect(&logger, &QOpenGLDebugLogger::messageLogged,
                this, &MyGLWidget::handleLoggedMessage);
        logger.startLogging();
    }

    qDebug() << "OpenGL renderer:" << (char *)glGetString(GL_RENDERER);
    qDebug() << "OpenGL version:" << (char *)glGetString(GL_VERSION);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    QByteArray vertexSrcArr = loadStringResource(":/voxelmarch.vert");
    const char *vertexSrc = vertexSrcArr.constData();
    glShaderSource(vertexShader, 1, &vertexSrc, nullptr);
    compileShaderCheck(vertexShader, "Vertex");

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    QByteArray fragmentSrcArr = loadStringResource(":/voxelmarch.frag");
    const char *fragmentSrc = fragmentSrcArr.constData();
    glShaderSource(fragmentShader, 1, &fragmentSrc, nullptr);
    compileShaderCheck(fragmentShader, "Fragment");

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glUseProgram(program);
    // clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    modelLoc = glGetUniformLocation(program, "Model");
    paletteLoc = glGetUniformLocation(program, "Palette");
    camPosLoc = glGetUniformLocation(program, "CamPos");
    camDirLoc = glGetUniformLocation(program, "CamDir");
    camULoc = glGetUniformLocation(program, "CamU");
    camVLoc = glGetUniformLocation(program, "CamV");


    glGenVertexArrays(1, &frameVAO);
    glBindVertexArray(frameVAO);

    GLfloat vertices[NUM_FRAME_VERTS][2] {
        {-1, -1}, {1, -1}, {-1, 1},
        {1, 1}, {-1, 1}, {1, -1}
    };
    glGenBuffers(1, &framePosBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, framePosBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(VERT_POSITION_LOC, 2, GL_FLOAT,
                          GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(VERT_POSITION_LOC);

    glGenBuffers(1, &frameUVBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, frameUVBuffer);
    // default aspect ratio (1, 1)
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(VERT_UV_LOC, 2, GL_FLOAT,
                          GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(VERT_UV_LOC);


    QImage voxModel(":/chr_knight.bmp");
    qDebug() << voxModel.format();
    const uchar *voxData = voxModel.constBits();
    QList<QRgb> palette = voxModel.colorTable();
    const QRgb *paletteData = palette.constData();

    glGenTextures(1, &modelTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, modelTexture);
    // TODO: if you ever upgrade to OpenGL 4.2 switch to glTexStorage
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8I, 16, 16, 16, 0,
                 GL_RED_INTEGER, GL_UNSIGNED_BYTE, voxData);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    glGenTextures(1, &paletteTexture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_1D, paletteTexture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, palette.size(), 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, paletteData);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);


    glUniform1i(modelLoc, 0);
    glUniform1i(paletteLoc, 1);

    glGenQueries(1, &timerQuery);
}

void MyGLWidget::handleLoggedMessage(const QOpenGLDebugMessage &message)
{
    logGLMessage(message);
}

void MyGLWidget::compileShaderCheck(GLuint shader, QString name)
{
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint logLen;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        char *log = new char[logLen];
        glGetShaderInfoLog(shader, logLen, NULL, log);
        qCritical() << name << "shader compile error:" << log;
        exit(EXIT_FAILURE);
    }
}

QByteArray MyGLWidget::loadStringResource(QString filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Error reading file" << filename;
        return QByteArray("");
    }
    return f.readAll();
}

void MyGLWidget::resizeGL(int w, int h)
{
    float aspect = (float)w / h;
    GLfloat uv[NUM_FRAME_VERTS][2] {
        {-aspect, -1}, {aspect, -1}, {-aspect, 1},
        {aspect, 1}, {-aspect, 1}, {aspect, -1}
    };
    glBindBuffer(GL_ARRAY_BUFFER, frameUVBuffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(uv), uv);
}

void MyGLWidget::mouseMoveEvent(QMouseEvent *event)
{
    QPoint pos = event->pos();
    QPoint delta(0, 0);
    if (trackMouse)
        delta = pos - prevMousePos;
    prevMousePos = pos;
    trackMouse = true;

    camYaw -= glm::radians((float)delta.x());
    camPitch -= glm::radians((float)delta.y());
}

void MyGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    trackMouse = false;
}

void MyGLWidget::keyPressEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_W:
        camVelocity += CAM_FORWARD; break;
    case Qt::Key_S:
        camVelocity -= CAM_FORWARD; break;
    case Qt::Key_D:
        camVelocity += CAM_RIGHT; break;
    case Qt::Key_A:
        camVelocity -= CAM_RIGHT; break;
    case Qt::Key_E:
        camVelocity += CAM_UP; break;
    case Qt::Key_Q:
        camVelocity -= CAM_UP; break;
    default:
        QOpenGLWidget::keyPressEvent(event);
    }
}

void MyGLWidget::keyReleaseEvent(QKeyEvent *event)
{
    switch(event->key()) {
    case Qt::Key_W:
        camVelocity -= CAM_FORWARD; break;
    case Qt::Key_S:
        camVelocity += CAM_FORWARD; break;
    case Qt::Key_D:
        camVelocity -= CAM_RIGHT; break;
    case Qt::Key_A:
        camVelocity += CAM_RIGHT; break;
    case Qt::Key_E:
        camVelocity -= CAM_UP; break;
    case Qt::Key_Q:
        camVelocity += CAM_UP; break;
    default:
        QOpenGLWidget::keyReleaseEvent(event);
    }
}

void MyGLWidget::paintGL()
{
    // render
    glBindVertexArray(frameVAO);

    bool measureTime = frame % 60 == 0;
    if (measureTime) {
        // measure render time
        if (frame != 0) {
            GLuint nanoseconds;
            glGetQueryObjectuiv(timerQuery, GL_QUERY_RESULT, &nanoseconds);
            qDebug() << (nanoseconds / 1000) << "us";
        }
        glBeginQuery(GL_TIME_ELAPSED, timerQuery);
    }

    glm::mat4 camMatrix = glm::identity<glm::mat4>();
    camMatrix = glm::rotate(camMatrix, camYaw, CAM_UP);
    camMatrix = glm::rotate(camMatrix, camPitch, CAM_RIGHT);

    camPos += camMatrix * glm::vec4(camVelocity * FLY_SPEED, 0);
    glm::vec4 camDir = camMatrix * glm::vec4(CAM_FORWARD, 0);
    glm::vec4 camU = camMatrix * glm::vec4(CAM_RIGHT, 0);
    glm::vec4 camV = camMatrix * glm::vec4(CAM_UP, 0);
    glUniform3f(camPosLoc, camPos.x, camPos.y, camPos.z);
    glUniform3f(camDirLoc, camDir.x, camDir.y, camDir.z);
    glUniform3f(camULoc, camU.x, camU.y, camU.z);
    glUniform3f(camVLoc, camV.x, camV.y, camV.z);


    glDrawArrays(GL_TRIANGLES, 0, NUM_FRAME_VERTS);

    if (measureTime)
        glEndQuery(GL_TIME_ELAPSED);

    glFlush();
    frame++;
    update();  // schedule an update to animate
}
