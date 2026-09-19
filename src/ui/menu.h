#pragma once
#include <windows.h>

enum MenuAction {
    MENU_NONE,
    MENU_REPAINT,
    MENU_REBUILD,
    MENU_EXIT
};

MenuAction MenuShow(HWND hwnd);
