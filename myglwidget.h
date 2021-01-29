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

class MyGLWidget : public QOpenGLWidget, protected QOpenGLExtraFunctions
{
    Q_OBJECT
public:
    MyGLWidget(QWidget *parent);
    ~MyGLWidget();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void handleLoggedMessage(const QOpenGLDebugMessage &message);
    void compileShaderCheck(GLuint shader, QString name);
    void linkProgramCheck(GLuint program, QString name);
    QByteArray loadStringResource(QString filename);

private:
    GLuint frameVAO;
    GLuint framePosBuffer, frameUVBuffer;
    GLuint modelTexture, paletteTexture;
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
