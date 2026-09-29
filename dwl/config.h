/* Taken from https://github.com/djpohly/dwl/issues/466 */
#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }
/* appearance */
static const int sloppyfocus               = 1;  /* focus follows mouse */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const unsigned int borderpx         = 1;  /* border pixel of windows */
static const float rootcolor[]             = COLOR(0x222222ff);
static const float bordercolor[]           = COLOR(0x444444ff);
static const float focuscolor[]            = COLOR(0x005577ff);
static const float urgentcolor[]           = COLOR(0xff0000ff);
/* This conforms to the xdg-protocol. Set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]         = {0.0f, 0.0f, 0.0f, 1.0f}; /* You can also use glsl colors */

/* tagging - TAGCOUNT must be no greater than 31 */
#define TAGCOUNT (9)

/* logging */
static int log_level = WLR_ERROR;

static const Rule rules[] = {
	/* app_id             title       tags mask     isfloating   skipfocus  monitor */
	/* skipfocus is a plain boolean here: 1 means the client can never hold the
	 * keyboard focus, by any route (map, sloppyfocus hover, focusstack). */
	{ "Gimp_EXAMPLE",     NULL,       0,            1,           0,         -1 }, /* Start on currently visible tags floating, not tiled */
	{ "firefox_EXAMPLE",  NULL,       1 << 8,       0,           0,         -1 }, /* Start on ONLY tag "9" */

	/* ibus candidate/property popups (Japanese conversion list, etc).
	 *
	 * This one is load-bearing, not cosmetic. ibus-ui-gtk3 draws the candidate
	 * window as an ordinary xdg_toplevel -- it has no other option, since this
	 * dwl has no text-input-v3 and the IME reaches apps through the GTK/Qt IM
	 * modules over D-Bus, entirely behind the compositor's back. So dwl sees a
	 * normal window: it tiles it, and it focuses it. Focusing it takes the
	 * keyboard away from the field you are composing in, ibus drops the
	 * preedit and hides the popup, focus falls back, the popup reopens --
	 * こんにちは flickers once and dies. sloppyfocus=1 makes it worse, because
	 * simply hovering the popup re-triggers it.
	 *
	 * isfloating keeps it out of the tiling layout; skipfocus is the actual
	 * fix. app_id is "ibus-ui-gtk3" because GTK3 on Wayland sets app_id from
	 * g_get_prgname(), i.e. the basename of the executable (verified with
	 * WAYLAND_DEBUG=1). The match is strstr, so it also covers any other
	 * toplevel that panel puts up.
	 *
	 * Known cosmetic limit: dwl has no way to place the popup under the caret,
	 * because the caret position travels over ibus's D-Bus channel, not over
	 * Wayland. The candidate list lands where a floating window lands. Fixing
	 * that properly needs text-input-v3 in dwl. */
	{ "ibus-ui-gtk3",     NULL,       0,            1,           1,         -1 },

	/* Tor Browser floats so it keeps its own default window size: letterboxing
	 * picks a common size to blend in, and tiling would stretch it to a
	 * fingerprintable one. app_id is "Tor Browser Alpha" (WAYLAND_DEBUG=1);
	 * strstr matching also covers a future stable "Tor Browser". */
	{ "Tor Browser",      NULL,       0,            1,           0,         -1 },

    /* default/example rule: can be changed but cannot be eliminated; at least one rule must exist */
};

/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "[]=",      tile },
	{ "><>",      NULL },    /* no layout function means floating behavior */
	{ "[M]",      monocle },
};

/* monitors */
/* (x=-1, y=-1) is reserved as an "autoconfigure" monitor position indicator
 * WARNING: negative values other than (-1, -1) cause problems with Xwayland clients due to
 * https://gitlab.freedesktop.org/xorg/xserver/-/issues/899 */
static const MonitorRule monrules[] = {
   /* name        mfact  nmaster scale layout       rotate/reflect                x    y
    * example of a HiDPI laptop monitor:
    { "eDP-1",    0.5f,  1,      2,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 }, */
	{ NULL,       0.55f, 1,      1.5,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 },
	/* default monitor rule: can be changed but cannot be eliminated; at least one monitor rule must exist */
};

/* keyboard */
/* Layout groups, cycled with Shift+CapsLock. Order matters: the first entry is
 * the default at login. CJK is NOT here -- Chinese and Japanese go through ibus
 * (started in ~/.config/dwl/startup.sh), because they need composition/candidate
 * popups that a plain xkb layout cannot express.
 *   us -> English      es -> Spanish
 *   pt -> Portuguese   ru -> Russian, phonetic (Cyrillic on the matching QWERTY
 *                            keys: s->с, d->д -- NOT the ЙЦУКЕН typist layout)
 *
 * .variant is positional and must line up with .layout entry-for-entry, so the
 * three leading commas mean "default variant" for us/es/pt.
 *
 * The current group is shown in waybar: dwl's kblayout() sees the group change
 * land as a modifiers event and calls `dwl-language report`, which owns the
 * indicator and the toast. Japanese/Chinese ride on top via ibus (Super+space).
 *
 * Do NOT use any grp: option built on a modifier key (grp:alt_shift_toggle,
 * grp:lalt_toggle, grp:lctrl_lshift_toggle, ...). Those redefine the modifier
 * itself -- alt_shift_toggle expands to
 *     key <LALT> {[ NoSymbol, ISO_Next_Group ], type[group1]="TWO_LEVEL" };
 * so with Shift held, Alt stops being a modifier. MODKEY is WLR_MODIFIER_ALT
 * below, so that silently kills every MODKEY|SHIFT binding (killclient, quit,
 * tag, tagmon, screenshots, nightlight) and scrambles the modifier map.
 * grp:shift_caps_toggle touches only <CAPS>, and its level-1 "any" keeps plain
 * CapsLock working as CapsLock.
 *
 * terminate:ctrl_alt_bksp must stay: the quit binding further down uses
 * XKB_KEY_Terminate_Server, which only exists when that option is set. */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	.layout = "us,es,pt,ru",
	.variant = ",,,phonetic",
	.options = "grp:shift_caps_toggle,terminate:ctrl_alt_bksp",
};

static const int repeat_rate = 25;
static const int repeat_delay = 600;

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 0;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;

/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_ALT

#define TAGKEYS(KEY,SKEY,TAG) \
	{ MODKEY,                    KEY,            view,            {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL,  KEY,            toggleview,      {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_SHIFT, SKEY,           tag,             {.ui = 1 << TAG} }, \
	{ MODKEY|WLR_MODIFIER_CTRL|WLR_MODIFIER_SHIFT,SKEY,toggletag, {.ui = 1 << TAG} }

/* helper for spawning shell commands in the pre dwm-5.0 fashion */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

/* commands */
/* footclient, not foot: startup.sh runs `foot --server`, so every new terminal
 * is a window on that one process instead of a fresh fork+exec. */
static const char *termcmd[] = { "footclient", NULL };
static const char *menucmd[] = { "fuzzel", NULL };
/* Alt+w, not Super+w: keyd's [meta] block remaps meta+w -> Ctrl+w for macOS-style
 * Cmd+W, so a Super+w binding here would never see an event. See
 * /etc/keyd/default.conf. */
static const char *browsercmd[] = { "firefox", NULL };

static const Key keys[] = {
	/* Note that Shift changes certain key codes: 2 -> at, etc. */
	/* modifier                  key                  function          argument */
	{ MODKEY,                    XKB_KEY_p,           spawn,            {.v = menucmd} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Return,      spawn,            {.v = termcmd} },
	{ MODKEY,                    XKB_KEY_w,           spawn,            {.v = browsercmd} },
	{ MODKEY,                    XKB_KEY_j,           focusstack,       {.i = +1} },
	{ MODKEY,                    XKB_KEY_k,           focusstack,       {.i = -1} },
	{ MODKEY,                    XKB_KEY_i,           incnmaster,       {.i = +1} },
	{ MODKEY,                    XKB_KEY_d,           incnmaster,       {.i = -1} },
	{ MODKEY,                    XKB_KEY_h,           setmfact,         {.f = -0.05f} },
	{ MODKEY,                    XKB_KEY_l,           setmfact,         {.f = +0.05f} },
	{ MODKEY,                    XKB_KEY_Return,      zoom,             {0} },
	{ MODKEY,                    XKB_KEY_Tab,         view,             {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_c,           killclient,       {0} },
	{ MODKEY,                    XKB_KEY_t,           setlayout,        {.v = &layouts[0]} },
	{ MODKEY,                    XKB_KEY_f,           setlayout,        {.v = &layouts[1]} },
	{ MODKEY,                    XKB_KEY_m,           setlayout,        {.v = &layouts[2]} },
	{ MODKEY,                    XKB_KEY_space,       setlayout,        {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_space,       togglefloating,   {0} },
	{ MODKEY,                    XKB_KEY_e,           togglefullscreen, {0} },
	{ MODKEY,                    XKB_KEY_0,           view,             {.ui = ~0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenright,  tag,              {.ui = ~0} },
	{ MODKEY,                    XKB_KEY_comma,       focusmon,         {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY,                    XKB_KEY_period,      focusmon,         {.i = WLR_DIRECTION_RIGHT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_less,        tagmon,           {.i = WLR_DIRECTION_LEFT} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_greater,     tagmon,           {.i = WLR_DIRECTION_RIGHT} },
	TAGKEYS(          XKB_KEY_1, XKB_KEY_exclam,                        0),
	TAGKEYS(          XKB_KEY_2, XKB_KEY_at,                            1),
	TAGKEYS(          XKB_KEY_3, XKB_KEY_numbersign,                    2),
	TAGKEYS(          XKB_KEY_4, XKB_KEY_dollar,                        3),
	TAGKEYS(          XKB_KEY_5, XKB_KEY_percent,                       4),
	TAGKEYS(          XKB_KEY_6, XKB_KEY_asciicircum,                   5),
	TAGKEYS(          XKB_KEY_7, XKB_KEY_ampersand,                     6),
	TAGKEYS(          XKB_KEY_8, XKB_KEY_asterisk,                      7),
	TAGKEYS(          XKB_KEY_9, XKB_KEY_parenleft,                     8),
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_q,           quit,             {0} },

	/* Apple function-row media keys — approximates KDE powerdevil + plasma-pa.
	 * Helper scripts live in ~/.local/bin and pop a lightweight mako toast. */
	{ 0, XKB_KEY_XF86MonBrightnessUp,   spawn, SHCMD("$HOME/.local/bin/dwl-brightness up") },
	{ 0, XKB_KEY_XF86MonBrightnessDown, spawn, SHCMD("$HOME/.local/bin/dwl-brightness down") },
	{ 0, XKB_KEY_XF86KbdBrightnessUp,   spawn, SHCMD("$HOME/.local/bin/dwl-brightness kbd-up") },
	{ 0, XKB_KEY_XF86KbdBrightnessDown, spawn, SHCMD("$HOME/.local/bin/dwl-brightness kbd-down") },
	{ 0, XKB_KEY_XF86AudioRaiseVolume,  spawn, SHCMD("$HOME/.local/bin/dwl-volume up") },
	{ 0, XKB_KEY_XF86AudioLowerVolume,  spawn, SHCMD("$HOME/.local/bin/dwl-volume down") },
	{ 0, XKB_KEY_XF86AudioMute,         spawn, SHCMD("$HOME/.local/bin/dwl-volume mute") },
	{ 0, XKB_KEY_XF86AudioMicMute,      spawn, SHCMD("wpctl set-mute @DEFAULT_AUDIO_SOURCE@ toggle") },
	{ 0, XKB_KEY_XF86AudioPlay,         spawn, SHCMD("playerctl play-pause") },
	{ 0, XKB_KEY_XF86AudioPause,        spawn, SHCMD("playerctl play-pause") },
	{ 0, XKB_KEY_XF86AudioNext,         spawn, SHCMD("playerctl next") },
	{ 0, XKB_KEY_XF86AudioPrev,         spawn, SHCMD("playerctl previous") },

	/* Screenshots — macOS-style copy-to-clipboard (and save a file). Pops a mako
	 * toast on capture. region = drag-select (Cmd+Shift+4), full = whole screen. */
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_s, spawn, SHCMD("$HOME/.local/bin/dwl-screenshot region") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_f, spawn, SHCMD("$HOME/.local/bin/dwl-screenshot full") },

	/* Night light ("redshift") — translucent amber overlay, since apple-drm has
	 * no gamma LUT (see ~/README.md sec 4). Alt+n toggle, Shift=warmer, Ctrl=cooler. */
	{ MODKEY,                    XKB_KEY_n, spawn, SHCMD("$HOME/.local/bin/dwl-nightlight toggle") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_n, spawn, SHCMD("$HOME/.local/bin/dwl-nightlight up") },
	{ MODKEY|WLR_MODIFIER_CTRL,  XKB_KEY_n, spawn, SHCMD("$HOME/.local/bin/dwl-nightlight down") },

	/* Toggle the top bar. SIGUSR1 is waybar's built-in show/hide; SIGUSR2 is
	 * reload and the custom modules use SIGRTMIN+8/9/10, so nothing collides. */
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_b, spawn, SHCMD("pkill -SIGUSR1 waybar") },

	/* Mute the trackpad (keyboard-only mode). libinput stops sending events at
	 * the source, so nothing moves the cursor and sloppyfocus can't steal focus
	 * on a stray palm. The keyboard and any external mouse are unaffected.
	 * Alt+m is the monocle layout; only the Shift variant is the trackpad. */
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_m, togglepointer, {0} },

	/* Cycle the input method (English -> 日本語 -> 中文, whatever ibus's
	 * preload-engines holds). This is the *other* language switch: Shift+CapsLock
	 * cycles the plain xkb layouts above, which can only ever be a 1:1 key->glyph
	 * map, while Japanese and Chinese need a candidate popup and therefore an IME.
	 * ibus's own <Super>space trigger is disabled (gsettings
	 * org.freedesktop.ibus.general.hotkey triggers) so the switch has exactly one
	 * owner and the bar indicator can never drift out of sync.
	 * Super, not Alt: keyd only rewrites meta+<letter>, so meta+space arrives
	 * here untouched -- and Alt+space is already setlayout.
	 * Reminder: with no text-input-v3 in this build the IME reaches GTK and Qt
	 * apps only, never native-Wayland Firefox. See startup.sh. */
	{ WLR_MODIFIER_LOGO, XKB_KEY_space, spawn, SHCMD("$HOME/.local/bin/dwl-language ime next") },

	/* Lock on demand. Same gtklock entry point swayidle uses for the 3 min idle
	 * timeout and before-sleep, so manual and automatic locks look identical. */
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_l, spawn, SHCMD("$HOME/.local/bin/dwl-lock") },

	/* Ctrl-Alt-Backspace and Ctrl-Alt-Fx used to be handled by X server */
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_Terminate_Server, quit, {0} },
	/* Ctrl-Alt-Fx is used to switch to another VT, if you don't know what a VT is
	 * do not remove them.
	 */
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT,XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ MODKEY, BTN_LEFT,   moveresize,     {.ui = CurMove} },
	{ MODKEY, BTN_MIDDLE, togglefloating, {0} },
	{ MODKEY, BTN_RIGHT,  moveresize,     {.ui = CurResize} },
};
