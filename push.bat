@echo off
rem rd/s/q build
git add main/*.cpp main/*.c main/*.h main/component.mk main/CMakeLists.txt
git add CMakeLists.txt partitions.csv sdkconfig
git add push.bat
git commit -m "See History.h for changes"
git push origin main

