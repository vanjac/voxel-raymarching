#include "myglwidget.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include "opengllog.h"
#include <glm/glm.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

const GLuint NUM_VERTICES = 16 * 256;

const GLuint VERT_POSITION_LOC = 0;

const float PROJ_FOV = glm::radians(60.0f);
const float PROJ_NEAR = 0.1;
const float PROJ_FAR = 100;

const GLchar *vertexShaderSrc = R"X(
#version 330 core

layout(location = 0) in vec3 position;

void main()
{
    gl_Position = vec4(position, 1.0);
}
)X";

const GLchar *geometryShaderSrc = R"X(
#version 330 core

layout (points) in;
layout (triangle_strip, max_vertices = 24) out;
out vec4 Color;

uniform mat4 ModelView;
uniform mat4 Projection;

uniform sampler3D Model;

vec4 transform(vec4 pos)
{
    return Projection * ModelView * pos;
}

void quad(vec4 origin, vec4 u, vec4 v)
{
    //Color = u + v;
    gl_Position = transform(origin);
    EmitVertex();
    gl_Position = transform(origin + u);
    EmitVertex();
    gl_Position = transform(origin + v);
    EmitVertex();
    gl_Position = transform(origin + u + v);
    EmitVertex();
    EndPrimitive();
}

void main()
{
    vec4 pos = gl_in[0].gl_Position;

    vec4 c = texture(Model, pos.zxy / 16);
    if (c.a < 0.5)
        return;
    Color = c;

    quad(pos + vec4(1, 0, 0, 0), vec4(0, 1, 0, 0), vec4(0, 0, 1, 0));
    quad(pos, vec4(0, 0, 1, 0), vec4(0, 1, 0, 0));
    quad(pos + vec4(0, 1, 0, 0), vec4(0, 0, 1, 0), vec4(1, 0, 0, 0));
    quad(pos, vec4(1, 0, 0, 0), vec4(0, 0, 1, 0));
    quad(pos + vec4(0, 0, 1, 0), vec4(1, 0, 0, 0), vec4(0, 1, 0, 0));
    quad(pos, vec4(0, 1, 0, 0), vec4(1, 0, 0, 0));
}

)X";

const GLchar *fragmentShaderSrc = R"X(
#version 330 core

in vec4 Color;
out vec4 fColor;

void main()
{
    fColor = Color;
}
)X";

MyGLWidget::MyGLWidget(QWidget *parent)
    : QOpenGLWidget(parent),
      logger(this)
{
    // simpler behavior
    setUpdateBehavior(UpdateBehavior::PartialUpdate);
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

    glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    glGenVertexArrays(1, &pointsVAO);
    glBindVertexArray(pointsVAO);

    GLfloat vertices[NUM_VERTICES][3];

    for (int z = 0; z < 16; z++) {
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++) {
                vertices[x + y * 16 + z * 256][0] = x;
                vertices[x + y * 16 + z * 256][1] = y;
                vertices[x + y * 16 + z * 256][2] = z;
            }
        }
    }

    glGenBuffers(1, &arrayBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, arrayBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    GLuint geometryShader = glCreateShader(GL_GEOMETRY_SHADER);
    glShaderSource(geometryShader, 1, &geometryShaderSrc, nullptr);
    glCompileShader(geometryShader);

    program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glAttachShader(program, geometryShader);
    glLinkProgram(program);
    glUseProgram(program);
    // clean up
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(geometryShader);

    glVertexAttribPointer(VERT_POSITION_LOC, 3, GL_FLOAT,
                          GL_FALSE, 0, (void *)0);
    glEnableVertexAttribArray(VERT_POSITION_LOC);

    modelViewLoc = glGetUniformLocation(program, "ModelView");
    projectionLoc = glGetUniformLocation(program, "Projection");
    GLint modelLoc = glGetUniformLocation(program, "Model");

    // default aspect ratio 1
    resizeGL(1, 1);

    QImage voxModel(":/chr_knight.png");
    const uchar *voxData = voxModel.constBits();

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);
    // TODO: if you ever upgrade to OpenGL 4.2 switch to glTexStorage
    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA, 16, 16, 16, 0,
                 GL_BGRA, GL_UNSIGNED_BYTE, voxData);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);

    glUniform1i(modelLoc, 0);

    glGenQueries(1, &timerQuery);
}

void MyGLWidget::handleLoggedMessage(const QOpenGLDebugMessage &message)
{
    logGLMessage(message);
}

void MyGLWidget::resizeGL(int w, int h)
{
    glm::mat4 projectionMat = glm::perspective(PROJ_FOV, (float)w / h, PROJ_NEAR, PROJ_FAR);
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projectionMat));
}

void MyGLWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // important to call early

    // animate

    glm::mat4 modelViewMat = glm::identity<glm::mat4>();
    modelViewMat = glm::translate(modelViewMat, glm::vec3(0,0,-5));
    modelViewMat = glm::scale(modelViewMat, glm::one<glm::vec3>() / 10.0f);
    modelViewMat = glm::rotate(modelViewMat, glm::radians(20.0f), glm::vec3(1,0,0));
    modelViewMat = glm::rotate(modelViewMat, glm::radians(frame * 2.0f), glm::vec3(0,1,0));
    glUniformMatrix4fv(modelViewLoc, 1, GL_FALSE, glm::value_ptr(modelViewMat));

    // render
    glBindVertexArray(pointsVAO);

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

    glDrawArrays(GL_POINTS, 0, NUM_VERTICES);

    if (measureTime)
        glEndQuery(GL_TIME_ELAPSED);

    glFlush();
    frame++;
    update();  // schedule an update to animate
}
