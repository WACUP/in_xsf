/*
	Copyright (C) 2005 Guillaume Duhamel
	Copyright (C) 2008-2012 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <algorithm>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

using u64 = uint64_t;
using s64 = int64_t;
using u32 = uint32_t;
using s32 = int32_t;
using u16 = uint16_t;
using s16 = int16_t;
using u8 = uint8_t;
using s8 = int8_t;
#ifndef FORCEINLINE
#define FORCEINLINE inline
#endif

#ifdef _WINDOWS
# define HAVE_LIBAGG
# define ENABLE_SSE
# define ENABLE_SSE2
// TODO - this doesn't work for all files under the x64
//		  builds. so for now it's simpler to disable it
//		  & reduce the compiled size until the crashing
//		  can be resolved either with a patch or from a
//		  potential move to a newer desmume revision...
/*/#define HAVE_JIT/*/
#undef HAVE_JIT/**/
#endif

#ifdef __GNUC__
# ifdef __SSE__
#  define ENABLE_SSE
# endif
# ifdef __SSE2__
#  define ENABLE_SSE2
# endif
# define HAVE_JIT
#endif

#ifdef NOSSE
# undef ENABLE_SSE
#endif

#ifdef NOSSE2
# undef ENABLE_SSE2
#endif

#ifdef _MSC_VER
# define strcasecmp(x, y) _stricmp(x, y)
# define strncasecmp(x, y, l) strnicmp(x, y, l)
# ifndef snprintf
#  define snprintf _snprintf
# endif
#endif

#ifndef MAX_PATH
# ifdef __GNUC__
#  include <climits>
#  ifndef PATH_MAX
#   define MAX_PATH 1024
#  else
#   define MAX_PATH PATH_MAX
#  endif
# endif
#endif

#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
# define DS_ALIGN(X) __declspec(align(X))
#elif defined(__GNUC__)
# define DS_ALIGN(X) __attribute__ ((aligned (X)))
#else
# define DS_ALIGN(X)
#endif

#define CACHE_ALIGN DS_ALIGN(32)

#ifdef __MINGW32__
# undef FASTCALL
# undef LDM_FASTCALL
# define FASTCALL __attribute__((fastcall))
# define LDM_FASTCALL
# define ASMJIT_CALL_CONV kX86FuncConvGccFastCall
# define ASMJIT_STDLIB_CALL_CONV kX86FuncConvCDecl
#elif defined (__i386__) && !defined(__clang__)
# define FASTCALL __attribute__((regparm(3)))
# define LDM_FASTCALL
# define ASMJIT_CALL_CONV kX86FuncConvGccRegParm3
# define ASMJIT_STDLIB_CALL_CONV kX86FuncConvCDecl
#elif defined(_MSC_VER) || defined(__INTEL_COMPILER)
# define FASTCALL
# define LDM_FASTCALL
# ifdef _WIN64
#  define ASMJIT_CALL_CONV kX86FuncConvW64
#  define ASMJIT_STDLIB_CALL_CONV kX86FuncConvW64
# else
#  define ASMJIT_CALL_CONV kX86FuncConvCDecl
#  define ASMJIT_STDLIB_CALL_CONV kX86FuncConvCDecl
# endif
#else
# define FASTCALL
# define LDM_FASTCALL
# if defined(__amd64__) || defined(__x86_64__)
#  if defined(_WIN64)
#   define ASMJIT_CALL_CONV kX86FuncConvW64
#   define ASMJIT_STDLIB_CALL_CONV kX86FuncConvW64
#  else
#   define ASMJIT_CALL_CONV kX86FuncConvU64
#   define ASMJIT_STDLIB_CALL_CONV kX86FuncConvU64
#  endif
# else
#  define ASMJIT_CALL_CONV kX86FuncConvCDecl
#  define ASMJIT_STDLIB_CALL_CONV kX86FuncConvCDecl
# endif
#endif

/*----------------------*/

#ifdef __BIG_ENDIAN__
# ifndef WORDS_BIGENDIAN
#  define WORDS_BIGENDIAN
# endif
#endif

#ifdef WORDS_BIGENDIAN
# define LOCAL_BE
#else
# define LOCAL_LE
#endif

/* little endian (ds' endianess) to local endianess convert macros */
#ifdef LOCAL_BE /* local arch is big endian */
inline uint16_t LE_TO_LOCAL_16(uint16_t x) { return ((x & 0xff) << 8) | ((x >> 8) & 0xff); }
inline uint32_t LE_TO_LOCAL_32(uint32_t x) { return ((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | ((x >> 24) & 0xff); }
inline uint64_t LE_TO_LOCAL_64(uint64_t x)
{
	return ((x & 0xff) << 56) | ((x & 0xff00) << 40) | ((x & 0xff0000) << 24) | ((x & 0xff000000) << 8) |
		((x >> 8) & 0xff000000) | ((x >> 24) & 0xff00) | ((x >> 40) & 0xff00) | ((x >> 56) & 0xff);
}
#else /* local arch is little endian */
inline uint16_t LE_TO_LOCAL_16(uint16_t x) { return x; }
inline uint32_t LE_TO_LOCAL_32(uint32_t x) { return x; }
inline uint64_t LE_TO_LOCAL_64(uint64_t x) { return x; }
#endif

template<typename T, size_t N> inline size_t ARRAY_SIZE(T (&)[N]) { return N; }

inline double u64_to_double(uint64_t u)
{
	union
	{
		uint64_t a;
		double b;
	} fuxor;
	fuxor.a = u;
	return fuxor.b;
}

// fairly standard for loop macros
#define MACRODO1(TRICK, TODO) { int X = TRICK; TODO; }
#define MACRODO2(TRICK, TODO) { MACRODO1((TRICK), TODO) MACRODO1(((TRICK) + 1), TODO) }
#define MACRODO4(TRICK, TODO) { MACRODO2((TRICK), TODO) MACRODO2(((TRICK) + 2), TODO) }

template<typename T> inline void reconstruct(T *t)
{
	t->~T();
	new(t) T();
}
