@echo off
del 1.txt
start aicup2019.exe --batch-mode --config config2.json --save-results 1.txt
start ..\strategy\x64\Release\strategy_6.exe 127.0.0.1 31002
..\strategy\x64\Debug\strategy.exe
type 1.txt
pause
del 1.txt