
cl /EHsc /Ox /Zi -Iinc src/win_msr.c spin_wait/wait.cpp src/utils.cpp  /Fe:bin/wait.exe
if %ERRORLEVEL% GTR 0 goto err
del win_msr.obj wait.obj utils.obj


cl /EHsc /Ox /Zi -Iinc spin_wait/spin.cpp src/utils.cpp /Fe:bin/spin.exe 
if %ERRORLEVEL% GTR 0 goto err
del spin.obj utils.obj

cl /EHsc spin_wait/win_send_signal.cpp /Fe:bin/win_send_signal.exe
if %ERRORLEVEL% GTR 0 goto err
del win_send_signal.obj
goto :EOF

goto :EOF
:err
echo got error on compile
