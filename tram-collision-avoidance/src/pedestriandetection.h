#ifndef PEDESTRIANDETECTION_H
#define PEDESTRIANDETECTION_H
// Includes
//#include "opencv/cv.h"
//#include "opencv2/core/core.hpp"
//#include "opencv2/imgproc/imgproc.hpp"
//#include "opencv2/objdetect/objdetect.hpp"
//#include "opencv2/highgui/highgui.hpp"

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <iostream>
#include <vector>
#include "component.h"
#include "framefeatures.h"

class PedestrianDetection : public Component
{
public:
    // Construction and destruction
    PedestrianDetection(cv::Mat const* iFrame);

    // Component interface
    void preprocess();
    void find_features(FrameFeatures& iFrameFeatures) throw(FeatureException);
    cv::Mat frameDebug() const;

private:
    // Feature detection
    cv::CascadeClassifier cascade;
    int scale;
    int tracksWidth, tracksStartCol, tracksEndCol;
    int adjustedX;

    void cropFrame();
    void enhanceFrame();
    void detectPedestrians(FrameFeatures& iFrameFeatures);

    cv::Mat mFrameCropped;

    // Frames
    cv::Mat mFramePreprocessed;
    cv::Mat mFrameDebug;
};
#endif // PEDESTRIANDETECTION_H
