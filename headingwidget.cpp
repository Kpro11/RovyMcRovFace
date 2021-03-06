#include "headingwidget.h"
#include <QDebug>
#include "QHBoxLayout"
#include <QLabel>
#include <math.h>
#include "converter.h"
#include "fontsize.h"

HeadingWidget::HeadingWidget(QWidget *parent) : QWidget(parent)
{

}

void HeadingWidget::setupStyleSheets() {

    whiteText = " QLabel { color: white;  } ";   // stylesheet for white text

    int bigFont = FontSize::getBigFont(windowWidth);
    int smallFont = FontSize::getSmallFont(windowWidth);

    charStyle = " QLabel { color: white; font-size: " + QString::number(bigFont) + "px; } ";
    numStyle = " QLabel { color: white; font-size: " + QString::number(smallFont) + "px; } ";
    yawStyleSheet = "QLabel { color: white; font-size: " + QString::number(bigFont) + "px; }";

    borderStyleSheet = "QFrame#headingWidget { border: ";
    borderStyleSheet += QString::number(FontSize::getBorder(windowWidth));
    borderStyleSheet += "px solid white; border-top: 0; border-left: 0; border-right: 0; }";
}

void HeadingWidget::setupUI(QWidget * _videoPlayer, int * _windowWidth, int * _windowHeight) {
    videoPlayer = _videoPlayer;
    windowWidth = _windowWidth;
    windowHeight = _windowHeight;

    setupStyleSheets();

    int frameStartX = * windowWidth / 12;
    int frameStartY = * windowHeight / 16;
    frameWidth = *windowWidth - frameStartX * 2;
    frameHeight = 35;

    pixelsPerSlot = frameWidth / 12;

    // create frame that acts as a container for this heading widget;
    frame = new QFrame( videoPlayer );
    frame->setObjectName("headingWidget");

    // set the position and dimensions (start x, start y, width, heigth
    frame->setGeometry(frameStartX, frameStartY, frameWidth, frameHeight);
    // add a border to the bottom
    frame->setStyleSheet( borderStyleSheet );

    // Add a label to show current heading / yaw
    currentYaw = new QLabel( QString::number(yaw) , videoPlayer );
    currentYaw->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    currentYaw->setStyleSheet(yawStyleSheet);
    currentYawPos = new Position();

    currentYawPos->width = 200;
    currentYawPos->height = frameHeight;
    currentYawPos->x = frameStartX + (frameWidth / 2) - currentYawPos->width / 2; //position to horizontally center the widget
    currentYawPos->y = frameStartY + frameHeight + frameHeight / 1.5;  // position to vertically center the widget
    setPosition(currentYaw, currentYawPos);

    yawReference = new QLabel( "0", videoPlayer );
    yawReference->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    yawReference->setStyleSheet(yawStyleSheet);
    yawReferencePos = new Position();

    yawReferencePos->width = currentYawPos->width;
    yawReferencePos->height = currentYawPos->height;
    yawReferencePos->x = currentYawPos->x;
    yawReferencePos->y = currentYawPos->y + currentYawPos->height * 1.5;
    setPosition(yawReference, yawReferencePos);

    yawReferenceLock = new QLabel("0", videoPlayer );
    yawReferenceLock->setStyleSheet(yawStyleSheet);
    yawReferenceLock->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    yawReferenceLockPos = new Position();

    yawReferenceLockPos->x= yawReferencePos->x - yawReferencePos->width / 1.4;
    yawReferenceLockPos->y = yawReferencePos->y;
    yawReferenceLockPos->width = yawReferencePos->width;
    yawReferenceLockPos->height = yawReferencePos->height;
    setPosition(yawReferenceLock, yawReferenceLockPos);

    QPixmap pixmapTarget = QPixmap(":/images/lock-icon.png");
    pixmapTarget = pixmapTarget.scaled(yawReferencePos->height / 2.2, yawReferencePos->height / 2.2);
    yawReferenceLock->setPixmap(pixmapTarget);

    // Hide the reference labels untill we want them
    yawReference->hide();
    yawReferenceLock->hide();



    // Create all the labels we need with appropiate styling
    for (int i = 0; i < 360; i += 15) {
        QString stylesheet = "";

        HeadingLabel *newLabel = new HeadingLabel();
        newLabel->value = i;
        switch (i) {
            case 0:
                newLabel->letter = 'N';
                stylesheet = charStyle;
                break;
            case 90:
                newLabel->letter = 'E';
                stylesheet = charStyle;
                break;
            case 180:
                newLabel->letter = 'S';
                stylesheet = charStyle;
                break;
            case 270:
                newLabel->letter = 'W';
                stylesheet = charStyle;
                break;
            default:
                newLabel->letter = ' ';
                stylesheet = numStyle;
        }

        if (newLabel->letter != ' ') {
            newLabel->label = new QLabel(newLabel->letter, frame);
        } else {
            newLabel->label = new QLabel(QString::number(newLabel->value), frame);
        }

        newLabel->label->setStyleSheet(stylesheet);

        // Hide the label for now
        newLabel->label->setGeometry(0,0,0,0);

        // add label to list of all labels
        labels << newLabel;
    }

    updateLabels();

    //testTimer = new QTimer(this);
    //connect(testTimer, SIGNAL(timeout()), this, SLOT(testUpdate()));
    //testTimer->start(30);
}

void HeadingWidget::updateLockPosition() {
    QString yRef = QString::number(yawRef);
}

void HeadingWidget::setPosition(QLabel * _lbl, Position * _lblPos) {
    _lbl->setGeometry(_lblPos->x, _lblPos->y, _lblPos->width, _lblPos->height);
}

//This function will update the current heading / yaw view by setting new positions on all labels
void HeadingWidget::updateLabels() {
    // asumes that yaw is in degrees
    // iterate trough every label
    for (int i = 0; i < labels.size(); i++) {

        int point = labels[i]->value;

        // finds the distance in this 360 degree circle
        double distance = distanceFromPointToYaw(point, yaw);
        // label should only show if it is in range
        if (abs(distance) <= 90) {
            // normalize
            double positionInRow = (distance + 90) / 15;

            // set position
            labels[i]->label->setGeometry(positionInRow * pixelsPerSlot, 0, 30, 30);
        } else {
            // hide the label
            labels[i]->label->setGeometry(0,0,0,0);
        }
    }
}

// calculates true distance between two points in a 360 degrees circle
double distanceFromPointToYaw(double point, double _yaw) {

    // To find the distance between two points in a "circle" you have to do more than just
    // calculate the difference. You also have to take into account that the values 0 and 360 are neighbors.
    // We therefor calculate three values here: Normal distance, distance counter-clockwise and
    // distance clockwise. The lowest of these variables are the true distance.

    // in this code we calculate (counter-)clockwise withouth checking if yaw was reached,
    // this is to keep the math simple

    // For example: A yaw at 330 and a point at 20 are only 50 degrees away from eachother
    // normal distance = 310
    // counter-clockwise distance = 360+320 = 680
    // clockwise distance = 50

    _yaw = static_cast<int>(_yaw) % 360;

    // the normal distance between yaw and point
    double distanceBetweenYawAndPoint = abs(_yaw - point);

    // calculate variables to help traverse (counter-)clockwise
    double yawDistanceToZero = _yaw;
    double yawDistanceToMax = 360 - _yaw;

    double pointDistanceToZero = point;
    double pointDistanceToMax = 360 - point;

    // distance from point to max (360) + from min (0) to yaw point
    double distanceAbove = pointDistanceToMax + yawDistanceToZero;
    // distance from point to min(0) + from max(360) to yaw point.
    double distanceBelow = pointDistanceToZero + yawDistanceToMax;

    // we now know what the true distance is.

    // find the lowest value and return
    if(distanceAbove < distanceBelow && distanceAbove < distanceBetweenYawAndPoint) {
        // since the value was above we return negative
        return -1 * distanceAbove;
    } else if (distanceBelow < distanceAbove && distanceBelow < distanceBetweenYawAndPoint) {
        return distanceBelow;
    } else { // distanceBetweenYawAndPoint was lowest
        // we calculate it again because it will produce the rigth "fortegn"
        return point - _yaw;
    }

    // The code belov returns the true distance
    // Choose the value that was lowest. This is the correct value
    // return std::min(std::min(distanceAbove, distanceBelow), distanceBetween_yawAndPoint);
}

void HeadingWidget::testUpdate() {
    /*
    yaw = abs(360 * sin(testTime * 3.141 / ( 360 * 4)) + 1);
    testTime += 1;
    updateLabels();
    */
}

//

/// @notice This function is called when we get a new yaw value.
/// @dev Wraps yaw, updates currentYaw and updates labels
/// @param _yaw The current yaw in degrees. Example values [-823.33333, -359.3, -2, 0, 40, 360, 720]
void HeadingWidget::updateYaw(double _yaw) {


    // set the new global yaw
    yaw = wrapYaw(_yaw);

    // updates headingWidget
    currentYaw->setText(formatYaw(yaw));
    updateLabels();
}


/// @notice Wraps the yaw around the 360 degree axis
/// @param _yaw The current yaw in degrees. Example values [-823.33333, -359.3, -2, 0, 40, 360, 720]
double HeadingWidget::wrapYaw(double _yaw) {
    // We need to wrap the yaw around the 360 degree axis and preserve decimals.

    /* We do this by
     * 1. round yaw to floor. Ex: 2.2 => 2, -4.3 => -4
     * 2. Wrap yaw to [0,359]
     * 3. Extract the decimals of the original yaw
     * 4. Add decimals to wrapped yaw. (If original yaw is negative we also have to invert the decimals around [1,0]
     */

    // Floor the yaw and cast to int.
    int roundedYaw = static_cast<int>(floor(_yaw));

    // because the c++ % operation is wierd and diffrent from python (a % b) we have to use this formula to immitate python %: (b + (a%b)) % b
    // Wraps the yaw around on 360 degrees
    // Example: -30 => 330, 370 => 10, -370 => 350
    int wrappedYaw = (360 + roundedYaw % 360) % 360;

    // Now we have to add the decimals back.
    double newYaw = wrappedYaw;
    if (_yaw >= 0) {
       // ex: 2.23 - 2 = 0.23
       newYaw += _yaw - floor(_yaw);
    } else {
       // here we have to find decimals and invert them since we get a 0 -> -360 scale and not -360 -> 0
       // ex: 1 - abs(-20.3-(-20)) = 1 - 0.3 = 0.7
       newYaw += 1 - abs(_yaw - ceil(_yaw));
    }

    return newYaw;
}

// This function will output a string with exactly one decimal
QString HeadingWidget::formatYaw(double _yaw) {
    QString output = "";
    // round of decimal to 1 place
    double rounded = ceil(_yaw * 10) / 10;

    output = QString::number(rounded);

    // add decimal if not there
    if (floor(rounded) == rounded) {
        output += ".0";
    }

    // If it shows 360, show 0 instead
    if (output == "360.0") {
        output = "0.0";
    }

    return output;
}

void HeadingWidget::updateYawReference(double _yaw) {
    double yawDeg = Converter::radToDeg(_yaw);
    double wrappedYaw = wrapYaw(yawDeg);
    yawReference->setText(formatYaw(wrappedYaw));
}

void HeadingWidget::updateAutoHeading(double _autoHeading) {
    if (_autoHeading <= 0) {
        yawReference->hide();
        yawReferenceLock->hide();
    } else {
        yawReference->show();
        yawReferenceLock->show();
    }
}
