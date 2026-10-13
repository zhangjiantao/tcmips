#!/usr/bin/env bash
set -e

cd "$(dirname "$0")"

cc -std=c11 -I../../../tcmips/lib/libmemfs/include -I../../../toolchains/sysroot/usr/include test_memfs.c ../../../tcmips/lib/libmemfs/memfs.c -o /tmp/test_memfs
/tmp/test_memfs

cc -std=c11 -DTCM_MEMFS_DEBUG -I../../../tcmips/lib/libmemfs/include -I../../../toolchains/sysroot/usr/include test_memfs.c ../../../tcmips/lib/libmemfs/memfs.c -o /tmp/test_memfs_debug
/tmp/test_memfs_debug
