del pat0*.png
make & bin\oppat.exe -r ..\oppat_data\lnx\arm_gb10 --cpu_diagram web\arm_cortex_a53_block_diagram.svg --phase0 "both Blowfish" --phase1 "Stream Triad" --by_phase --ph_image pat --img_wxh_pxls 1148,1326 --follow_proc geekbench > tmpa.jnk

goto :EOF
make & bin\oppat.exe -r ..\oppat_data\lnx\arm_multi10 --cpu_diagram web\arm_cortex_a53_block_diagram.svg --phase0 "end phase MT mem RD BW test" --phase1 "end phase MT spin test" --by_phase --ph_image pat --img_wxh_pxls 1148,1326 --follow_proc spin.x > tmpa.jnk

