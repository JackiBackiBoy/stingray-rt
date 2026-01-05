#pragma once
#include <stdint.h>

#define internal      static
#define global        static
#define local_persist static

#define Kilobytes(value) ((value) * 1024ULL)
#define Megabytes(value) (Kilobytes(value) * 1024ULL)
#define Gigabytes(value) (Megabytes(value) * 1024ULL)
#define Terabytes(value) (Gigabytes(value) * 1024ULL)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

typedef int8_t   b8;
typedef int16_t  b16;
typedef int32_t  b32;
typedef int64_t  b64;
