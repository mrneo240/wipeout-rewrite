psp-docker meson psp_builddir --cross-file mips-allegrex-psp  -Drenderer=gu -Dplatform=psp
psp-docker meson compile -C psp_builddir

psp-docker psp-fixup-imports wipeout-rewrite
psp-docker psp-prxgen wipeout-rewrite wipeout-rewrite.prx
psp-docker mksfo  'wipeout-rewrite' PARAM.SFO
psp-docker pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL wipeout-rewrite.prx NULL

http://ppsspp-debugger.unknownbrackets.org/cpu

ninja -C dc_builddir/ -t compdb -x c_COMPILER cpp_COMPILER > dc_builddir/compile_commands.json
