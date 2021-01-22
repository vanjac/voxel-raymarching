#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      glWidget(this)
{
    resize(640, 480);
    setCentralWidget(&glWidget);
}

MainWindow::~MainWindow()
{ }
