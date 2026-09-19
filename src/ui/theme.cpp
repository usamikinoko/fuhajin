#include "theme.h"
#include "../app/settings.h"

static const Theme THEMES[THEME_N] = {
    { RGB(16, 17, 20), RGB(104, 110, 126), RGB(150, 156, 170), RGB(238, 241, 246),
      RGB(104, 214, 132), RGB(246, 199, 72), RGB(248, 104, 96) },
    { RGB(246, 247, 250), RGB(196, 201, 212), RGB(118, 126, 140), RGB(24, 28, 36),
      RGB(22, 142, 74), RGB(190, 116, 0), RGB(206, 48, 42) }
};

const Theme& ThemeGet() { return THEMES[g_set.theme]; }
