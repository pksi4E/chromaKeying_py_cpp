#include <iostream>
#include <cmath>

#include <opencv2/core/mat.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

using namespace std;

constexpr char ESC { 27 };
constexpr uchar Y { 0 }, Cr { 1 }, Cb { 2 };

cv::Mat gFrameInter; // frame to choose the options from
cv::Mat gFrameInterClone;
cv::Mat gFrameInterYCC;
cv::Mat gFrIntCh[3];

int gThresh1 { 10 }, gThresh2 { 70 };
cv::Mat gBMask;
int gSliderPos1 { 7 };
bool gSliderAvailable { false };
cv::Scalar gMean;
cv::Mat gMeanColor;
cv::Mat backgroundImage;

void toleranceSliderMove(int, void*);
void onMouse(int, int, int, int, void*);
cv::Mat getOneFrame(cv::VideoCapture);
double valueForMask(int, int, cv::Scalar, cv::Scalar, int, int);
void createMask(int);
void createOutput();

// 1. When applications starts, we click LEFT MOUSE BUTTON
//      on the green screen to select the roi to take the mean value from,
//      and little rectangle will show up,
// 2. After that, we click RIGHT MOUSE BUTTON,
//      and the background is now replaced with the new background.
// 3. Next, we can now move the slider Tolerance to make output better.
// 4. Click ESC, to start saving the video.
// 5. You can click ESC again, when saving window is still open, to cancel
//      the saving at this point.
//
// !!! If we want to change from image 'demo' to 'asteroid', please change
//       fileName to:  fileName = text2 and fileNameModified = textModified2.
// !!! I'm sorry, but the size of the video is big.
int main()
{
    // Names
    cv::String text1 {"greenscreen-demo.mp4"}, //1920x1080
                text2 {"greenscreen-asteroid.mp4"}; //1280x720
    cv::String textModified1 {"greenscreen-demo-Modified.avi"},
                textModified2 {"greenscreen-asteroid-Modified.avi"};
    cv::String fileName = text1;
    cv::String fileNameModified = textModified1;
    cv::String slidersWinname { "Sliders Window" };
    cv::String interWinname { "Interface" };
    cv::String backgroundImageName { "orangeBlueSky.jpg" };

    // background image
    backgroundImage = cv::imread(backgroundImageName, cv::IMREAD_COLOR);
    if (fileName == text1)
        backgroundImage = backgroundImage(cv::Range(0, 1080), cv::Range(0, 1920));
    else if (fileName == text2)
        backgroundImage = backgroundImage(cv::Range(0, 720), cv::Range(0, 1280));
    backgroundImage.convertTo(backgroundImage, CV_32FC3);

    // video capture object
    cv::VideoCapture vin{ fileName, cv::CAP_ANY }; // vin ~ videoInput
    if (!vin.isOpened()) {
        cout << "Error with loading a video!\n";
        return -1;
    }
    int vidWidth = static_cast<int>(vin.get(cv::CAP_PROP_FRAME_WIDTH));
    int vidHeight = static_cast<int>(vin.get(cv::CAP_PROP_FRAME_HEIGHT));
    cout << vidWidth << " x " << vidHeight << '\n';

    // video writer object
    cv::VideoWriter vout{ fileNameModified, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'),
                24, cv::Size{vidWidth, vidHeight} };

    // /////////// --- Interface --- ////////////// //
    //**********************************************//
    cv::namedWindow(slidersWinname, cv::WINDOW_NORMAL); // sliders window
    cv::namedWindow(interWinname, cv::WINDOW_NORMAL); // interface window

    // Trackbar and mouse callback
    cv::createTrackbar("Tolerance", slidersWinname, &gSliderPos1, 25, toleranceSliderMove);
    cv::setMouseCallback(interWinname, onMouse);

    vin >> gFrameInter;
    gFrameInterClone = gFrameInter.clone(); // copy of the interface frame

    // converting the interface frame to YCrCb and splitting the channels
    cv::cvtColor(gFrameInter, gFrameInterYCC, cv::COLOR_BGR2YCrCb);
    cv::split(gFrameInterYCC, gFrIntCh);

    // mask for background, initialized with zeros
    gBMask = cv::Mat::zeros(gFrameInter.size().height, gFrameInter.size().width, CV_32FC3);

    cv::imshow(interWinname, gFrameInter); cv::waitKey(0);
    cv::destroyAllWindows();


    // ///////// --- Saving the video --- ///////// //
    //**********************************************//
    cv::Mat frame, frame2;
    cv::Mat frameYCC, frameYCCCh[3];
    cv::Mat Cr, mask;
    cv::Mat channels[3];
    char keyPressed{};

    // saving loop
    while (keyPressed != ESC) {
        vin >> gFrameInter;
        if (gFrameInter.empty()) break;
        cv::cvtColor(gFrameInter, gFrameInterYCC, cv::COLOR_BGR2YCrCb);
        cv::split(gFrameInterYCC, gFrIntCh);

        createMask(gThresh2);
        createOutput();

        vout << gFrameInter;
        cv::namedWindow("ESC to stop saving...");
        keyPressed = cv::waitKey(1) & 0xFF;
    }

    // --- Ending --- //
    //****************//
    vin.release();
    vout.release();
    cv::destroyAllWindows();
    return 0;
}

void toleranceSliderMove(int, void*)
{
    gFrameInter = gFrameInterClone.clone();
    gSliderPos1 = gSliderPos1 * 10;
    gThresh2 = gSliderPos1;
    createMask(gThresh2);
    createOutput();
    cv::imshow("Interface", gFrameInter);
}

void onMouse(int event, int x, int y, int flags, void* userdata)
{
    cv::Point pt{x, y};
    cv::Point ptRect1{x - 100, y - 50};
    cv::Point ptRect2{x + 100, y + 50};
    cv::Rect rect1{ ptRect1, ptRect2 };
    static bool canCut = false;

    if (event == cv::EVENT_LBUTTONDOWN) {
        // draw small rectangle
        cv::rectangle(gFrameInter, ptRect1, ptRect2, cv::Scalar{0, 0, 255}, 3);
        cv::imshow("Interface", gFrameInter);

        // mean in the rect of the 3 YCC channels
        gMean = cv::mean(gFrameInterYCC(rect1), cv::noArray()); // gMean is a Scalar

        // changing mean color from YCC to BGR
        //   and creating Mat with elements equal to the chosen color
        gMeanColor = cv::Mat{1, 1, CV_8UC3, cv::Scalar(gMean)};
        cv::cvtColor(gMeanColor, gMeanColor, cv::COLOR_YCrCb2BGR);
        cv::Scalar vec = gMeanColor.at<cv::Vec3b>(0, 0);
        gMeanColor = cv::Mat{gFrameInter.size().height, gFrameInter.size().width,
                CV_32FC3, cv::Scalar{vec}};

        canCut = true;
        gSliderAvailable = true;
    }
    else if (event == cv::EVENT_RBUTTONDOWN && canCut) {
        // --- creating a mask!!! ---
        gFrameInter = gFrameInterClone.clone();
        createMask(gThresh2);
        createOutput();
        cv::imshow("Interface", gFrameInter);
    }
}

double valueForMask(int CrPoint, int CbPoint,
                cv::Scalar CrChosenPoint, cv::Scalar CbChosenPoint,
                int th1, int th2)
{
    double dist; // distance between (Cr, Cb) and (CrChosen, CbChosen)
    dist = std::sqrt(std::pow((CrPoint - CrChosenPoint[0]), 2)
            + std::pow((CbPoint - CbChosenPoint[0]), 2));

    if (dist < th1)
        return 1.0;
    else if (dist < th2)
        return 1 - ((dist - th1) / (th2 - th1));
    else
        return 0.0;
}

void createMask(int thresh)
{
    if (gSliderAvailable) {
        int widthFr = gFrameInter.size().width;
        int heightFr = gFrameInter.size().height;
        int ptCr, ptCb;

        for (int i = 0; i < heightFr; ++i) {
            for (int j = 0; j < widthFr; ++j) {
                ptCr = static_cast<int>( gFrIntCh[Cr].at<uchar>(i, j) );
                ptCb = static_cast<int>( gFrIntCh[Cb].at<uchar>(i, j) );

                gBMask.at<cv::Vec3f>(i, j)
                        = cv::Vec3f::all( valueForMask(ptCr, ptCb,
                                                       gMean[Cr], gMean[Cb],
                                                       gThresh1, thresh)
                                         );
            }
        }
        // Display gBMask
//        cv::namedWindow("gBMask", cv::WINDOW_NORMAL);
//        cv::imshow("gBMask", gBMask); cv::waitKey(0);
    }
}

void createOutput()
{
    cv::Mat help1, help2, out1;

    cv::multiply(gBMask, gMeanColor, help1);
    help1.convertTo(help1, CV_8UC3);

    cv::multiply(gBMask, backgroundImage, help2);
    help2.convertTo(help2, CV_8UC3);

    cv::subtract(gFrameInter, help1, out1);
    cv::add(out1, help2, gFrameInter);
}
