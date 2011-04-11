///////////////////////////////////////////////////////////////////////////////
// Configuration
//

// Headers
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include <iostream>
#include "framefeatures.h"
#include "component.h"
#include "trackdetection.h"

// Definitions
#define FEATURE_EXPIRATION 5


///////////////////////////////////////////////////////////////////////////////
// Main
//

int main(int argc, char** argv)
{
    //
    // Setup application
    //

    // Read command-line parameters
    if (argc < 2)
    {
        std::cout << "Error: invalid usage" << "\n";
        std::cout << "Usage: ./wp1 <input_filename> <output_filename>" << std::endl;
        return 1;
    }

    // Open input video
    std::string iVideoFile = argv[1];
    cv::VideoCapture iVideo(iVideoFile);
    if(!iVideo.isOpened())
    {
        std::cout << "Error: could not open video" << std::endl;
        return 1;
    }

    // Open output video
    cv::VideoWriter oVideo;
    if (argc == 3)
    {
        std::string oVideoFile = argv[2];
        oVideo = cv::VideoWriter(oVideoFile,
                                 CV_FOURCC('M', 'J', 'P', 'G'),
                                 iVideo.get(CV_CAP_PROP_FPS),
                                 cv::Size(iVideo.get(CV_CAP_PROP_FRAME_WIDTH),iVideo.get(CV_CAP_PROP_FRAME_HEIGHT)),
                                 true);
    }

    // Setup OpenCV
    cv::namedWindow("tram collision avoidance", 1);


    //
    // Process video
    //

    cv::Mat tFrame;
    unsigned int tFrameCount = 0;
    FrameFeatures tOldFeatures;
    unsigned int tAgeTrack = 0;
    while (iVideo.grab() && iVideo.retrieve(tFrame))
    {
        // Initialize the frame
        std::cout << "-- PROCESSING FRAME " << tFrameCount++ << " --" << std::endl;
        imshow("tram collision avoidance", tFrame);

        // Set-up struct with features
        FrameFeatures tFeatures;
        cv::Mat tFeaturesVisualized = tFrame.clone();

        // Load objects
        TrackDetection tTrackDetection(tFrame);

        // Preprocess
        std::cout << "* Preprocessing" << std::endl;
        tTrackDetection.preprocess();
        tTrackDetection.setFrameDebug(tTrackDetection.framePreprocessed());

        // Find features
        std::cout << "* Finding features" << std::endl;
        try
        {
            // Find tracks
            std::cout << "- Finding tracks" << std::endl;
            try
            {
                tTrackDetection.find_features(tFeatures);
                tTrackDetection.copy_features(tFeatures, tOldFeatures);
                tAgeTrack = FEATURE_EXPIRATION;
            }
            catch (FeatureException e)
            {
                std::cout << "  Warning: " << e.what() << std::endl;
                if (tAgeTrack > 0)
                {
                    tAgeTrack--;
                    tTrackDetection.copy_features(tOldFeatures, tFeatures);
                }
                else
                    throw FeatureException("could not find the tracks");
            }
            for (size_t i = 0; i < tFeatures.track_left.size()-1; i++)
                cv::line(tFeaturesVisualized, tFeatures.track_left[i], tFeatures.track_left[i+1], cv::Scalar(0, 255, 0), 3);
            for (size_t i = 0; i < tFeatures.track_right.size()-1; i++)
                cv::line(tFeaturesVisualized, tFeatures.track_right[i], tFeatures.track_right[i+1], cv::Scalar(0, 255, 0), 3);
            imshow("tram collision avoidance", tFeaturesVisualized);
        }
        catch (FeatureException e)
        {
            std::cout << "! Error: " << e.what() << std::endl;
        }

        // Halt on keypress
        if (cv::waitKey(30) >= 0)
            break;
        std::cout << std::endl;
    }

    return 0;
}
