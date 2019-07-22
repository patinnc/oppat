del pat0*.png
del pat_dash0*.png

make & bin\oppat.exe -r ..\oppat_data\lnx\arm_gb10 --cpu_diagram web\arm_cortex_a53_block_diagram.svg --phase0 "both Blowfish" --phase1 "Stream Triad" --by_phase --ph_image pat --img_wxh_pxls 1148,1326 --follow_proc geekbench --skip_phases_with_string "idle: " > tmpa.jnk

goto :EOF

set END_PHASE="Single-Core JPEG"
set END_PHASE="Multi-Core Memory Bandwidth"
make & bin\oppat.exe -r ..\oppat_data\lnx\gb10 --cpu_diagram web\haswell_block_diagram.svg --phase0 "Single-Core AES" --phase1 %END_PHASE% --by_phase --ph_image pat --img_wxh_pxls 1017,1388 --follow_proc geekbench --skip_phases_with_string "idle: " > tmpa.jnk

goto :EOF
make & bin\oppat.exe -r ..\oppat_data\lnx\arm_multi10 --cpu_diagram web\arm_cortex_a53_block_diagram.svg --phase0 "end phase MT mem RD BW test" --phase1 "end phase MT spin test" --by_phase --ph_image pat --img_wxh_pxls 1148,1326 --follow_proc spin.x > tmpa.jnk

