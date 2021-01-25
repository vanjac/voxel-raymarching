#ifndef MYGLWIDGET_H
#define MYGLWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLDebugLogger>
#include <QOpenGLExtraFunctions>
//#include <QOpenGLFunctions_3_3_Core>

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

private slots:
    void handleLoggedMessage(const QOpenGLDebugMessage &message);

private:
    GLuint pointsVAO;
    GLuint arrayBuffer;
    GLuint texture;
    GLuint program;
    GLuint timerQuery;
    GLint modelViewLoc, projectionLoc;
    int frame = 0;

    QOpenGLDebugLogger logger;
};

#endif // MYGLWIDGET_H
