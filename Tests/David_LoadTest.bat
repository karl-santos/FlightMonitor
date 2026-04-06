@echo off
SET SERVER_IP=10.144.114.0
SET PORT=6767
SET /A "index = 1"
SET /A "count = 25"

:while
if %index% leq %count% (
     START /MIN Client.exe %SERVER_IP% %PORT% "Telem_2023_3_12 14_56_40.txt"
     SET /A index = %index% + 1
     @echo %index%
     goto :while
)