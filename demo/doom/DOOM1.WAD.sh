cd "$(dirname "$(readlink -f "$0")")"
wget https://www.jbserver.com/downloads/games/doom/misc/shareware/doom1.wad.zip
unzip doom1.wad.zip
xxd -i doom1.wad > doomgeneric/DOOM1.WAD.c