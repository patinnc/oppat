set ph_end=Single-Core PDF Rendering
set ph_end=Single-Core LZMA
set ph_end=Multi-Core Memory Bandwidth
del pat*.png
make && bin\oppat.exe -r ..\oppat_data\lnx\gb8 --cpu_diagram web\haswell_block_diagram.svg --ph_image pat --by_phase --phase0 "Single-Core AES" --phase1 "%ph_end%" --img_wxh_pxls 1017,1388 > tmp.jnk

goto :EOF
make && bin\oppat.exe -r ..\oppat_data\lnx\multi7 --cpu_diagram web\haswell_block_diagram.svg --ph_image pat --by_phase --phase0 "end phase MT mem RD BW test" --phase1 "end phase MT spin test"  > tmp.jnk
make && bin\oppat.exe -r ..\oppat_data\lnx\gb8 --cpu_diagram web\haswell_block_diagram.svg --phase0 "Single-Core AES" --phase1 "%ph_end%" --ph_image pat --ph_step_int 0.1 > tmp.jnk
make && bin\oppat.exe -r ..\oppat_data\lnx\gb8 --cpu_diagram web\haswell_block_diagram.svg --phase AES --ph_image pat --ph_step_int 0.1,30 > tmp.jnk

@rem make && bin\oppat.exe -r ..\oppat_data\lnx\multi7 --cpu_diagram web\haswell_block_diagram.svg --ph_image pat --ph_step_int 3.0 --phase0 "begin phase MT mem RD BW test" --phase1 "end phase MT spin test"  > tmp.jnk
