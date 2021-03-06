//
// Configuration
//

// Includes
#include "pedestriandetection.h"


//
// Construction and destruction
//

PedestrianDetection::PedestrianDetection(cv::Mat const* iFrame) : Component(iFrame)
{
    adjustedX = 0;
    tracksWidth = -1;
}


//
// Component interface
//

void PedestrianDetection::preprocess()
{
    mFrameDebug = (*frame()).clone();

    enhanceFrame();
}

void PedestrianDetection::find_features(FrameFeatures& iFrameFeatures) throw(FeatureException)
{
    //Detecting current tracks width
    if (iFrameFeatures.tracks.first.size() > 1 && iFrameFeatures.tracks.second.size() > 1) {
        int x1 = iFrameFeatures.tracks.first[0].x;
        int y1 = iFrameFeatures.tracks.first[0].y;
        int x2 = iFrameFeatures.tracks.second[0].x;
        int y2 = iFrameFeatures.tracks.second[0].y;

        tracksWidth = sqrt(pow(x2-x1, 2) + pow(y2-y1, 2));
        tracksStartCol = x1;
        tracksEndCol = x2;
    }
    //Crop the frame = faster detection
    cropFrame();
    //Detect pedestrians
    detectPedestrians(iFrameFeatures);
}

cv::Mat PedestrianDetection::frameDebug() const
{
    return mFrameDebug;
}


//
// Feature detection
//

void PedestrianDetection::cropFrame()
{
    //Only interested in the area next to the tracks
    cv::Range rowRange(0, frame()->rows);
    cv::Range colRange;
    if (tracksWidth > -1) {
        adjustedX = tracksStartCol - 2*tracksWidth;
        if (adjustedX < 0) {
            adjustedX = 0;
        }
        colRange = cv::Range(adjustedX, (tracksEndCol + 2*tracksWidth > frame()->cols?frame()->cols:tracksEndCol + 2*tracksWidth));
    } else {
        colRange = cv::Range(0, frame()->cols);
    }
    cv::Mat blockFromFrame(*frame(), rowRange, colRange);

    //Scaling it down to 190x[x]
    scale = blockFromFrame.rows / 190;
    mFrameCropped = cv::Mat(blockFromFrame.rows / scale, blockFromFrame.cols / scale, CV_8UC3);
    cv::resize(blockFromFrame, mFrameCropped, mFrameCropped.size(), 0, 0, cv::INTER_LINEAR);
}

void PedestrianDetection::enhanceFrame()
{
    //    int brightness = 0;
    //        int contrast = 0;
    //        double a, b;
    //        if( contrast > 0 )
    //        {
    //            double delta = 127.*contrast/100;
    //            a = 255./(255. - delta*2);
    //            b = a*(brightness - delta);
    //        }
    //        else
    //        {
    //            double delta = -128.*contrast/100;
    //            a = (256.-delta*2)/255.;
    //            b = a*brightness + delta;
    //        }
    //        Mat dst;
    //        frame.convertTo(dst, CV_8U, a, b);
    //        GaussianBlur(dst, dst, Size(5, 5), 1.2, 1.2);
}

void PedestrianDetection::detectPedestrians(FrameFeatures& iFrameFeatures)
{
    bool added = false;
    std::vector<cv::Rect> found_filtered;
    std::vector<cv::Rect> found;
    //Loading cascade xml
    if (!cascade.load("./res/haarcascade_fullbody.xml"))
        throw FeatureException("Could not load cascade definition file");
    cascade.detectMultiScale(mFrameCropped, found);

    size_t j;
    //Find the rect's found for the people
    for(size_t i = 0; i < found.size(); i++ )
    {
        cv::Rect r = found[i];
        for( j = 0; j < found.size(); j++ )
            if( j != i && (r & found[j]) == r)
                break;
        if( j == found.size() )
            found_filtered.push_back(r);
    }
    //Add them to the found features
    for(size_t i = 0; i < found_filtered.size(); i++ )
    {
        cv::Rect r = found_filtered[i];
        r.x *= scale;
        r.x += adjustedX;
        r.y *= scale;
        r.width *= scale;
        r.height *= scale;

        if (!added) {
            iFrameFeatures.pedestrians.clear();
        }

        iFrameFeatures.pedestrians.push_back(r);

        added = true;

        cv::rectangle(mFrameDebug, r.tl(), r.br(), cv::Scalar(0,0,255), 2);
    }
    //If no features are found: exception
    if (!added) {
        throw FeatureException("no pedestrians found");
    }
}
