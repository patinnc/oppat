
set OBJDIR=obj
mkdir bin %OBJDIR%

echo ml64.exe /c spin_wait/win_rorl.asm /Fo%OBJDIR%/win_rorl.obj
ml64.exe /Fo%OBJDIR%/win_rorl.obj /c spin_wait/win_rorl.asm 
cl /EHsc /Ox /Zi -Iinc spin_wait/spin.cpp %OBJDIR%/win_rorl.obj src/mygetopt.c src/utils2.cpp src/utils.cpp spin_wait/trace_marker.cpp /Fe:bin/spin.exe
if %ERRORLEVEL% GTR 0 goto err
del spin.obj utils.obj mygetopt.obj trace_marker.obj utils2.obj

cl /EHsc /Ox /Zi -Iinc src/utils.cpp  spin_wait/clocks.cpp /Fe:bin/clocks.exe
if %ERRORLEVEL% GTR 0 goto err
del clocks.obj utils.obj

cl /EHsc /Ox /Zi -Iinc src/win_msr.c spin_wait/wait.cpp src/utils.cpp  /Fe:bin/wait.exe
if %ERRORLEVEL% GTR 0 goto err
del win_msr.obj wait.obj utils.obj

cl /EHsc spin_wait/win_send_signal.cpp /Fe:bin/win_send_signal.exe
if %ERRORLEVEL% GTR 0 goto err
del win_send_signal.obj

cl /EHsc spin_wait/win_gui_delay.cpp spin_wait/trace_marker.cpp /Fe:bin/win_gui_delay.exe
del win_gui_delay.obj trace_marker.obj
if %ERRORLEVEL% GTR 0 goto err
goto :EOF

goto :EOF
:err
echo got error on compile
