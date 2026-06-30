# https://github.com/evansm7/pico-mac/issues/25

xxd -i 1986-03-4D1F8172-MacPlusv3.ROM > umac-rom.h
xxd -i DEMO.dsk > umac-disc.h