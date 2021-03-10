"""
chromaKeying.py
"""
import numpy as np
import cv2 as cv
import math

ESC = 27
Y, Cr, Cb = 0, 1, 2

gSliderPos1 = 7
gFrIntCh = [0, 0, 0]
gThresh1, gThresh2 = 10, 70
gSliderAvailable = False
gCanCut = False 

## 1. When application starts, we click LEFT MOUSE BUTTON
##      on the green screen to select the roi to take the mean value from,
##      and little rectangle will show up.
## 2. Next, we click the RIGHT MOUSE BUTTON,
##      and the background is now replaced with the new background.
## 3. Next, we can now move the slider Tolerance to make output better.
## 4. Click ESC, to start saving the video.
## 5. You can click ESC again, when saving window is still open,
##      to cancel the saving at this point.
##
## 1b. Alternatively, when app starts you can move the slider to the
##      proper position and then select the roi by clicking the
##      LEFT MOUSE BUTTON and then click RIGHT MOUSE BUTTON to switch
##      the background.
##
## !!! If we want to change from image 'demo' to 'asteroid', please change
##      fileName to: fileName = text2 and fileNameModified = textModified2
## !!! I'm sorry, but the size of the video is big and saving is slow.

def toleranceSliderMove(*args):
    """ Function responsible for slider changes. We're modyfing
        an interface frame accoring to the slider param. """
    global gFrameInter, gFrameInterCopy, gSliderPos1, gThresh2

    gFrameInter = gFrameInterCopy.copy()
    gSliderPos1 = args[0] * 10
    gThresh2 = gSliderPos1
    createMask(gThresh2)
    createOutput()
    cv.imshow("Interface", gFrameInter)

def onMouse(event, x, y, flag, userdata):
    """ Function responsible for selecting proper rectangle on an image,
        creating mask and showing results to the user. """
    global gMean, gMeanColor, gFrameInter, gFrameInterYCC, gThresh2
    global gCanCut, gSliderAvailable

    pt = (x, y)
    ptRect1 = (x - 100, y - 50)
    ptRect2 = (x + 100, y + 50)

    if event == cv.EVENT_LBUTTONDOWN:
        # Draw small rectangle
        cv.rectangle(gFrameInter, ptRect1, ptRect2, (0, 0, 255), 3)
        cv.imshow("Interface", gFrameInter)

        # Mean in the rectangle of the 3 YCCchannels
        gMean = cv.mean(gFrameInterYCC[y - 50 : y + 50, x - 100 : x + 100])
        gMean = gMean[:3]

        # Changing mean color from YCC to BGR
        # and creating ndarray with elements equal to the chosen color
        gMeanColor = np.array([[gMean]], dtype=np.uint8)
        gMeanColor = cv.cvtColor(gMeanColor, cv.COLOR_YCrCb2BGR)
        gMeanColor = gMeanColor[0, 0] * np.ones((gFrameInter.shape[0],
                                    gFrameInter.shape[1], 3), dtype=np.float32)

        gCanCut = True
        gSliderAvailable = True

    elif event == cv.EVENT_RBUTTONDOWN and gCanCut:
        gFrameInter = gFrameInterCopy.copy()
        createMask(gThresh2)
        createOutput()
        cv.imshow("Interface", gFrameInter)


def createMask(thresh):
    """ Funcion that creates a mask. """
    global gSliderAvailable, gBMask, gFrameInter, gMean, gThresh1

    if gSliderAvailable:
        widthFr = gFrameInter.shape[1]
        heightFr = gFrameInter.shape[0]
        # print(widthFr, heightFr)
        for i in range(heightFr):
            for j in range(widthFr):
                ptCr = int(gFrIntCh[Cr][i, j])
                ptCb = int(gFrIntCh[Cb][i, j])

                val = valueForMask(ptCr, ptCb,
                                    gMean[Cr], gMean[Cb],
                                    gThresh1, thresh)
                gBMask[i, j] = (val, val, val)

        # Display gBMask
        # cv.namedWindow("gBMask", cv.WINDOW_NORMAL)
        # cv.imshow("gBMask", gBMask); cv.waitKey(0)

        

def valueForMask(CrPoint, CbPoint, CrChosenPoint, CbChosenPoint, th1, th2):
    """ Function that gets the proper value for a mask. """
    dist = math.sqrt(math.pow(CrPoint - CrChosenPoint, 2)
                        + math.pow(CbPoint - CbChosenPoint, 2))
    if dist < th1:
        return 1.0
    elif dist < th2:
        return 1 - ((dist - th1) / (th2 - th1))
    else:
        return 0.0

def createOutput():
    """ Function calculating the output image with modified background. """
    global gFrameInter, backgroundImage, gSliderAvailable

    if gSliderAvailable:
        help1 = cv.multiply(gBMask, gMeanColor)
        help1 = np.uint8(help1)

        help2 = cv.multiply(gBMask, backgroundImage)
        help2 = np.uint8(help2)

        out1 = cv.subtract(gFrameInter, help1)
        gFrameInter = cv.add(out1, help2)

def main():
    global gFrameInter, gFrameInterCopy
    global gFrameInterYCC, gBMask, gSliderPos1
    global backgroundImage, gThresh2

    # Names
    text1 = "greenscreen-demo.mp4" # 1920x1080
    text2 = "greenscreen-asteroid.mp4" # 1280x720
    textModified1 = "greenscreen-demo-Modified.avi"
    textModified2 = "greenscreen-asteroid-Modified.avi"
    fileName = text1
    fileNameModified = textModified1
    slidersWinname = "Sliders Window"
    interWinname = "Interface"
    backgroundImageName = "orangeBlueSky.jpg"

    # Background image
    backgroundImage = cv.imread(backgroundImageName, cv.IMREAD_COLOR)
    # Fitting the background image for the video
    if fileName == text1:
        backgroundImage = backgroundImage[0:1080, 0:1920]
    elif fileName == text2:
        backgroundImage = backgroundImage[0:720, 0:1280]
    backgroundImage = np.float32(backgroundImage)
    print(backgroundImage.dtype)

    # Video capture object
    vin = cv.VideoCapture(fileName, cv.CAP_ANY) # vin ~ video input
    if not vin.isOpened():
        print("Error with loading a video!")
        return -1

    # Resolution of the video
    vidWidth = int(vin.get(cv.CAP_PROP_FRAME_WIDTH))
    vidHeight = int(vin.get(cv.CAP_PROP_FRAME_HEIGHT))
    print(f"{vidWidth} x {vidHeight}")

    # Video writer object
    vout = cv.VideoWriter(fileNameModified,
                            cv.VideoWriter_fourcc('M', 'J', 'P', 'G'),
                            24, (vidWidth, vidHeight))

    ###########################
    ##### // INTERFACE \\ #####
    ###########################

    # Setting windows
    cv.namedWindow(slidersWinname, cv.WINDOW_NORMAL) # sliders window
    cv.namedWindow(interWinname, cv.WINDOW_NORMAL) # interface window

    # Trackbar and mouse callback
    cv.createTrackbar("Tolerance", slidersWinname,
                        gSliderPos1, 25, toleranceSliderMove)
    cv.setMouseCallback(interWinname, onMouse)

    # Reading a single frame for the interface
    retval, gFrameInter = vin.read()
    gFrameInterCopy = gFrameInter.copy()

    # Converting the interface frame to YCrCb and splitting the channels
    gFrameInterYCC = cv.cvtColor(gFrameInter, cv.COLOR_BGR2YCrCb)
    gFrIntCh[Y], gFrIntCh[Cr], gFrIntCh[Cb] = cv.split(gFrameInterYCC)

    # Mask for background initialized with zeros
    gBMask = np.zeros((gFrameInter.shape[0], gFrameInter.shape[1], 3), dtype=np.float32)
    print(gBMask.shape)

    cv.imshow(interWinname, gFrameInter)
    cv.waitKey(0)
    cv.destroyAllWindows()

    ##### // SAVING THE VIDEO \\ #####
    keyPressed = 0
    while keyPressed != ESC:
        retval, gFrameInter = vin.read()
        
        if retval == True:
            gFrameInterYCC = cv.cvtColor(gFrameInter, cv.COLOR_BGR2YCrCb)
            gFrIntCh[Y], gFrIntCh[Cr], gFrIntCh[Cb] = cv.split(gFrameInterYCC)
            
            createMask(gThresh2)
            createOutput()

            vout.write(gFrameInter)
            cv.namedWindow("ESC to stop saving...")
            keyPressed = cv.waitKey(1) & 0xFF

        else:
            break

    # Ending
    vin.release()
    vout.release()
    cv.destroyAllWindows()

    return 0

if __name__ == "__main__":
    main()