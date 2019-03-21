
set UNI=/D_UNICODE
set UNI=
set RT_DIR=%~dp0
pushd .
cd libxlswriter
make  CC=cl AR=lib ARFLAGS="/out:" LIBXLSXWRITER_A=libxlswriter.lib  CFLAGS="-Ox %UNI% -Iinclude -I%RT_DIR%/zlib-1.2.11 /nologo " OBJ_END=obj %1

popd
goto :EOF

cl %UNI% tstit.c -I include src\libxlswriter.lib \data\ppat\ppat\zlib-1.2.11\zlib.lib
