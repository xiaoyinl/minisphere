/**
 *  miniSphere JavaScript game engine
 *  Copyright (c) 2015-2017, Fat Cerberus
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name of miniSphere nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
**/

#ifdef _MSC_VER
#define _CRT_NONSTDC_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _USE_MATH_DEFINES
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include <setjmp.h>
#include <time.h>
#include <sys/stat.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_audio.h>
#include <allegro5/allegro_acodec.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_memfile.h>
#include <allegro5/allegro_native_dialog.h>
#include <allegro5/allegro_primitives.h>
#include <ChakraCore.h>
#include <duktape.h>
#include <dyad.h>
#include <zlib.h>

#include "lstring.h"
#include "path.h"
#include "vector.h"

#include "version.h"

#include "geometry.h"
#include "console.h"
#include "game.h"
#include "jsal.h"
#include "kev_file.h"
#include "font.h"
#include "screen.h"
#include "script.h"
#include "utility.h"

#ifdef _MSC_VER
#define strcasecmp stricmp
#define snprintf _snprintf
#endif

#define SPHERE_PATH_MAX 1024

#if defined(__GNUC__)
#define no_return __attribute__((noreturn)) void
#elif defined(__clang__)
#define no_return __attribute__((noreturn)) void
#elif defined(_MSC_VER)
#define no_return __declspec(noreturn) void
#else
#define no_return void
#endif

// at some point some of these global variables need to get eaten, preferably by
// an eaty pig.  they're a relic from the early stages of miniSphere development and
// I've been too lazy to try to refactor them away.
extern duk_context*         g_duk;
extern ALLEGRO_EVENT_QUEUE* g_events;
extern game_t*              g_game;
extern path_t*              g_game_path;
extern path_t*              g_last_game_path;
extern screen_t*            g_screen;
extern font_t*              g_system_font;

no_return sphere_abort   (const char* message);
no_return sphere_exit    (bool shutting_down);
no_return sphere_restart (void);
void      sphere_run     (bool allow_dispatch);
void      sphere_sleep   (double time);
