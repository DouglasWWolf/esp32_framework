@echo off
rem rd/s/q build
git add main/*.cpp main/*.h main/CMakeLists.txt
git add CMakeLists.txt *.csv sdkconfig
git add push.bat
git commit -m "See History.h for changes"
git push origin main

