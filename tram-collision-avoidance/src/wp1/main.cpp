///////////////////////////////////////////////////////////////////////////////
// Configuration
//

// Headers
#include "opencv/cv.h"
#include "opencv/highgui.h"
#include <cmath>
#include <iostream>
#include <vector>

// Feature properties
#define TRACK_WIDTH 15
#define TRACK_COUNT 2
#define TRACK_START_OFFSET 10
#define TRACK_SPACE_MIN 100
#define TRACK_SPACE_MAX 175
#define SEGMENT_LENGTH_MIN 25
#define SEGMENT_LENGTH_DELTA 10
#define SEGMENT_ANGLE_MIN -M_PI_4
#define SEGMENT_ANGLE_MAX M_PI_4
#define SEGMENT_ANGLE_DELTA M_PI_4/64.0
#define SEGMENT_ANGLE_DELTA_MAX M_PI/6.0


///////////////////////////////////////////////////////////////////////////////
// Routines
//

//
// Auxiliary
//

int twz(cv::Point a, cv::Point b, cv::Point c)
{
    int dxb = b.x - a.x, dyb = b.y - a.y,
            dxc = c.x - a.x, dyc = c.y - a.y;
    if (dxb * dyc > dyb * dxc)
        return 1;
    else if (dxb * dyc < dyb * dxc)
        return -1;
    else if (dxb * dxc < 0 || dyb * dyc < 0)
        return -1;
    else if (dxb * dxb + dyb * dyb >= dxc * dxc + dyc * dyc)
        return 0;
    else
        return 1;
}

// Test of two segments intersect
bool intersect(const cv::Point &p1, const cv::Point &p2,
               const cv::Point &p3, const cv::Point &p4)
{
    return     (twz(p1, p2, p3) * twz(p1, p2, p4) <= 0)
            && (twz(p3, p4, p1) * twz(p3, p4, p2) <= 0);
}

// Fetch the intersection point of two colliding segments (this has to
// be tested beforehand)
cv::Point intersect_point(const cv::Point &p1, const cv::Point &p2,
                          const cv::Point &p3, const cv::Point &p4)
{
    int d = (p1.x-p2.x)*(p3.y-p4.y) - (p1.y-p2.y)*(p3.x-p4.x);

    // HACK HACK
    if (d == 0)
        d++;

    int xi = ((p3.x-p4.x)*(p1.x*p2.y-p1.y*p2.x)
              -(p1.x-p2.x)*(p3.x*p4.y-p3.y*p4.x))
            /d;
    int yi = ((p3.y-p4.y)*(p1.x*p2.y-p1.y*p2.x)
              -(p1.y-p2.y)*(p3.x*p4.y-p3.y*p4.x))
            /d;

    return cv::Point(xi, yi);
}


//
// Preprocessing
//

cv::Mat preprocess(const cv::Mat& iFrame)
{
    // Convert to grayscale
    cv::Mat tFrameGray(iFrame.size(), CV_8U);
    cvtColor(iFrame, tFrameGray, CV_RGB2GRAY);

    // Sobel transform
    cv::Mat tFrameSobel(iFrame.size(), CV_16S);
    Sobel(tFrameGray, tFrameSobel, CV_16S, 3, 0, 9);

    // Convert to 32F
    cv::Mat tFrameSobelFloat(iFrame.size(), CV_8U);
    tFrameSobel.convertTo(tFrameSobelFloat, CV_32F, 1.0/256, 128);

    // Threshold
    cv::Mat tFrameThresholded = tFrameSobelFloat > 200;

    // Blank out useless region
    rectangle(tFrameThresholded, cv::Rect(0, 0, iFrame.size[1], iFrame.size[0] * 0.50), cv::Scalar::all(0), CV_FILLED);
    std::vector<cv::Point> tRectRight, tRectLeft;
    tRectRight.push_back(cv::Point(iFrame.size[1], iFrame.size[0]));
    tRectRight.push_back(cv::Point(iFrame.size[1]-iFrame.size[1]*0.25, iFrame.size[0]));
    tRectRight.push_back(cv::Point(iFrame.size[1], 0));
    fillConvexPoly(tFrameThresholded, &tRectRight[0], tRectRight.size(), cv::Scalar::all(0));
    tRectLeft.push_back(cv::Point(0, iFrame.size[0]));
    tRectLeft.push_back(cv::Point(0+iFrame.size[1]*0.25, iFrame.size[0]));
    tRectLeft.push_back(cv::Point(0, 0));
    fillConvexPoly(tFrameThresholded, &tRectLeft[0], tRectLeft.size(), cv::Scalar::all(0));

    return tFrameThresholded;
}


//
// Feature detection
//

// Find lines in a frams
std::vector<cv::Vec4i> find_lines(cv::Mat& iFrame,
                                  cv::Mat& iDebug)
{
    // Find the lines through Hough transform
    std::vector<cv::Vec4i> oLines;
    cv::HoughLinesP(iFrame, // Image
                oLines,             // Lines
                1,                  // Rho
                CV_PI/180,          // Theta
                20,                 // Threshold
                50,                 // Minimum line length
                3                   // Maximum line gap
                );

    // Draw lines
    for(size_t i = 0; i < oLines.size(); i++)
    {
        line(iDebug,
             cv::Point(oLines[i][0],
                       oLines[i][1]),
             cv::Point(oLines[i][2],
                       oLines[i][3]),
             cv::Scalar(0,0,255),
             3,
             8
             );
    }

    return oLines;
}

// Find the start candidates of a track
std::vector<cv::Point> find_track_start(const cv::Mat& iFrame,
                                        const std::vector<cv::Vec4i>& iLines,
                                        cv::Mat& iDebug)
{
    // Find track start candidates
    std::vector<int> track_points, track_intersections;
    int y = iFrame.size[0] - TRACK_START_OFFSET;
    for (int x = iFrame.size[1]-TRACK_WIDTH/2; x > TRACK_WIDTH/2; x--)
    {
        // Count the amount of segments intersecting with the current track start point
        int segments = 0;
        cv::Vec2i p1(x+TRACK_WIDTH/2, y);
        cv::Vec2i p2(x-TRACK_WIDTH/2, y);
        for(size_t i = 0; i < iLines.size(); i++)
        {
            cv::Vec2i p3(iLines[i][0], iLines[i][1]);
            cv::Vec2i p4(iLines[i][2], iLines[i][3]);

            if (intersect(p1, p2, p3, p4))
            {
                segments++;
            }
        }
        if (segments == 0)
            continue;

        // Check if we are updating an existing track
        bool tUpdateExisting = false;
        for (size_t i = 0; i < track_points.size(); i++)
        {
            if ((track_points[i] - x) < TRACK_WIDTH)
            {
                tUpdateExisting = true;
                if (track_intersections[i] < segments)
                {
                    track_intersections[i] = segments;
                    track_points[i] = x;
                }
                break;
            }
        }
        if (!tUpdateExisting)   // New track point!
        {
            if (track_points.size() < TRACK_COUNT)
            {
                track_points.push_back(x);
                track_intersections.push_back(segments);
            }
            else
            {
                // Look for the track point with the least intersecting segments
                int least = 0;
                for (size_t i = 1; i < track_points.size(); i++)
                {
                    if (track_intersections[i] < track_intersections[least])
                        least = i;
                }

                // Replace it
                track_intersections[least] = segments;
                track_points[least] = x;
            }
        }
    }

    // Draw and generate output array
    std::vector<cv::Point> oTrackCandidates;
    for (size_t i = 0; i < track_points.size(); i++)
    {
        if (track_intersections[i] % 2)                                          // Small hack to improve detection
            track_points[i] -= TRACK_WIDTH/(2 * (track_intersections[i] % 2));   // of the track center.
        int x = track_points[i];
        circle(iDebug, cv::Point(x, y), 5, cv::Scalar(0, 255, 255), -1);
        oTrackCandidates.push_back(cv::Point(x, y));
    }

    return oTrackCandidates;
}

// Find the segments of a track
std::vector<cv::Point> find_track(const cv::Point& iStart,
                                  const std::vector<cv::Vec4i>& iLines,
                                  cv::Mat& iDebug)
{
    // Create output vector
    std::vector<cv::Point> oTrack;
    oTrack.push_back(iStart);

    // Detect new segments
    bool tNewSegment = true;
    while (tNewSegment)
    {
        // Display segment
        cv::Point segment_last = oTrack.back();
        circle(iDebug, segment_last, 5, cv::Scalar(0, 255, 0), -1);
        tNewSegment = false;

        // Convenience variables for segment center
        int x = segment_last.x;
        int y = segment_last.y;

        // Scan
        double max_overlap = 0;
        int optimal_length;
        double optimal_angle;
        for (int length = SEGMENT_LENGTH_MIN; ; length += SEGMENT_LENGTH_DELTA)
        {
            bool has_improved = false;

            for (double angle = SEGMENT_ANGLE_MIN; angle < SEGMENT_ANGLE_MAX; angle += SEGMENT_ANGLE_DELTA)
            {
                // Track segment coordinates (not rotated)
                cv::Rect r(cv::Point(x-TRACK_WIDTH/2, y),
                           cv::Point(x+TRACK_WIDTH/2, y-length));
                cv::Point r1(x - TRACK_WIDTH/2, y);
                cv::Point r2(x + TRACK_WIDTH/2, y);
                cv::Point r3(x + TRACK_WIDTH/2, y - length);
                cv::Point r4(x - TRACK_WIDTH/2, y - length);

                // Process all lines
                double tOverlap = 0;
                for(size_t i = 0; i < iLines.size(); i++)
                {
                    // Line coordinates (inversly rotated)
                    cv::Point p1(iLines[i][0], iLines[i][1]);
                    cv::Point p1_rot(x + cos(-angle)*(p1.x-x) - sin(-angle)*(p1.y-y),
                                     y + sin(-angle)*(p1.x-x) + cos(-angle)*(p1.y-y));
                    cv::Point p2(iLines[i][2], iLines[i][3]);
                    cv::Point p2_rot(x + cos(-angle)*(p2.x-x) - sin(-angle)*(p2.y-y),
                                     y + sin(-angle)*(p2.x-x) + cos(-angle)*(p2.y-y));

                    // Full overlap
                    if (r.contains(p1_rot) && r.contains(p2_rot))
                        tOverlap += sqrt((p1.x-p2.x)*(p1.x-p2.x) + (p1.y-p2.y)*(p1.y-p2.y));

                    // Partial overlap with one point contained
                    else if (r.contains(p1_rot) || r.contains(p2_rot))
                    {
                        // First point (the one within the rectangle)
                        cv::Point p3;
                        if (r.contains(p1_rot))
                            p3 = p1_rot;
                        else
                            p3 = p2_rot;

                        // Second point (intersecting the rectangle)
                        cv::Point p4;
                        if (intersect(p1_rot, p2_rot, r1, r2))
                            p4 = intersect_point(p1_rot, p2_rot, r1, r2);
                        else if (intersect(p1_rot, p2_rot, r2, r3))
                            p4 = intersect_point(p1_rot, p2_rot, r2, r3);
                        else if (intersect(p1_rot, p2_rot, r3, r4))
                            p4 = intersect_point(p1_rot, p2_rot, r3, r4);
                        else if (intersect(p1_rot, p2_rot, r4, r1))
                            p4 = intersect_point(p1_rot, p2_rot, r4, r1);

                        // Calculate the distance
                        tOverlap += sqrt((p3.x-p4.x)*(p3.x-p4.x) + (p3.y-p4.y)*(p3.y-p4.y));
                    }

                    // Partial overlap with no point contained
                    else
                    {
                        std::vector<cv::Point> p;

                        if (intersect(p1_rot, p2_rot, r1, r2))
                            p.push_back(intersect_point(p1_rot, p2_rot, r1, r2));
                        if (intersect(p1_rot, p2_rot, r2, r3))
                            p.push_back(intersect_point(p1_rot, p2_rot, r2, r3));
                        if (intersect(p1_rot, p2_rot, r3, r4))
                            p.push_back(intersect_point(p1_rot, p2_rot, r3, r4));
                        if (intersect(p1_rot, p2_rot, r4, r1))
                            p.push_back(intersect_point(p1_rot, p2_rot, r4, r1));

                        if (p.size() == 2)
                            tOverlap += sqrt((p[0].x-p[1].x)*(p[0].x-p[1].x) + (p[0].y-p[1].y)*(p[0].y-p[1].y));
                    }
                }

                // Have the new values increased the overlap?
                if (tOverlap > max_overlap)
                {
                    max_overlap = tOverlap;
                    optimal_angle = angle;
                    optimal_length = length;
                    has_improved = true;
                }
            }

            // Check if the optimal segment has been found
            // (i.e. no improvement in the last iteration)
            if (!has_improved)
            {
                // If a segment had been found, save it
                if (max_overlap > 0)
                {
                    // Track segment coordinates (forwardly rotated)
                    cv::Point r1_rot(x - cos(optimal_angle) * TRACK_WIDTH/2,
                                     y - sin(optimal_angle) * TRACK_WIDTH/2);
                    cv::Point r2_rot(x + cos(optimal_angle) * TRACK_WIDTH/2,
                                     y + sin(optimal_angle) * TRACK_WIDTH/2);
                    cv::Point r3_rot(x + cos(optimal_angle) * TRACK_WIDTH/2 + sin(optimal_angle) * optimal_length,
                                     y + sin(optimal_angle) * TRACK_WIDTH/2 - cos(optimal_angle) * optimal_length);
                    cv::Point r4_rot(x - cos(optimal_angle) * TRACK_WIDTH/2 + sin(optimal_angle) * optimal_length,
                                     y - sin(optimal_angle) * TRACK_WIDTH/2 - cos(optimal_angle) * optimal_length);

                    // Segment coordinates (center point of the top of the segment)
                    double x_new = x + optimal_length * sin(optimal_angle);
                    double y_new = y - optimal_length * cos(optimal_angle);

                    // Check the slope
                    double slope_current = atan2(y_new - oTrack.back().y,
                                                 x_new - oTrack.back().x);
                    double slope_prev;
                    if (oTrack.size() >= 2)
                    {
                        slope_prev = atan2(oTrack[oTrack.size()-1].y - oTrack[oTrack.size()-2].y,
                                           oTrack[oTrack.size()-1].x - oTrack[oTrack.size()-2].x);
                    }
                    if (oTrack.size() == 1 || abs(slope_current - slope_prev) <= SEGMENT_ANGLE_DELTA_MAX)
                    {
                        // Draw the segment
                        cv::line(iDebug, r1_rot, r2_rot, cv::Scalar(0, 255, 0), 1);
                        cv::line(iDebug, r2_rot, r3_rot, cv::Scalar(0, 255, 0), 1);
                        cv::line(iDebug, r3_rot, r4_rot, cv::Scalar(0, 255, 0), 1);
                        cv::line(iDebug, r4_rot, r1_rot, cv::Scalar(0, 255, 0), 1);

                        // Save the segment
                        oTrack.push_back(cv::Point(x_new, y_new));
                        tNewSegment = true;
                    }
                }

                // Stop the current iteration
                break;
            }
        }
    }

    return oTrack;
}

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
    cv::namedWindow("wp1", 1);


    //
    // Process video
    //

    cv::Mat tFrame;
    while (iVideo.grab() && iVideo.retrieve(tFrame))
    {
        // PREPROCESS //

        cv::Mat tFramePreprocessed = preprocess(tFrame);


        // FEATURE DETECTION //

        // Frame with features
        cv::Mat tFrameFeatures;
#if defined(DEBUG_PREPROCESSED)
        cvtColor(tFramePreprocessed, tFrameFeatures, CV_GRAY2BGR);
#else
        tFrameFeatures = tFrame.clone();
#endif

        // Detect lines
        std::vector<cv::Vec4i> tLines = find_lines(tFramePreprocessed, tFrameFeatures);

        // Find track start candidates
        std::vector<cv::Point> tTrackCandidates = find_track_start(tFramePreprocessed, tLines, tFrameFeatures);

        // Detect track segments
        if (tTrackCandidates.size() == 2)
        {
            int tTrackSpacing = abs(tTrackCandidates[0].x - tTrackCandidates[1].x);
            if (tTrackSpacing > TRACK_SPACE_MIN && tTrackSpacing < TRACK_SPACE_MAX)
            {
                // Detect both tracks
                std::vector<cv::Point> tTrack1 = find_track(tTrackCandidates[0], tLines, tFrameFeatures);
                std::vector<cv::Point> tTrack2 = find_track(tTrackCandidates[1], tLines, tFrameFeatures);

            }
        }


        // DISPLAY //

        // Display frame
        imshow("wp1", tFrameFeatures);
        if (oVideo.isOpened())
            oVideo << tFrameFeatures;

        // Halt on keypress
        if (cv::waitKey(30) >= 0)
            break;
    }

    return 0;
}
