#ifndef MYGLWIDGET_H
#define MYGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
//#include <QOpenGLFunctions_3_3_Core>
#include <QByteArray>
#include <QMouseEvent>
#include <QKeyEvent>
#include <glm/glm.hpp>

#include "voxloader.h"

class MyGLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    MyGLWidget(QWidget *parent);
    ~MyGLWidget();

protected:
    // QOpenGLWidget events
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    // general widget events
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

    // get the locations of each uniform
    void getProgramUniforms(GLuint program);
    void uploadVoxelData(const VoxPack &pack);

    // OpenGL helper functions
    void compileShaderCheck(GLuint shader, QString name);
    void linkProgramCheck(GLuint program, QString name);
    // Qt IO helper function
    QByteArray loadStringResource(QString filename);

private slots:
    void handleLoggedMessage(const QOpenGLDebugMessage &message);

private:
    GLuint frameVAO;
    GLuint framePosBuffer, frameUVBuffer;
    GLuint modelBuffer, modelTexture, paletteTexture;
    GLuint program;
    GLuint timerQuery;
    // shader uniform locations
    GLint modelLoc, paletteLoc, blockDimLoc;
    GLint camPosLoc, camDirLoc, camULoc, camVLoc, pixelSizeLoc;
    GLint ambientColorLoc, sunDirLoc, sunColorLoc;
    GLint pointLightPosLoc, pointLightColorLoc, pointLightRangeLoc;

    int frame = 0;
    bool trackMouse = false;
    QPoint prevMousePos;
    float camYaw = 0, camPitch = 0;
    glm::vec4 camPos = glm::vec4(8,8,8,1);
    glm::vec3 camVelocity = glm::vec3(0,0,0);

    QOpenGLDebugLogger logger;
};

#endif // MYGLWIDGET_H
