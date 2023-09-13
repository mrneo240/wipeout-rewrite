#!/bin/sh
psp-fixup-imports $1
psp-prxgen $1 `basename $1`.prx
mksfo  'wipeout-rewrite' PARAM.SFO
pack-pbp EBOOT.PBP PARAM.SFO NULL NULL NULL NULL NULL `basename $1`.prx NULL
cp `basename $1`.prx EBOOT.BIN
cp `basename $1`.prx BOOT.BIN
#cp `basename $1`.prx ../
#cp `basename $1` ../wipeout-rewrite_psp.elf
