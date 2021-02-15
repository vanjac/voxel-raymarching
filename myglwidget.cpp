#include "myglwidget.h"
#include <QOpenGLContext>
#include <QFile>
#include "opengllog.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const GLsizei NUM_FRAME_VERTS = 6;
const GLuint VERT_POSITION_LOC = 0;
const GLuint VERT_UV_LOC = 1;

const float FLY_SPEED = 0.05f;

const glm::vec3 CAM_FORWARD(1, 0, 0);
const glm::vec3 CAM_RIGHT(0, -1, 0);
const glm::vec3 CAM_UP(0, 0, 1);

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
    linkProgramCheck(program, "Program");
    glUseProgram(program);
    // clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    getProgramUniforms(program);

    glGenVertexArrays(1, &frameVAO);
    glBindVertexArray(frameVAO);

    // just a rectangle to fill the screen
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

    // UV coordinates specify a portion of normalized device coordinates
    // changes with the aspect ratio in resizeGL()
    glGenBuffers(1, &frameUVBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, frameUVBuffer);
    // default aspect ratio (1, 1)
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(VERT_UV_LOC, 2, GL_FLOAT,
                          GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(VERT_UV_LOC);

    {
        VoxLoader voxload(":/minecraft.vox");
        voxload.load();
        uploadVoxelData(voxload.pack);
    }

    // default uniform values
    glm::vec3 ambientColor = glm::vec3(58, 75, 105) / 255.0f;
    glUniform3f(ambientColorLoc, ambientColor.r, ambientColor.g, ambientColor.b);
    glm::vec3 sunDir = glm::normalize(glm::vec3(2, 1, -3));
    glUniform3f(sunDirLoc, sunDir.x, sunDir.y, sunDir.z);
    glm::vec3 sunColor = 1.5f * glm::vec3(252, 255, 213) / 255.0f;
    glUniform3f(sunColorLoc, sunColor.x, sunColor.y, sunColor.z);
    glUniform3f(pointLightPosLoc, 8.5, 8.5, 3.5);
    glm::vec3 pointColor = 100.0f * glm::vec3(255, 16, 8) / 255.0f;
    glUniform3f(pointLightColorLoc, pointColor.r, pointColor.g, pointColor.b);
    glUniform1f(pointLightRangeLoc, 64.0f);

    // used to measure frame time
    glGenQueries(1, &timerQuery);
}

void MyGLWidget::getProgramUniforms(GLuint program)
{
    modelLoc = glGetUniformLocation(program, "Model");
    paletteLoc = glGetUniformLocation(program, "Palette");
    blockDimLoc = glGetUniformLocation(program, "BlockDim");
    camPosLoc = glGetUniformLocation(program, "CamPos");
    camDirLoc = glGetUniformLocation(program, "CamDir");
    camULoc = glGetUniformLocation(program, "CamU");
    camVLoc = glGetUniformLocation(program, "CamV");
    pixelSizeLoc = glGetUniformLocation(program, "PixelSize");
    ambientColorLoc = glGetUniformLocation(program, "AmbientColor");
    sunDirLoc = glGetUniformLocation(program, "SunDir");
    sunColorLoc = glGetUniformLocation(program, "SunColor");
    pointLightPosLoc = glGetUniformLocation(program, "PointLightPos");
    pointLightColorLoc = glGetUniformLocation(program, "PointLightColor");
    pointLightRangeLoc = glGetUniformLocation(program, "PointLightRange");
}

#define UDF_INDEX(x, y, z, dim) (((x) + (dim)*(y) + (dim)*(dim)*(z)) * 2)

bool IsFilled(unsigned char *udfVoxData, int dim, int zOffset,
              int cx, int cy, int cz, int size, int value)
{
    int minX = cx - size, minY = cy - size, minZ = cz - size;
    int maxX = cx + size, maxY = cy + size, maxZ = cz + size;
    for (int z = minZ; z <= maxZ; z++) {
        for (int y = minY; y <= maxY; y++) {
            for (int x = minX; x <= maxX; x++) {
                if (glm::length(glm::vec3( glm::max(glm::abs(x - cx) - 1, 0),
                                           glm::max(glm::abs(y - cy) - 1, 0),
                                           glm::max(glm::abs(z - cz) - 1, 0) ))
                        >= size + 0.01)
                    continue;
                int index = UDF_INDEX((x + dim) % dim, (y + dim) % dim,
                                      (z + dim) % dim + zOffset, dim);
                if (udfVoxData[index] != value)
                    return false;
            }
        }
    }
    return true;
}

void MyGLWidget::uploadVoxelData(const VoxPack &pack)
{
    const VoxModel &model = pack.models[0];

    int numVoxels = model.numVoxels();
    int xDim = model.xDim;

    // distance field stores the minimum distance from the *edge* of this voxel
    // to the *edge* of a voxel of a different value
    unsigned char *udfVoxData = new unsigned char[numVoxels * 2];
    for (int i = 0; i < numVoxels; i++) {
        udfVoxData[i * 2] = model.data[i];
        udfVoxData[i * 2 + 1] = 0;
    }

    // very slow brute force!!
    // assume cubic blocks stacked on the z axis
    // TODO: hardcoded max offset
    for (int blockOffset = 0; blockOffset < 80; blockOffset += xDim) {
        for (int z = 0; z < xDim; z++) {
            for (int y = 0; y < xDim; y++) {
                for (int x = 0; x < xDim; x++) {
                    int index = UDF_INDEX(x, y, z + blockOffset, xDim);
                    int value = udfVoxData[index];
                    int size;
                    for (size = 1; size < xDim; size++) {
                        if (!IsFilled(udfVoxData, xDim, blockOffset,
                                      x, y, z, size, value)) {
                            break;
                        }
                    }
                    size--;
                    udfVoxData[index + 1] = size;
                }
            }
        }
    }


    glGenTextures(1, &modelTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, modelTexture);
    // TODO: if you ever upgrade to OpenGL 4.2 switch to glTexStorage
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RG8UI, model.xDim, model.yDim, model.zDim, 0,
                 GL_RG_INTEGER, GL_UNSIGNED_BYTE, udfVoxData);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    glGenTextures(1, &paletteTexture);
    glActiveTexture(GL_TEXTURE0 + 1);
    glBindTexture(GL_TEXTURE_1D, paletteTexture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA, PALETTE_SIZE, 0,
                 GL_RGBA, GL_FLOAT, pack.palette);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_REPEAT);

    glUniform1i(modelLoc, 0);  // TEXTURE0
    glUniform1i(paletteLoc, 1);  // TEXTURE1
    glUniform1i(blockDimLoc, xDim);  // cube
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

void MyGLWidget::linkProgramCheck(GLuint program, QString name)
{
    glLinkProgram(program);
    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLen;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
        char *log = new char[logLen];
        glGetProgramInfoLog(program, logLen, NULL, log);
        qCritical() << name << "link error:" << log;
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
    // update UV coordinates to match aspect ratio
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

    // move the camera
    camYaw -= glm::radians((float)delta.x() * 0.4);
    camPitch -= glm::radians((float)delta.y() * 0.4);
    float pitchLimit = glm::pi<float>() / 2;
    if (camPitch > pitchLimit)
        camPitch = pitchLimit;
    else if (camPitch < -pitchLimit)
        camPitch = -pitchLimit;
}

void MyGLWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
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
    glBindVertexArray(frameVAO);

    glm::mat4 camMatrix = glm::identity<glm::mat4>();
    camMatrix = glm::rotate(camMatrix, camYaw, CAM_UP);
    camMatrix = glm::rotate(camMatrix, camPitch, CAM_RIGHT);

    // apply velocity
    camPos += camMatrix * glm::vec4(camVelocity * FLY_SPEED, 0);

    glm::vec4 camDir = camMatrix * glm::vec4(CAM_FORWARD, 0);
    glm::vec4 camU = camMatrix * glm::vec4(CAM_RIGHT, 0);
    glm::vec4 camV = camMatrix * glm::vec4(CAM_UP, 0);
    glUniform3f(camPosLoc, camPos.x, camPos.y, camPos.z);
    glUniform3f(camDirLoc, camDir.x, camDir.y, camDir.z);
    glUniform3f(camULoc, camU.x, camU.y, camU.z);
    glUniform3f(camVLoc, camV.x, camV.y, camV.z);
    glUniform1f(pixelSizeLoc, 2.0 / height());

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

    glDrawArrays(GL_TRIANGLES, 0, NUM_FRAME_VERTS);

    if (measureTime)
        glEndQuery(GL_TIME_ELAPSED);

    glFlush();
    frame++;
    update();  // schedule an update to animate
}
