@echo off
start aicup2019.exe --config config2.json
start ..\strategy\x64\Debug\strategy.exe
..\strategy\x64\Release\strategy_6.exe 127.0.0.1 31002