/* See LICENSE file for copyright and license details. */

/* appearance */
static const unsigned int borderpx  = 1;        /* border pixel of windows */
static int titles = 1;                        /* initial title bar visibility */
static const unsigned int titlebarheight = 30;
static const unsigned int resizegrip = 6;
static const unsigned int gappx     = 6;
static const unsigned int systraypinning = 0;   /* 0 follows focus; otherwise monitor number, starting at 1 */
static const unsigned int systrayspacing = 2;
static const int systraypinningfailfirst = 1;
static const int showsystray = 1;
static const unsigned int snap      = 32;       /* snap pixel */
static const int showbar            = 1;        /* 0 means no bar */
static const int topbar             = 0;        /* 0 means bottom bar */
static const char *fonts[]          = { "monospace:size=14", "FontAwesome:size=14", "Material Icons:size=14" };
static const char dmenufont[]       = "monospace:size=14";
static const char col_gray1[]       = "#222222";
static const char col_gray2[]       = "#444444";
static const char col_gray3[]       = "#bbbbbb";
static const char col_gray4[]       = "#eeeeee";
static const char default_flycolor[] = "#666666";

static const char *flycolors[] = {
	"#666666",
	"#005577",
	"#117755",
	"#aa7711",
	"#771111",
	"#551177",
	"#aa1177",
};

static const char *colors[][3] = {
	/*               fg         bg         border   */
	[SchemeNorm] = { col_gray3, col_gray1, col_gray2 },
	[SchemeSel]  = { col_gray4, default_flycolor, default_flycolor }, /* [1] and [2] are changed in cycle_flycolors */
	[SchemeWarn] = { "#e59b55", col_gray1, col_gray2 },
	[SchemeCritical] = { "#e06c75", col_gray1, col_gray2 },
	[SchemeGood] = { "#98c379", col_gray1, col_gray2 },
	[SchemeMuted] = { "#888888", col_gray1, col_gray2 },
	[SchemeYellow] = { "#e5d567", col_gray1, col_gray2 },
};

static const char *titlecolors[][3] = {
	{ "#cccccc", "#222222", "#444444" },
	{ "#ffffff", "#666666", "#666666" },
};

/* tagging */
static const char *tags[] = { "1", "2", "3", "4", "5" };

static const Rule rules[] = {
	/* xprop(1):
	 *	WM_CLASS(STRING) = instance, class
	 *	WM_NAME(STRING) = title
	 */
	/* class      instance    title       tags mask     isfloating   monitor */
	{ "dolphin",  NULL,       NULL,       0,            1,           -1 },
	{ "mpv",      NULL,       NULL,       0,            1,           -1 },
	{ "Mpv",      NULL,       NULL,       0,            1,           -1 },
	{ "Gimp",     NULL,       NULL,       0,            1,           -1 },
	{ "copyq",    NULL,       NULL,       0,            1,           -1 },
	{ "CopyQ",    NULL,       NULL,       0,            1,           -1 },
	{ "dwm-calendar", NULL,   NULL,       0,            1,           -1 },
	{ "dwm-weather",  NULL,   NULL,       0,            1,           -1 },
	{ "pinentry", NULL,       NULL,       0,            1,           -1 },
};

/* layout(s) */
static const float mfact     = 0.55; /* factor of master area size [0.05..0.95] */
static const int nmaster     = 1;    /* number of clients in master area */
static const int resizehints = 0;    /* 1 means respect size hints in tiled resizals */
static const int lockfullscreen = 0; /* 1 will force focus on the fullscreen window */
static const int refreshrate = 120;  /* refresh rate (per second) for client move/resize */

static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=", tile },       /* first entry is default */
	{ "><>", NULL },       /* no layout function means floating behavior */
	{ "[M]", monocle },
	{ "bsp", bsptile },
};

/* key definitions */
#define MODKEY Mod4Mask
#define TAGKEYS(KEY,TAG) \
{ MODKEY,                       KEY,      view,           {.ui = 1 << TAG} }, \
{ MODKEY|ControlMask,           KEY,      toggleview,     {.ui = 1 << TAG} }, \
{ MODKEY|ShiftMask,             KEY,      tag,            {.ui = 1 << TAG} }, \
{ MODKEY|ControlMask|ShiftMask, KEY,      toggletag,      {.ui = 1 << TAG} },

/* shell commands */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
static char dmenumon[16] = "0"; /* component of dmenucmd, manipulated in spawn() */
static const char *dmenucmd[] = {
	"dmenu_run",
	"-m", dmenumon,
	"-fn", dmenufont,
	"-nb", col_gray1,
	"-nf", col_gray3,
	"-sb", default_flycolor, /* changed as dmenucmd[10] in cycle_flycolors */
	"-sf", col_gray4,
	"-i", "-l", "12",
	"-hp", "st,firefox,code,dolphin,keepassxc,discord,spotify",
	NULL
};
static const char *termcmd[]  = { "st", NULL };

static const Key keys[] = {
	{ MODKEY, XK_Return, spawn, {.v = termcmd} },
	{ Mod1Mask, XK_F1, spawn, {.v = dmenucmd} },
	{ MODKEY, XK_space, spawn, {.v = dmenucmd} },
	{ MODKEY, XK_b, spawn, SHCMD("firefox") },
	{ MODKEY, XK_c, spawn, SHCMD("code") },
	{ MODKEY, XK_x, spawn, SHCMD("discord") },
	{ MODKEY, XK_p, spawn, SHCMD("keepassxc") },
	{ MODKEY|ShiftMask, XK_p, toggletitles, {0} },
	{ MODKEY, XK_z, spawn, SHCMD("spotify") },
	{ MODKEY, XK_f, spawn, SHCMD("dolphin") },
	{ MODKEY, XK_n, spawn, SHCMD("dwm-action network") },
	{ MODKEY|ShiftMask, XK_n, spawn, SHCMD("dwm-action bluetooth") },
	{ MODKEY|ShiftMask, XK_w, spawn, SHCMD("dwm-wallpaper") },
	{ MODKEY, XK_period, spawn, SHCMD("dwm-action emoji") },
	{ MODKEY|ShiftMask, XK_c, centerfloating, {0} },
	{ MODKEY|ShiftMask, XK_r, spawn, SHCMD("rec") },
	{ MODKEY|ShiftMask, XK_y, spawn, SHCMD("dwm-action screenshot") },
	{ MODKEY, XK_r, spawn, SHCMD("rec") },
	{ MODKEY, XK_l, spawn, SHCMD("dwm-action lock") },
	{ MODKEY, XK_v, spawn, SHCMD("dwm-action clipboard") },
	{ ControlMask|Mod1Mask, XK_Delete, quit, {0} },
	{ MODKEY|ShiftMask, XK_x, quit, {0} },
	{ MODKEY|ShiftMask, XK_q, killclient, {0} },
	{ MODKEY|ShiftMask, XK_equal, grouptabs, {0} },
	{ MODKEY|ShiftMask, XK_minus, ungrouptab, {0} },
	{ MODKEY, XK_equal, resizefloating, {.i = +50} },
	{ MODKEY, XK_minus, resizefloating, {.i = -50} },
	{ MODKEY, XK_plus, resizefloating, {.i = +50} },
	{ MODKEY, XK_KP_Add, resizefloating, {.i = +50} },
	{ MODKEY, XK_KP_Subtract, resizefloating, {.i = -50} },
	{ MODKEY|Mod1Mask, XK_h, adjustfloating, {.v = (int[]){-50, 0, 0, 0}} },
	{ MODKEY|Mod1Mask, XK_j, adjustfloating, {.v = (int[]){0, +50, 0, 0}} },
	{ MODKEY|Mod1Mask, XK_k, adjustfloating, {.v = (int[]){0, -50, 0, 0}} },
	{ MODKEY|Mod1Mask, XK_l, adjustfloating, {.v = (int[]){+50, 0, 0, 0}} },
	{ MODKEY|Mod1Mask|ShiftMask, XK_h, adjustfloating, {.v = (int[]){0, 0, -50, 0}} },
	{ MODKEY|Mod1Mask|ShiftMask, XK_j, adjustfloating, {.v = (int[]){0, 0, 0, +50}} },
	{ MODKEY|Mod1Mask|ShiftMask, XK_k, adjustfloating, {.v = (int[]){0, 0, 0, -50}} },
	{ MODKEY|Mod1Mask|ShiftMask, XK_l, adjustfloating, {.v = (int[]){0, 0, +50, 0}} },
	{ MODKEY|Mod1Mask, XK_c, centerfloating, {0} },
	{ Mod1Mask, XK_Tab, alttab, {.i = 1} },
	{ Mod1Mask|ShiftMask, XK_Tab, alttab, {.i = -1} },
	{ Mod1Mask, XK_grave, alttab, {.i = 2} },
	{ Mod1Mask|ShiftMask, XK_grave, alttab, {.i = -2} },
	{ MODKEY, XK_w, alttab, {.i = 4} },
	{ ControlMask, XK_F9, alttab, {.i = 3} },
	{ ControlMask, XK_F10, alttab, {.i = 4} },
	{ MODKEY, XK_Left, snapwindow, {.i = 1} },
	{ MODKEY|Mod1Mask, XK_Left, focusdirection, {.i = 1} },
	{ MODKEY, XK_Right, snapwindow, {.i = 2} },
	{ MODKEY|Mod1Mask, XK_Right, focusdirection, {.i = 2} },
	{ MODKEY, XK_Up, snapwindow, {.i = 3} },
	{ MODKEY|Mod1Mask, XK_Up, focusdirection, {.i = 3} },
	{ MODKEY, XK_Down, snapwindow, {.i = 4} },
	{ MODKEY|Mod1Mask, XK_Down, focusdirection, {.i = 4} },
	{ MODKEY|Mod1Mask|ShiftMask, XK_Left, tagmon, {.i = -1} },
	{ MODKEY|ControlMask|ShiftMask, XK_Left, movedesktop, {.i = -1} },
	{ MODKEY|Mod1Mask|ShiftMask, XK_Right, tagmon, {.i = 1} },
	{ MODKEY|ControlMask|ShiftMask, XK_Right, movedesktop, {.i = 1} },
	{ MODKEY, XK_Prior, snapwindow, {.i = 5} },
	{ MODKEY, XK_Next, minimize, {0} },
	{ MODKEY|ShiftMask, XK_Next, restore, {0} },
	{ MODKEY, XK_d, showdesktop, {0} },
	{ ControlMask, XK_F12, showdesktop, {0} },
	TAGKEYS(XK_1, 0)
	TAGKEYS(XK_2, 1)
	TAGKEYS(XK_3, 2)
	TAGKEYS(XK_4, 3)
	TAGKEYS(XK_5, 4)
	{ 0, XF86XK_AudioRaiseVolume, spawn, SHCMD("dwm-action volume-up") },
	{ 0, XF86XK_AudioLowerVolume, spawn, SHCMD("dwm-action volume-down") },
	{ 0, XF86XK_AudioMute, spawn, SHCMD("dwm-action mute") },
	{ 0, XF86XK_AudioMicMute, spawn, SHCMD("dwm-action mic-mute") },
	{ 0, XF86XK_MonBrightnessUp, spawn, SHCMD("dwm-action brightness-up") },
	{ 0, XF86XK_MonBrightnessDown, spawn, SHCMD("dwm-action brightness-down") },
	{ 0, XF86XK_AudioPlay, spawn, SHCMD("dwm-action PlayPause") },
	{ 0, XF86XK_AudioPause, spawn, SHCMD("dwm-action Pause") },
	{ 0, XF86XK_AudioNext, spawn, SHCMD("dwm-action Next") },
	{ 0, XF86XK_AudioPrev, spawn, SHCMD("dwm-action Previous") },
	{ 0, XF86XK_AudioStop, spawn, SHCMD("dwm-action Stop") },
	{ 0, XF86XK_ScreenSaver, spawn, SHCMD("dwm-action lock") },
	{ ShiftMask, XF86XK_AudioRaiseVolume, spawn, SHCMD("dwm-action volume-up-small") },
	{ ShiftMask, XF86XK_AudioLowerVolume, spawn, SHCMD("dwm-action volume-down-small") },
	{ ShiftMask, XF86XK_MonBrightnessUp, spawn, SHCMD("dwm-action brightness-up-small") },
	{ ShiftMask, XF86XK_MonBrightnessDown, spawn, SHCMD("dwm-action brightness-down-small") },
	{ MODKEY, XF86XK_AudioMute, spawn, SHCMD("dwm-action mic-mute") },
	{ MODKEY|ControlMask, XK_j, focusstack, {.i = 1} },
	{ MODKEY|ControlMask, XK_k, focusstack, {.i = -1} },
	{ MODKEY|ControlMask, XK_i, incnmaster, {0} },
	{ MODKEY|ControlMask, XK_h, setmfact, {.f = -0.05} },
	{ MODKEY|ControlMask, XK_l, setmfact, {.f = +0.05} },
	{ MODKEY|ControlMask, XK_Return, zoom, {0} },
	{ MODKEY|ControlMask, XK_b, togglebar, {0} },
	{ MODKEY|ControlMask, XK_minus, setgaps, {.i = -1} },
	{ MODKEY|ControlMask, XK_equal, setgaps, {.i = +1} },
	{ MODKEY|ControlMask|ShiftMask, XK_equal, setgaps, {.i = 0} },
	{ MODKEY|ControlMask, XK_t, setlayout, {.v = &layouts[0]} },
	{ MODKEY|ControlMask, XK_s, setlayout, {.v = &layouts[3]} },
	{ MODKEY|ControlMask, XK_f, setlayout, {.v = &layouts[1]} },
	{ MODKEY|ControlMask, XK_m, setlayout, {.v = &layouts[2]} },
	{ MODKEY|ControlMask, XK_c, cycle_flycolors, {.i = 1} },
	{ MODKEY|ShiftMask, XK_t, setmode, {.i = 0} },
	{ MODKEY|ShiftMask, XK_f, setmode, {.i = 1} },
	{ MODKEY|ControlMask, XK_space, togglefloating, {0} },
	{ MODKEY|ControlMask, XK_F11, fullscreen, {0} },
	{ MODKEY|ControlMask, XK_comma, focusmon, {.i = -1} },
	{ MODKEY|ControlMask, XK_period, focusmon, {.i = 1} },
	{ MODKEY|ControlMask|ShiftMask, XK_c, cycle_flycolors, {.i = -1} },
};

/* button definitions */
/* click can be ClkTagBar, ClkLtSymbol, ClkStatusText, ClkWifi, ClkBluetooth, ClkClock, ClkWinTitle, ClkClientWin, or ClkRootWin */
static const Button buttons[] = {
	/* click                event mask      button          function        argument */
	{ ClkLtSymbol,          0,              Button1,        setlayout,      {0} },
	{ ClkLtSymbol,          0,              Button3,        setlayout,      {.v = &layouts[2]} },
	{ ClkWinTitle,          0,              Button2,        zoom,           {0} },
	{ ClkStatusText,        0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkClock,             0,              Button1,        spawn,          SHCMD("dwm-calendar") },
	{ ClkClock,             0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkWifi,              0,              Button1,        spawn,          SHCMD("dwm-action network") },
	{ ClkWifi,              0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkBluetooth,         0,              Button1,        spawn,          SHCMD("dwm-action bluetooth") },
	{ ClkBluetooth,         0,              Button2,        spawn,          {.v = termcmd } },
	{ ClkWeather,           0,              Button1,        spawn,          SHCMD("dwm-weather --forecast") },
	{ ClkClientWin,         MODKEY,         Button1,        movemouse,      {0} },
	{ ClkClientWin,         MODKEY,         Button2,        togglefloating, {0} },
	{ ClkClientWin,         MODKEY,         Button3,        resizemouse,    {0} },
	{ ClkClientWin,         Mod1Mask,       Button1,        resizemouse,    {0} },
	{ ClkTagBar,            0,              Button1,        view,           {0} },
	{ ClkTagBar,            0,              Button3,        toggleview,     {0} },
	{ ClkTagBar,            MODKEY,         Button1,        tag,            {0} },
	{ ClkTagBar,            MODKEY,         Button3,        toggletag,      {0} },
};
