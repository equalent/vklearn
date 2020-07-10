@echo off
mkdir gen_win64
cd gen_win64
cmake -G "Visual Studio 16 2019" ..
cd ..