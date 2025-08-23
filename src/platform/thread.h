#pragma once


void thread_create();

void mutex_create();

int atomic_add(int* ptr, int value);
i64 atomic_add64(i64* ptr, i64 value);