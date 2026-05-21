/* Copyright (c) 2005 Jeff Johnston <jjohnstn@redhat.com> */
#include <sys/types.h>
#include "local.h"

/* Shared timezone information for libc/time functions.  */
// static __tzinfo_type tzinfo;

// tcmips
static __tzinfo_type __tzinfo = {
    .__tznorth = 1,
    .__tzyear  = 0,

    .__tzrule[0] = {
        .ch     = 'M',
        .m      = 1,
        .n      = 1,
        .d      = 0,
        .s      = 0,     // 00:00 UTC
        .offset = 28800, // +8 hours
    },

    .__tzrule[1] = {
        .offset = 28800,
    }
};

__tzinfo_type *
__gettzinfo(void)
{
    return &__tzinfo;
}
