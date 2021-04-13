# Bad Apple on windows 10 desktop
this is my go at playing bad apple on ____.
my inspiration for this video was by Junferno https://youtu.be/7WHA_Gi4nPA.

This version of BadApple runs in real time (if your computer can handle it ;-) 
The audio of the video is added in post

# Guide for replicating this
I do not advise to do this on your main machine because you'll need to clear your desktop and change the desktop background.

1. Download the bad apple video and with ffmpeg convert it to an image sequence
2. Go to Source.cpp and edit line 83 to the path of the image sequence
3. Clear the desktop, if some icons remaign clear the folder ``C:\Users\Public\Desktop``
4. Disable file name extensions
5. Go to the desktop and press ``CTRL-SHIFT-4`` to set the small icon size
6. Compile and run the code in the github
7. The program will create 1632 images on your desktop, so if explorer crashes close and reopen the program
8. Wait for the program to index all the files
9. Enjoy!
