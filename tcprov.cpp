#include "tcprov.h"
#include <stdlib.h>
#include "secondarywindow.h"
#include <QTextCodec>
#include <QTimer>
#include <QDebug>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include "converter.h"

#pragma comment(lib, "Ws2_32.lib")

// This class / function connects, reads and writes to the FhSim simulator or sintef backend using WinSockets
// This class also keeps track of the next variables to send.
// We initially implemnted this using QTcpSocket, but the communication with the simulator was not well defined
// so it was difficult to figure out the message format/encoding. We therefor based this code upon the supplied
// reference implementation recived from sintef.

TcpRov::TcpRov(QObject *parent) : QObject(parent)
{
    // Create and connect tcp timers
    tcpReadTimer = new QTimer(this);
    QObject::connect(tcpReadTimer, SIGNAL(timeout()), this, SLOT(tcpRead()));
}

// This function reads the data from the socket.
void TcpRov::tcpRead() {

    int iResult;

    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = 12 * recNum;

    iResult = recv(ConnectSocket, recvbuf, recvbuflen, 0);

    if (iResult > 0)
        qDebug() << "Bytes received: " << iResult;
    else if (iResult == 0) {
        stopTcpReadTimer();
        qDebug() << "Connection closed";
        return;
    } else {
        qDebug() << "recv failed with error: " << WSAGetLastError();
        stopTcpReadTimer();
        return;
    }

    memcpy(&readData.north, &recvbuf[0], 8);
    memcpy(&readData.east, &recvbuf[0 + 8 * 1], 8);
    memcpy(&readData.down, &recvbuf[0 + 8 * 2], 8);
    memcpy(&readData.roll, &recvbuf[0 + 8 * 3], 8);
    memcpy(&readData.pitch, &recvbuf[0 + 8 * 4], 8);
    memcpy(&readData.yaw, &recvbuf[0 + 8 * 5], 8);

    // make sure that the down is above zero
    if (readData.down < 0)
        readData.down = 0;

    // convert yaw from rad to degrees
    readData.yaw = Converter::radToDeg(readData.yaw);

    qDebug() << "Sending this data:\t" <<  "North:\t" << readData.north << " m\t" << "East:\t" << readData.east << " m\t" << "Down:\t" << readData.down << " m" << "Roll:\t" << readData.roll << " rad\t" << "Pitch:\t" << readData.pitch << " rad\t" << "yaw:\t" << readData.yaw << " deg";

    // send data to ui
    emit updateROVValues(readData.north, readData.east, readData.down, readData.roll, readData.pitch, readData.yaw);
    emit updateYaw(readData.yaw);
    emit updateDepth(readData.down);
    emit updateBias(biasSurge, biasSway, biasHeave);
    if (nextData.autoHeading >= 1) {
        emit updateReferenceHeading(nextData.yaw);
    }
    if (nextData.autoDepth >= 1) {
        emit updateReferenceDepth(nextData.heave);
    }
    // send next data
    tcpSend();
}

// this function connects to FhSim/backend and start the read-write loop
void TcpRov::tcpConnect() {
    //
    // The following code was mostly supplied by sintef, but modified sligthly on our part.
    //

    // Initialize Winsock
    int iResult; //variable to check for errors
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) {
        qDebug() << "WSAStartup failed: " << iResult;
        return;
    }

    struct addrinfo *result = NULL,
            *ptr = NULL,
            hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    // Resolve the server address and port
    iResult = getaddrinfo(IPString.c_str(), PortString.c_str(), &hints, &result);
    if (iResult != 0) {
        qDebug() << "getaddrinfo failed: " << iResult;
        WSACleanup();
        return;
    }

    // Attempt to connect to the first address returned by the call to getaddrinfo
    ptr = result;

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);

    if (ConnectSocket == INVALID_SOCKET) {
        qDebug() << "Error at socket(): " << WSAGetLastError();
        freeaddrinfo(result);
        WSACleanup();
        return;
    }

    // call to function outside of qt scope
    winsockConnect(&ConnectSocket, ptr);

    if (ConnectSocket == INVALID_SOCKET) {
        qDebug() << "Unable to connect to server!";
        WSACleanup();
        return;
    }

    // Should really try the next address returned by getaddrinfo if the connect call failed
    // But for this simple example we just free the resources returned by getaddrinfo and print an error message
    freeaddrinfo(result);

    startTcpReadTimer();

    qDebug() << "finnished connecting";
}

// this function send data to the socket
void TcpRov::tcpSend() {


    // check how long the connection to the simulator has run for.
    runTime = (iRead + 1) * timeStep;
    //qDebug () << "Sending time: " << runTime;

    // increment the counter of how many times we have read.
    iRead++;

    // The reference variables that sintef provided:
    // protip: scroll out in the simulator to see the ship

    /*
    nextData.ForceSurge = -30.0 + 0.5 * sin(runTime*3.14159265/6.0);
    nextData.ForceSway = 0;
    nextData.ForceHeave = 0.2 * runTime;
    nextData.ForceYaw = 0.1 * sin(runTime*3.14159265/6.0);
    */

    qDebug() << "Sending this data to tcp " << nextData.surge << nextData.sway << nextData.heave << nextData.roll << nextData.pitch << nextData.yaw << nextData.autoDepth << nextData.autoHeading;

    msg_buf.clear();

    // Need to reserve space
    // msg_buf.reserve(sizeof(runTime) + sizeof(nextData));
    msg_buf.reserve(sizeof(nextData));

    // the simulator expects the first number to be the time it has run, but in their referance implementation they do not send this number
    // adding runTime just changes the rotation of the rov, so i guess that code is not in the current simulator but maybe in the next
    // if that is the case, uncomment the next line and the msg_buf line
    // msg_buf.append((const char*)&runTime, sizeof(runTime));
    msg_buf.append((const char*)&nextData.surge, sizeof(nextData.surge));
    msg_buf.append((const char*)&nextData.sway, sizeof(nextData.sway));
    msg_buf.append((const char*)&nextData.heave, sizeof(nextData.heave));
    msg_buf.append((const char*)&nextData.roll, sizeof(nextData.roll));
    msg_buf.append((const char*)&nextData.pitch, sizeof(nextData.pitch));
    msg_buf.append((const char*)&nextData.yaw, sizeof(nextData.yaw));
    msg_buf.append((const char*)&nextData.autoDepth, sizeof(nextData.autoDepth));
    msg_buf.append((const char*)&nextData.autoHeading, sizeof(nextData.autoHeading));

    int iResult = send(ConnectSocket, &msg_buf[0], msg_buf.size(), 0);
    qDebug() << "Buffer size: " << msg_buf.size();
    std::cout << msg_buf;
    if (iResult == SOCKET_ERROR)
    {
        qDebug() << "Send failed with WSA error: " << WSAGetLastError();
        stopTcpReadTimer();
        return;
    }
    qDebug() << "Bytes sent: " << iResult;

    //reset nextData values
    resetValuesButNotFlagValues();
}

void TcpRov::startTcpReadTimer() {

    // qDebug() << "Starting the tcpReadTimer";
    // start to launch the read functions every timeStep
    tcpReadTimer->start(timeStep * 1000);
}

void TcpRov::stopTcpReadTimer() {
    // qDebug() << "Stopping the tcpReadTimer";
    tcpReadTimer->stop();
}

/// @brief sets all the values that is going into the next tcp send
void TcpRov::setValues(double _north, double _east, double _down, double _roll, double _pitch, double _yaw, double _autoDepth, double _autoHeading) {
    nextData.surge = _north;
    nextData.sway = _east;
    // the following code will ensure that reference depth is never set below 0 (above water)
    if (_autoDepth > 0)
        if (_down < 0)
            nextData.heave = 0;
        else
            nextData.heave = _down;
    else
        nextData.heave = _down;
    nextData.roll = _roll;
    nextData.pitch = _pitch;
    nextData.yaw = _yaw;
    nextData.autoDepth = _autoDepth;
    nextData.autoHeading = _autoHeading;
}

/// sets the auto heading flag to 0 or 1
void TcpRov::setAutoHeading(double _autoHeading) {
    autoHeading = _autoHeading;
    nextData.autoHeading = _autoHeading;
    emit updateAutoHeading(_autoHeading);
}

/// sets the auto depth flag to 0 or 1
void TcpRov::setAutoDepth(double _autoDepth) {
    autoDepth = _autoDepth;
    nextData.autoDepth = _autoDepth;
    emit updateAutoDepth(_autoDepth);
}

/// The same as the functions above but it does not recive or set a value. Should only be called if autoHeading is NOT set by functions above
void TcpRov::autoHeadingWasUpdated() {
    nextData.autoHeading = autoHeading;
    emit updateAutoHeading(autoHeading);
}

/// The same as the functions above but it does not recive or set a value. Should only be called if autoDepth is NOT set by functions above
void TcpRov::autoDepthWasUpdated() {
    nextData.autoDepth = autoDepth;
    emit updateAutoDepth(autoDepth);
}

// this function resets all the nextData variables to zero. This is done such that we don't double send data.
void TcpRov::resetAllValues() {
    nextData.surge = 0;
    nextData.sway = 0;
    nextData.heave = 0;
    nextData.roll = 0;
    nextData.pitch = 0;
    nextData.yaw = 0;
    /*
    nextData.autoDepth = 0;
    nextData.autoHeading = 0;
    */
}

// This function reset all values execpt flags and if a flag is 1 then it will not reset the corresponding value
void TcpRov::resetValuesButNotFlagValues() {
    /* Uncomment next version of simulator
    if (nextData.autoDepth == 0) {
        nextData.heave = 0;
    }
    if (nextData.autoHeading == 0) {
        nextData.yaw = 0;
    }
    */

    // Delete this when uncommenting the lines above
    nextData.heave = 0;
    nextData.yaw = 0;

    // Do not delete this
    nextData.surge = 0;
    nextData.sway = 0;
    nextData.roll = 0;
    nextData.pitch = 0;
}


// this function is an extension of TcpRov::tcpConnect and was made because "connect" is a reserved word for QT. But this function does not inherit from QT.
void winsockConnect(SOCKET *_ConnectSocket, addrinfo *ptr) {
    // Connect to server.
    int iResult = connect(*_ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(*_ConnectSocket);
        *_ConnectSocket = INVALID_SOCKET;
    }
}

/// @brief sets the reference heading. only used when auto heading is set
/// @param _ref double reference heading [0, 2 * PI) radians
void TcpRov::setReferenceHeading(double _ref) {
    referenceHeading = _ref;
    nextData.yaw = _ref;
}

/// @brief sets the reference depth. only used when auto depth is set
/// @param _ref double reference depth [0, 200] meters
void TcpRov::setReferenceDepth(double _ref) {
    referenceDepth = _ref;
    nextData.heave = _ref;
}


