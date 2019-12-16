@echo off
del log.txt
set /p last=<last.txt

for /L %%u in (1,1,10) do ( 
    start aicup2019.exe --batch-mode --config config4.json --save-results log.txt
    start ..\strategy\x64\Release\strategy_%last%.exe 127.0.0.1 31002
    ..\strategy\x64\Debug\strategy.exe
    type log.txt
    del log.txt
)

pause