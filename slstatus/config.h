/* See LICENSE file for copyright and license details. */

/* interval between updates (in ms) */
const unsigned int interval = 2000;

/* text to show if no value can be retrieved */
static const char unknown_str[] = "n/a";

/* maximum output string length */
#define MAXLEN 2048

/*
 * Bar format (left to right):
 *   CPU%  RAM%  VOL%  WIFI  BAT%[state]  DATE TIME
 *
 * Adjust the wifi interface name (wlan0) or battery name (BAT0) if yours differ.
 */
static const struct arg args[] = {
	/* function        format            argument */
	{ cpu_perc,        "CPU %s%%  ",     NULL },
	{ ram_perc,        "RAM %s%%  ",     NULL },
	{ run_command,     "VOL %s%%  ",     "pactl get-sink-volume @DEFAULT_SINK@ | awk '/Volume:/{gsub(/%/,\"\"); print $5; exit}'" },
	{ wifi_essid,      "NET %s  ",       "wlan0" },
	{ battery_perc,    "BAT %s%%",       "BAT0" },
	{ battery_state,   "%s  ",           "BAT0" },
	{ datetime,        "%s",             "%a %d %b  %H:%M" },
};
