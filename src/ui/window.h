#pragma once
#include <windows.h>

enum { TIMER_SAMPLE = 1 };

bool WindowRegister(HINSTANCE instance);
HWND WindowCreate(HINSTANCE instance);
