# Map-Processor v1.0
![asdf](https://user-images.githubusercontent.com/125653767/233795295-60679642-a980-4f1f-8e0a-cf8c7abeac65.jpeg)
A console application that calculates province positions and their neighbours


This is a simple console application that calculates provinces' positions, and their neighbours based on an appropriate input image.
You can use the application from a console (called Command Prompt on Windows or Terminal on Linux). The program uses flags which can be accessed after a quick

`Map.exe --help`

Command.
The application also includes automatic flags (in default_flags.txt) which can be changed for faster input.
There's also an example next to the program so feel free to experiment with that.

## Dependencies:
- Opencv 4.6.0 - https://opencv.org/releases/
- C++ standard library

## More information:
- This project was made in Code Blocks

## How to run?
- If you are on Windows, please for the love of God, don't try to run this in Code Blocks; try Visual Studio or Find my Itch.io project which is already compiled and ready to run (https://shadow9876.itch.io/map-processor)
- If you are on Linux try the prebuilt libraries and if that doesn't work, well... there are a dozen of tutorials which may or may not work, good luck regardless!

## Efficiency
- This project is not optimised for memory... so please don't run a 16000x5600 image with 5000 provinces because it won't work
- I know it stores too many images, but I don't have a better idea as of yet
