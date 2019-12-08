@echo off
set /p last=<last.txt
start aicup2019.exe --config config2.json
start ..\strategy\x64\Debug\strategy.exe
..\strategy\x64\Release\strategy_%last%.exe 127.0.0.1 31002