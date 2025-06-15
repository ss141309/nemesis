/*
Copyright 2025 समीर सिंह Sameer Singh

This file is part of nemesis.

nemesis is free software: you can redistribute it and/or modify it under the terms of the GNU
General Public License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version.

nemesis is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
Public License for more details.

You should have received a copy of the GNU General Public License along with nemesis. If not, see
<https://www.gnu.org/licenses/>. */

#pragma once

#include <stdint.h>
#include <string.h>

#define private static inline

#define ERR_ARITHMETIC_OVERFLOW "An arithematic operation caused an overflow (result > MAX)"
#define ERR_NULL_FILEPATH "File path cannot be null"
#define ERR_INVALID_FILE_SIZE "File size cannot be negative"
#define ERR_ROM_FILE_TOO_LARGE "Given file is too large (maximum supported ROM size is 5MiB)"
#define ERR_ROM_TYPE_NOT_SUPPORTED \
  "This ROM format is not supported (currently supported types are iNES and NES2.0)"
#define ERR_INVALID_MISC_ROM_AREA_SIZE "Miscellaneous ROM size cannot be negative"

#define ANSI_RESET "\x1b[0m"
#define ANSI_RED "\x1b[31m"
#define ANSI_YELLOW "\x1b[33m"
#define ANSI_BLUE "\x1b[34m"

#define log_base(stream, color, level, ...)                              \
  do {                                                                   \
    char func_line[64];                                                  \
    snprintf(func_line, sizeof(func_line), "%s:%d", __func__, __LINE__); \
    fprintf(stream, "%s[%s]%s ", color, level, ANSI_RESET);              \
    fputs("asmsen: ", stream);                                           \
    fprintf(stream, "[%s] ", __FILE_NAME__);                             \
    fprintf(stream, "%-40s", func_line);                                 \
    fprintf(stream, __VA_ARGS__);                                        \
    fputc('\n', stream);                                                 \
  } while (0)

#define log_info(...) log_base(stdout, ANSI_BLUE, "INFO", __VA_ARGS__)
#define log_warn(...) log_base(stderr, ANSI_YELLOW, "WARN", __VA_ARGS__)
#define log_error(...) log_base(stderr, ANSI_RED, "ERROR", __VA_ARGS__)

#define return_value_if(cond, value, ...) \
  do {                                    \
    if ((cond)) {                         \
      log_error(__VA_ARGS__);             \
      return (value);                     \
    }                                     \
  } while (0)

static inline uint8_t get_0th_bit(uint8_t val) { return val & 0x01; }
static inline uint8_t get_1st_bit(uint8_t val) { return val & 0x02; }
static inline uint8_t get_2nd_bit(uint8_t val) { return val & 0x04; }
static inline uint8_t get_3rd_bit(uint8_t val) { return val & 0x08; }
static inline uint8_t get_lower_2_bits(uint8_t val) { return val & 0x03; }
static inline uint8_t get_lower_4_bits(uint8_t val) { return val & 0x0F; }
static inline uint8_t get_lower_6_bits(uint8_t val) { return val & 0x3F; }
static inline uint8_t get_upper_4_bits(uint8_t val) { return val >> 4; }
static inline uint8_t get_upper_6_bits(uint8_t val) { return val >> 2; }

static inline const char* get_filename_from_path(const char* path) {
  const char* filename = strrchr(path, '/');

  if (filename == nullptr) {
    return path;
  }

  filename++;

  if (*filename == '\0') {
    return nullptr;
  }

  return filename;
}
