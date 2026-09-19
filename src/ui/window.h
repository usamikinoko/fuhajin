#pragma once
#include <windows.h>

// 定时器 ID：菜单改刷新间隔时也要用
enum { TIMER_SAMPLE = 1 };

bool WindowRegister(HINSTANCE instance);
HWND WindowCreate(HINSTANCE instance);
