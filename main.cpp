#include "mainwindow.h"
#include "secondarywindow.h"
#include <QApplication>
#include "qdebug.h"
#include "QScreen"
#include <QtAVWidgets>
#include <tcprov.h>
#include <headingwidget.h>

int main(int argc, char *argv[])
{

    QtAV::Widgets::registerRenderers();
    QApplication a(argc, argv);

    // 2 windows code taken from https://stackoverflow.com/questions/34551046/create-two-windows
    MainWindow w1;
    SecondaryWindow w2;

    // The following section will place and rezise the windows according to the screen setup on the host computer
    // If run on only one screen it will automaticly be placed and resized as two "small" windows with the same aspect ratio
    // - as the host's screen.
    // If run on two or more screens it will chose the two first, place the windows accordingly and make them borderless fullscreen.
    QList<QScreen *> qList = QApplication::screens();

    //Debug: lists all screens with info
    for (int i = 0; i < qList.length(); i++) {
        qDebug() << "Screen " << i << " " << qList[i]->geometry();
    }

    // Get screen geometry of main window
    QRect screenGeometryMainWindow = qList[0]->geometry();

    if (qList.length() >= 2) {
        //Both windows are made fullscreen && w1 => primarydisplay && w2 => secondary display
        w1.setWindowFlags(Qt::FramelessWindowHint);
        w1.showFullScreen();

        // update mainwindow variables to keep track of own dimensions
        w1.windowHeight = screenGeometryMainWindow.height();
        w1.windowWidth = screenGeometryMainWindow.width();

        w2.move(qList[1]->geometry().x(), 0);
        // If we want the window to always stay on top we can use Qt::WindowStaysOnTopHint
        w2.setWindowFlags(Qt::FramelessWindowHint);
        w2.showFullScreen();

        // Should we add the posibility of secondary window keeping track of own variables?
    } else {
        //There is only one screen - put the windows side by side on the same screen
        int height = screenGeometryMainWindow.height() / 2;
        int width = screenGeometryMainWindow.width() / 2;

        // update mainwindow variables to keep track of own dimensions
        w1.windowHeight = height;
        w1.windowWidth = width;

        w1.resize(width, height);
        w2.resize(width, height);

        //Moves the windows so they are centered and positioned correctly
        w1.move(0,0);
        w2.move(width,0);
    }

    // run mainwindow's setupUI function that sets up the rest of ui that relies on screen dimensions
    w1.setupUI();

    w1.show();
    w2.show();

    // qDebug() << qList.length();
    // qDebug() << "Width: " << width;

    //
    // starting tcp communication init
    //

    TcpRov *tcpRov = new TcpRov();
    w1.tcpRov = tcpRov;


    // connecting button
    QObject::connect(&w2, SIGNAL(connectToROV()), tcpRov, SLOT(tcpConnect()));

    // connect rov values with ui and tcpRov
    QObject::connect(tcpRov, SIGNAL(updateROVValues(double, double, double, double, double, double)), &w2, SLOT(updateROVValues(double, double, double, double, double, double)) );

    // connect "set auto H & W" gui elements to tcp rov
    QObject::connect(&w2, qOverload<double>(&SecondaryWindow::updateAutoHeading), tcpRov, &TcpRov::setAutoHeading);

    HeadingWidget * hw = w1.headingWidget;
    // conect rov values to headingWidget
    QObject::connect(tcpRov, qOverload<double>(&TcpRov::updateYaw), hw, &HeadingWidget::updateYaw);
    QObject::connect(&w1, qOverload<double>(&MainWindow::updateYawReference), hw, &HeadingWidget::updateYawReference);
    QObject::connect(tcpRov, qOverload<double>(&TcpRov::updateAutoHeading), hw, &HeadingWidget::updateAutoHeading);


    DepthWidget * dw = w1.depthWidget;
    QObject::connect(tcpRov, qOverload<double>(&TcpRov::updateDepth), dw, &DepthWidget::updateDepth);
    QObject::connect(tcpRov, qOverload<double>(&TcpRov::updateAutoDepth), dw, &DepthWidget::updateAutoDepth);
    QObject::connect(&w1, qOverload<double>(&MainWindow::updateDepthReference), dw, &DepthWidget::updateDepthReference);

    BiasWidget * bw = w1.biasWidget;
    QObject::connect(tcpRov, qOverload<double, double, double>(&TcpRov::updateBias), bw, &BiasWidget::updateBias);

    //
    // End tcp init
    //

    return a.exec();
}
