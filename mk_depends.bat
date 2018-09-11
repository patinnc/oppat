@echo off
set SCR_DIR=%~dp0
perl %SCR_DIR%\scripts\cinclude.pl --include inc  --src src --quotetype quote --path --obj_sfx "$(OBJ_EXT)" > %SCR_DIR%\depends_win.mk

