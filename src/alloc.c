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

// Code taken from https://nullprogram.com/blog/2023/09/27/
#include "alloc.h"

#include <string.h>

void *alloc(arena_t *a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count, int8_t flags) {
  ptrdiff_t padding = -(uintptr_t)a->beg & (align - 1);
  ptrdiff_t available = a->end - a->beg - padding;
  if (available < 0 || count > available / size) {
    return nullptr;
  }
  void *p = a->beg + padding;
  a->beg += padding + count * size;
  return flags & NOZERO ? p : memset(p, 0, count * size);
}
