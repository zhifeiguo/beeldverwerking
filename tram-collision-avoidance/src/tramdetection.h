#ifndef TRAMDETECTION_H
#define TRAMDETECTION_H

// Includes
#include "opencv/cv.h"
#include "highgui.h"
#include <cmath>
#include <vector>
#include "component.h"
#include "framefeatures.h"

class TramDetection : public Component
{
public:
    TramDetection();
    // Construction and destruction
    TramDetection(const cv::Mat& iFrame);

    // Component interface
    void preprocess();
    void find_features(FrameFeatures& iFrameFeatures) throw(FeatureException);
    cv::Mat frameDebug() const;

private:
    // Frames
    cv::Mat mFramePreprocessed;
    cv::Mat mFrameDebug;
};

#endif // TRAMDETECTION_H
