#pragma once
#include <windows.h>

enum MenuAction {
    MENU_NONE,     // 点了空白处，什么都没变
    MENU_REPAINT,  // 配置变了：重画 + 存盘
    MENU_REBUILD,  // 字号变了：连绘图资源一起重建
    MENU_EXIT
};

// 弹右键菜单，就地改 g_set，并返回调用方需要跟进的动作。
MenuAction MenuShow(HWND hwnd);
