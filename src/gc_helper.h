#pragma once

#include <gc.h>
#define New(type)	((type*)GC_MALLOC(sizeof(type)))
#define NewAtom(type)	((type*)GC_MALLOC_ATOMIC(sizeof(type)))
#define New_N(type,n)	((type*)GC_MALLOC((n)*sizeof(type)))
#define NewAtom_N(type,n)	((type*)GC_MALLOC_ATOMIC((n)*sizeof(type)))
#define New_Reuse(type,ptr,n)   ((type*)GC_REALLOC((ptr),(n)*sizeof(type)))
