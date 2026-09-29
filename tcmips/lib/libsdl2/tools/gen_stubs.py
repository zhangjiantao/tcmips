#!/usr/bin/env python3
"""
Generate sdl_stubs.c from the parsed SDL2 function JSON.
Functions in IMPLEMENTED are skipped; remaining ones become stubs.
"""
import json
import re
import sys

IMPLEMENTED = {  # same set as before
    "SDL_malloc", "SDL_calloc", "SDL_realloc", "SDL_free",
    "SDL_aligned_alloc", "SDL_aligned_free",
    "SDL_GetMemoryFunctions", "SDL_GetOriginalMemoryFunctions", "SDL_SetMemoryFunctions",
    "SDL_getenv", "SDL_setenv",
    "SDL_sscanf", "SDL_vsscanf", "SDL_snprintf", "SDL_vsnprintf", "SDL_asprintf", "SDL_vasprintf",
    "SDL_abs", "SDL_isalpha", "SDL_isalnum", "SDL_isblank", "SDL_iscntrl", "SDL_isdigit",
    "SDL_isxdigit", "SDL_ispunct", "SDL_isspace", "SDL_isupper", "SDL_islower", "SDL_isprint",
    "SDL_isgraph", "SDL_toupper", "SDL_tolower",
    "SDL_memset", "SDL_memcpy", "SDL_memmove", "SDL_memcmp",
    "SDL_strlen", "SDL_strlcpy", "SDL_utf8strlcpy", "SDL_strlcat", "SDL_strdup", "SDL_strrev",
    "SDL_strupr", "SDL_strlwr", "SDL_strchr", "SDL_strrchr", "SDL_strstr", "SDL_strcasestr",
    "SDL_strcmp", "SDL_strncmp", "SDL_strcasecmp", "SDL_strncasecmp", "SDL_strtokr",
    "SDL_utf8strlen", "SDL_utf8strnlen",
    "SDL_itoa", "SDL_uitoa", "SDL_ltoa", "SDL_ultoa", "SDL_lltoa", "SDL_ulltoa",
    "SDL_atoi", "SDL_atof", "SDL_strtol", "SDL_strtoul", "SDL_strtoll", "SDL_strtoull", "SDL_strtod",
    "SDL_wcslen", "SDL_wcslcpy", "SDL_wcslcat", "SDL_wcsdup", "SDL_wcsstr", "SDL_wcscmp",
    "SDL_wcsncmp", "SDL_wcscasecmp", "SDL_wcsncasecmp",
    "SDL_iconv_open", "SDL_iconv_close", "SDL_iconv", "SDL_iconv_string",
    "SDL_acos", "SDL_acosf", "SDL_asin", "SDL_asinf", "SDL_atan", "SDL_atanf", "SDL_atan2",
    "SDL_atan2f", "SDL_ceil", "SDL_ceilf", "SDL_copysign", "SDL_copysignf", "SDL_cos", "SDL_cosf",
    "SDL_exp", "SDL_expf", "SDL_fabs", "SDL_fabsf", "SDL_floor", "SDL_floorf", "SDL_fmod",
    "SDL_fmodf", "SDL_log", "SDL_logf", "SDL_log10", "SDL_log10f", "SDL_pow", "SDL_powf",
    "SDL_round", "SDL_roundf", "SDL_lround", "SDL_lroundf", "SDL_scalbn", "SDL_scalbnf",
    "SDL_sin", "SDL_sinf", "SDL_sqrt", "SDL_sqrtf", "SDL_tan", "SDL_tanf", "SDL_trunc", "SDL_truncf",
    "SDL_crc16", "SDL_crc32",
    "SDL_GetPlatform", "SDL_GetCPUCount",
    "SDL_Has3DNow", "SDL_HasAVX", "SDL_HasAVX2", "SDL_HasAVX512F", "SDL_HasAltiVec",
    "SDL_HasMMX", "SDL_HasRDTSC", "SDL_HasSSE", "SDL_HasSSE2", "SDL_HasSSE3",
    "SDL_HasSSE41", "SDL_HasSSE42", "SDL_HasNEON",
    "SDL_Init", "SDL_InitSubSystem", "SDL_QuitSubSystem", "SDL_Quit", "SDL_WasInit",
    "SDL_SetMainReady", "SDL_QuitRequested",
    "SDL_GetTicks", "SDL_GetTicks64", "SDL_GetPerformanceCounter", "SDL_GetPerformanceFrequency", "SDL_Delay",
    "SDL_PollEvent", "SDL_WaitEvent", "SDL_WaitEventTimeout", "SDL_PushEvent", "SDL_PumpEvents",
    "SDL_PeepEvents", "SDL_GetKeyboardState", "SDL_GetModState", "SDL_SetModState",
    "SDL_GetKeyFromScancode", "SDL_GetScancodeFromKey", "SDL_GetScancodeName",
    "SDL_GetScancodeFromName", "SDL_GetKeyName", "SDL_GetKeyFromName",
    "SDL_StartTextInput", "SDL_StopTextInput", "SDL_IsTextInputActive", "SDL_SetTextInputRect",
    "SDL_ClearComposition", "SDL_HasScreenKeyboardSupport", "SDL_IsScreenKeyboardShown",
    "SDL_GetMouseState", "SDL_GetGlobalMouseState", "SDL_GetRelativeMouseState",
    "SDL_WarpMouseInWindow", "SDL_SetRelativeMouseMode", "SDL_GetRelativeMouseMode", "SDL_CaptureMouse",
    "SDL_CreateCursor", "SDL_CreateColorCursor", "SDL_CreateSystemCursor", "SDL_GetDefaultCursor",
    "SDL_SetCursor", "SDL_GetCursor", "SDL_FreeCursor", "SDL_ShowCursor",
    "SDL_SetEventFilter", "SDL_GetEventFilter", "SDL_AddEventWatch", "SDL_DelEventWatch",
    "SDL_FilterEvents", "SDL_GetEventState", "SDL_SetEventEnabled", "SDL_FlushEvent",
    "SDL_FlushEvents", "SDL_HasEvent", "SDL_HasEvents",
    "SDL_RecordGesture", "SDL_SaveAllDollarTemplates", "SDL_SaveDollarTemplate", "SDL_LoadDollarTemplates",
    "SDL_CreateWindow", "SDL_CreateWindowFrom", "SDL_DestroyWindow", "SDL_GetWindowTitle",
    "SDL_SetWindowTitle", "SDL_SetWindowIcon", "SDL_SetWindowData", "SDL_GetWindowData",
    "SDL_GetWindowID", "SDL_GetWindowFromID", "SDL_GetWindowFlags",
    "SDL_GetWindowPosition", "SDL_GetWindowSize", "SDL_GetWindowSizeInPixels",
    "SDL_GetWindowBordersSize", "SDL_SetWindowPosition", "SDL_SetWindowSize",
    "SDL_SetWindowMinimumSize", "SDL_GetWindowMinimumSize", "SDL_SetWindowMaximumSize", "SDL_GetWindowMaximumSize",
    "SDL_SetWindowBordered", "SDL_SetWindowResizable", "SDL_SetWindowAlwaysOnTop", "SDL_SetWindowFullscreen",
    "SDL_SetWindowGrab", "SDL_GetWindowGrab", "SDL_SetWindowMouseGrab", "SDL_GetWindowMouseGrab",
    "SDL_SetWindowKeyboardGrab", "SDL_GetWindowKeyboardGrab",
    "SDL_SetWindowGammaRamp", "SDL_GetWindowGammaRamp", "SDL_SetWindowHitTest",
    "SDL_ShowWindow", "SDL_HideWindow", "SDL_RaiseWindow", "SDL_MaximizeWindow", "SDL_MinimizeWindow",
    "SDL_RestoreWindow", "SDL_DisableScreenSaver", "SDL_EnableScreenSaver", "SDL_IsScreenSaverEnabled",
    "SDL_UpdateWindowSurface", "SDL_UpdateWindowSurfaceRects", "SDL_GetWindowSurface",
    "SDL_VideoInit", "SDL_VideoQuit", "SDL_GetNumVideoDrivers", "SDL_GetVideoDriver",
    "SDL_GetCurrentVideoDriver", "SDL_GetNumDisplays", "SDL_GetDisplayName", "SDL_GetDisplayBounds",
    "SDL_GetDisplayUsableBounds", "SDL_GetDisplayDPI", "SDL_GetDisplayOrientation",
    "SDL_GetNumDisplayModes", "SDL_GetDisplayMode", "SDL_GetDesktopDisplayMode", "SDL_GetCurrentDisplayMode",
    "SDL_GetClosestDisplayMode", "SDL_GetWindowDisplayIndex", "SDL_GetWindowDisplayMode", "SDL_SetWindowDisplayMode",
    "SDL_GetWindowOpacity", "SDL_SetWindowOpacity", "SDL_GetWindowBrightness", "SDL_SetWindowBrightness",
    "SDL_SetWindowModalFor", "SDL_SetWindowInputFocus", "SDL_FlashWindow", "SDL_GetWindowPixelFormat",
    "SDL_ShowMessageBox", "SDL_ShowSimpleMessageBox",
    "SDL_HasIntersection", "SDL_IntersectRect", "SDL_UnionRect", "SDL_EnclosePoints",
    "SDL_IntersectRectAndLine", "SDL_HasIntersectionF", "SDL_IntersectFRect", "SDL_UnionFRect",
    "SDL_EncloseFPoints", "SDL_IntersectFRectAndLine", "SDL_PointInFRect", "SDL_RectEmpty", "SDL_FRectEmpty",
    "SDL_CreateRGBSurface", "SDL_CreateRGBSurfaceWithFormat", "SDL_CreateRGBSurfaceFrom",
    "SDL_CreateRGBSurfaceWithFormatFrom", "SDL_FreeSurface", "SDL_DuplicateSurface",
    "SDL_SetSurfacePalette", "SDL_LockSurface", "SDL_UnlockSurface",
    "SDL_SetSurfaceRLE", "SDL_SurfaceHasRLE", "SDL_SetColorKey", "SDL_HasColorKey", "SDL_GetColorKey",
    "SDL_SetSurfaceColorMod", "SDL_GetSurfaceColorMod", "SDL_SetSurfaceAlphaMod", "SDL_GetSurfaceAlphaMod",
    "SDL_SetSurfaceBlendMode", "SDL_GetSurfaceBlendMode", "SDL_SetClipRect", "SDL_GetClipRect",
    "SDL_FillRect", "SDL_FillRects", "SDL_UpperBlit", "SDL_BlitSurface", "SDL_LowerBlit",
    "SDL_UpperBlitScaled", "SDL_BlitScaled", "SDL_LowerBlitScaled",
    "SDL_SoftStretch", "SDL_SoftStretchLinear",
    "SDL_ConvertSurface", "SDL_ConvertSurfaceFormat", "SDL_ConvertPixels", "SDL_PremultiplyAlpha",
    "SDL_MapRGB", "SDL_MapRGBA", "SDL_GetRGB", "SDL_GetRGBA",
    "SDL_PixelFormatEnumToMasks", "SDL_MasksToPixelFormatEnum",
    "SDL_SetPixelFormatPalette", "SDL_SetPaletteColors", "SDL_AllocPalette", "SDL_FreePalette",
    "SDL_AllocFormat", "SDL_FreeFormat", "SDL_GetSurfacePalette",
    "SDL_CreateShapedWindow", "SDL_IsShapedWindow", "SDL_SetWindowShape", "SDL_GetShapedWindowMode",
    "SDL_GetNumRenderDrivers", "SDL_GetRenderDriverInfo", "SDL_CreateWindowAndRenderer", "SDL_CreateRenderer",
    "SDL_CreateSoftwareRenderer", "SDL_GetRenderer", "SDL_GetRendererInfo", "SDL_GetRendererOutputSize",
    "SDL_DestroyRenderer", "SDL_RenderClear", "SDL_RenderPresent", "SDL_RenderSetLogicalSize",
    "SDL_RenderGetLogicalSize", "SDL_RenderSetIntegerScale", "SDL_RenderGetIntegerScale",
    "SDL_RenderSetViewport", "SDL_RenderGetViewport", "SDL_RenderSetClipRect", "SDL_RenderGetClipRect",
    "SDL_RenderIsClipEnabled", "SDL_RenderSetScale", "SDL_RenderGetScale",
    "SDL_RenderWindowToLogical", "SDL_RenderLogicalToWindow", "SDL_RenderSetVSync", "SDL_RenderGetVSync",
    "SDL_SetRenderDrawColor", "SDL_GetRenderDrawColor", "SDL_SetRenderDrawBlendMode", "SDL_GetRenderDrawBlendMode",
    "SDL_RenderFillRect", "SDL_RenderFillRects", "SDL_RenderDrawRect", "SDL_RenderDrawRects",
    "SDL_RenderDrawLine", "SDL_RenderDrawLines", "SDL_RenderDrawPoint", "SDL_RenderDrawPoints",
    "SDL_RenderCopy", "SDL_RenderCopyEx", "SDL_RenderCopyExF", "SDL_RenderCopyF",
    "SDL_RenderGeometry", "SDL_RenderGeometryRaw", "SDL_RenderReadPixels", "SDL_RenderTargetSupported",
    "SDL_SetRenderTarget", "SDL_GetRenderTarget", "SDL_RenderGetMetalLayer", "SDL_RenderGetMetalCommandEncoder",
    "SDL_CreateTexture", "SDL_CreateTextureFromSurface", "SDL_CreateTextureWithProperties",
    "SDL_QueryTexture", "SDL_SetTextureColorMod", "SDL_GetTextureColorMod", "SDL_SetTextureAlphaMod",
    "SDL_GetTextureAlphaMod", "SDL_SetTextureBlendMode", "SDL_GetTextureBlendMode",
    "SDL_SetTextureScaleMode", "SDL_GetTextureScaleMode", "SDL_SetTextureWrapMode", "SDL_GetTextureWrapMode",
    "SDL_SetTextureUserData", "SDL_GetTextureUserData", "SDL_UpdateTexture", "SDL_UpdateYUVTexture",
    "SDL_UpdateNVTexture", "SDL_LockTexture", "SDL_LockTextureToSurface", "SDL_UnlockTexture",
    "SDL_DestroyTexture", "SDL_GL_BindTexture", "SDL_GL_UnbindTexture",
    "SDL_RenderDrawPointF", "SDL_RenderDrawPointsF", "SDL_RenderDrawLineF", "SDL_RenderDrawLinesF",
    "SDL_RenderDrawRectF", "SDL_RenderDrawRectsF", "SDL_RenderFillRectF", "SDL_RenderFillRectsF",
    "SDL_RenderCopyF", "SDL_RenderCopyExF",
    "SDL_RWFromFile", "SDL_RWFromMem", "SDL_RWFromConstMem", "SDL_AllocRW", "SDL_FreeRW",
    "SDL_ReadU8", "SDL_ReadLE16", "SDL_ReadBE16", "SDL_ReadLE32", "SDL_ReadBE32", "SDL_ReadLE64", "SDL_ReadBE64",
    "SDL_WriteU8", "SDL_WriteLE16", "SDL_WriteBE16", "SDL_WriteLE32", "SDL_WriteBE32", "SDL_WriteLE64", "SDL_WriteBE64",
    "SDL_RWsize", "SDL_RWseek", "SDL_RWtell", "SDL_RWread", "SDL_RWwrite", "SDL_RWclose",
    "SDL_LoadFile_RW", "SDL_LoadFile",
    "SDL_GetBasePath", "SDL_GetPrefPath", "SDL_GetClipboardText", "SDL_SetClipboardText", "SDL_HasClipboardText",
    "SDL_SetError", "SDL_GetError", "SDL_ClearError", "SDL_OutOfMemory", "SDL_GetErrorMsg",
    "SDL_Log", "SDL_LogVerbose", "SDL_LogDebug", "SDL_LogInfo", "SDL_LogWarn", "SDL_LogError", "SDL_LogCritical",
    "SDL_LogMessage", "SDL_LogMessageV", "SDL_LogSetPriority", "SDL_LogGetPriority", "SDL_LogResetPriorities",
    "SDL_LogSetOutputFunction", "SDL_LogGetOutputFunction",
    "SDL_SetHint", "SDL_SetHintWithPriority", "SDL_GetHint", "SDL_GetHintBoolean", "SDL_ClearHints",
    "SDL_AddHintCallback", "SDL_DelHintCallback",
}

FORCE_RETURN = {
    "SDL_ReportAssertion": "SDL_ASSERTION_IGNORE",
    "SDL_GetNumAudioDrivers": "1",
    "SDL_GetAudioDriver": "\"dummy\"",
    "SDL_GetCurrentAudioDriver": "\"dummy\"",
    "SDL_GetNumAudioDevices": "1",
    "SDL_GetAudioDeviceName": "\"dummy\"",
    "SDL_GetNumTouchDevices": "0",
    "SDL_GetTouchDevice": "0",
    "SDL_GetNumTouchFingers": "0",
    "SDL_GetTouchFinger": "NULL",
    "SDL_NumJoysticks": "0",
    "SDL_NumSensors": "0",
    "SDL_HapticOpen": "NULL",
    "SDL_HapticOpenFromMouse": "NULL",
    "SDL_HapticOpenFromJoystick": "NULL",
    "SDL_LoadObject": "NULL",
    "SDL_LoadFunction": "NULL",
    "SDL_CreateThread": "NULL",
    "SDL_CreateThreadWithStackSize": "NULL",
    "SDL_CreateMutex": "NULL",
    "SDL_CreateSemaphore": "NULL",
    "SDL_CreateCond": "NULL",
    "SDL_TLSCreate": "0",
    "SDL_GUIDFromString": "(SDL_GUID){0}",
    "SDL_JoystickGetDeviceGUID": "(SDL_JoystickGUID){0}",
    "SDL_JoystickGetGUID": "(SDL_JoystickGUID){0}",
    "SDL_JoystickGetGUIDFromString": "(SDL_JoystickGUID){0}",
    "SDL_GameControllerGetBindForAxis": "(SDL_GameControllerButtonBind){0}",
    "SDL_GameControllerGetBindForButton": "(SDL_GameControllerButtonBind){0}",
}

TYPE_DEFAULTS = {
    "void": "",
    "int": "0",
    "long": "0L",
    "Sint64": "0",
    "Uint64": "0",
    "Uint32": "0",
    "Uint16": "0",
    "Uint8": "0",
    "size_t": "0",
    "float": "0.0f",
    "double": "0.0",
    "char*": "NULL",
    "const char*": "NULL",
    "char *": "NULL",
    "const char *": "NULL",
    "SDL_bool": "SDL_FALSE",
}


def split_args(args_str):
    if args_str == 'void':
        return []
    parts = []
    depth = 0
    cur = []
    for ch in args_str:
        if ch == '(' or ch == '[' or ch == '<':
            depth += 1
            cur.append(ch)
        elif ch == ')' or ch == ']' or ch == '>':
            depth -= 1
            cur.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append(''.join(cur).strip())
    return parts


def normalize_type(ret):
    ret = ret.strip().replace(" ", " ")
    if ret.startswith("extern"):
        ret = ret[len("extern"):].strip()
    ret = ret.replace("DECLSPEC", "").replace("SDLCALL", "").strip()
    return ret


TYPE_KEYWORDS = {
    "void", "char", "short", "int", "long", "float", "double", "signed", "unsigned",
    "bool", "size_t", "ssize_t", "ptrdiff_t", "intptr_t", "uintptr_t",
    "Uint8", "Uint16", "Uint32", "Uint64", "Sint8", "Sint16", "Sint32", "Sint64",
    "SDL_AudioFormat", "SDL_bool", "SDL_Window", "SDL_Surface", "SDL_Renderer",
    "SDL_Texture", "SDL_RWops", "SDL_Event", "SDL_cond", "SDL_mutex", "SDL_sem",
    "SDL_cursor", "SDL_thread", "SDL_AudioSpec", "SDL_AudioCVT", "SDL_AudioStream",
    "SDL_Haptic", "SDL_Joystick", "SDL_GameController", "SDL_Sensor", "SDL_Mutex",
    "SDL_SpinLock", "SDL_ThreadFunction", "SDL_AssertData", "SDL_AssertionHandler",
    "SDL_MessageBoxData", "SDL_MessageBoxColorScheme", "SDL_Version", "SDL_Palette",
    "SDL_PixelFormat", "SDL_SysWMinfo", "SDL_HapticEffect", "SDL_GameControllerButtonBind",
}


def split_type_and_name(param):
    # If the parameter already ends with a name, return (type, name).
    # Otherwise return the whole param as the type and None as the name.
    m = re.match(r'^(.*\S)\s+([A-Za-z_][A-Za-z0-9_]*)$', param.strip())
    if m:
        # If the last identifier looks like a type, the parameter is anonymous.
        if m.group(2) not in TYPE_KEYWORDS:
            return m.group(1).strip(), m.group(2)
    return param.strip(), None


def gen_function_def(ret, name, args):
    if args == "void" or args == "":
        return f"{normalize_type(ret)} {name}(void)"
    parts = split_args(args)
    named_params = []
    for i, p in enumerate(parts):
        typ, nm = split_type_and_name(p)
        if nm is None:
            nm = f"__p{i}"
        named_params.append(f"{typ} {nm}")
    return f"{normalize_type(ret)} {name}({', '.join(named_params)})"


def default_return(ret, name):
    ret = normalize_type(ret)
    if name in FORCE_RETURN:
        return FORCE_RETURN[name]
    if ret in TYPE_DEFAULTS:
        return TYPE_DEFAULTS[ret]
    if "*" in ret:
        return "NULL"
    if "SDL_" in ret and ret.startswith("SDL_"):
        return "0"
    if ret in ("Uint64", "Sint64", "Sint32", "Uint32"):
        return "0"
    return "0"


def gen_stub(fn):
    name = fn["name"]
    # The host AST occasionally resolves a platform-specific type differently
    # from the header we compile with. Override where needed.
    if name == "SDL_RWFromFP":
        fn = dict(fn)
        fn["args"] = "void * fp, SDL_bool autoclose"
        fn["ret"] = "SDL_RWops *"
    ret = fn["ret"]
    args = fn["args"]

    body = [f'    TCMIPS_SDL_STUB_MSG("{name}");']

    params = split_args(args)
    for i, p in enumerate(params):
        _, pn = split_type_and_name(p)
        if pn is None:
            pn = f"__p{i}"
        body.append(f"    (void){pn};")

    expr = default_return(ret, name)
    if expr == "":
        return gen_function_def(ret, name, args) + " {\n" + "\n".join(body) + "\n}\n"
    return gen_function_def(ret, name, args) + " {\n" + "\n".join(body) + f"\n    return {expr};\n}}\n"


def main():
    funcs_path = sys.argv[1]
    out_path = sys.argv[2]
    with open(funcs_path) as f:
        funcs = json.load(f)

    lines = [
        "/* Auto-generated SDL2 stubs for TCMIPS compatibility layer.",
        " * Generated by tools/gen_stubs.py — do not edit manually. */",
        "",
        '#include <SDL.h>',
        '#include <stdio.h>  /* for FILE* used by SDL_RWFromFP stubs */',
        '#include "tcmips_sdl2_internal.h"',
        "",
    ]

    stubbed = []
    for fn in funcs:
        name = fn["name"]
        if name in IMPLEMENTED:
            continue
        lines.append(gen_stub(fn))
        stubbed.append(name)

    with open(out_path, 'w') as f:
        f.write("\n".join(lines))

    print(f"Wrote {out_path} ({len(stubbed)} stubs)")


if __name__ == "__main__":
    main()
