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
#pragma once

#include <stdalign.h>
#include <stddef.h>
#include <stdint.h>

#define new(...) newx(__VA_ARGS__, new4, new3, new2)(__VA_ARGS__)
#define newx(a, b, c, d, e, ...) e
#define new2(a, t) (t *)alloc(a, sizeof(t), alignof(t), 1, 0)
#define new3(a, t, n) (t *)alloc(a, sizeof(t), alignof(t), n, 0)
#define new4(a, t, n, f) (t *)alloc(a, sizeof(t), alignof(t), n, f)

#define NOZERO 1

typedef struct {
  char *beg;
  char *end;
} arena_t;

void *alloc(arena_t *a, ptrdiff_t size, ptrdiff_t align, ptrdiff_t count, int8_t flags);
