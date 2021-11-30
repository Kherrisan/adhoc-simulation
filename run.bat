@echo off
echo wscript.sleep 200>sleep.vbs
start /wait sleep.vbs
start cmake-build-debug\server.exe 8888
start /wait sleep.vbs
start cmake-build-debug\server.exe 9999 wc
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10001
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10002 9999
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10003
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10004 9999
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10005
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10006
start /wait sleep.vbs
start cmake-build-debug\client.exe 8888 10007
start /wait sleep.vbs
start cmake-build-debug\blackhole.exe 8888 10008
del /f /s /q sleep.vbs