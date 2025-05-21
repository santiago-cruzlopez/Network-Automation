// https://stackoverflow.com/questions/29638812/capture-frame-with-opencv-using-blackmagic-card-intensity-shuttle

from Capture import Capture
import cv2
import os
from src import improcess

 # video recorder


capture = cv2.VideoCapture(1)

cap = Capture(capture)
fourcc = cv2.cv.CV_FOURCC(*'MSVC')  # cv2.VideoWriter_fourcc() does not exist
video_writer = cv2.VideoWriter("C:\Users\Syllia\Videos\output3.avi", fourcc, 30.0, (640, 480))

i = 0
cap.set_brightness(25)

while(capture.isOpened()):
    # Capture frame-by-frame

    ret, frame = cap.cap.read()
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    if(i==120):  
        cap.adjust_brightness(100, frame)
    if i >= 240 and i<360:
        frame = improcess.binarize(170, frame)

    if i>360:
        frame = gray


    video_writer.write(frame)
    cv2.imshow('frame',frame)
    i = i+1

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

# When everything done, release the capture
video_writer.release()
capture.release()
cv2.destroyAllWindows()
