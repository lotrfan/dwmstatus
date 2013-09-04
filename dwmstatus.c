/* made by profil 2011-12-29.
 **
 ** Compile with:
 ** gcc -Wall -pedantic -std=c99 -lX11 status.c
 */

// TODO: switch to using *n functions for strings (that take the number of char's as arg)

#define _POSIX_SOURCE
#include <stdio.h>
#define __USE_BSD 1 // for getloadavg
#include <stdlib.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <strings.h>
#define __USE_BSD 1 // for strdup
#include <string.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>

#include <sys/utsname.h>

#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/un.h>
#include <linux/wireless.h>
#include <linux/if.h>
#include <iwlib.h>

#include <mpd/client.h>

#define _GNU_SOURCE
#include <pulse/pulseaudio.h>

#define WIRELESS_DEV "wlp2s0"
#define WIRED_DEV "enp10s0"
#define BONDED_DEV "bond0"
#define DROPBOX_SOCKET "/home/jeffrey/.dropbox/command_socket" /* Set to NULL to "remove" dropbox reporting */

#define COLOR_NORMAL 0
#define COLOR_CRITICAL 5
#define COLOR_WARNING 4


#define FG  "3"
#define BG  "4"
#define COL_DEF_FG(b)  "\x1b[" b "8;5;245m"
#define COL_DEF_BG(b)  "\x1b[" b "8;5;232m"

#define COL_WARNING_FG(b) "\x1b[1;" b "3m"
#define COL_WARNING_BG(b) COL_DEF_BG(b)
#define COL_WARNING COL_WARNING_FG(FG) COL_WARNING_BG(BG)

/*#define COL_CRITICAL_FG(b) "\x1b[" b "8;5;255m"*/
#define COL_CRITICAL_FG(b) "\x1b[1;" b "1m"
#define COL_CRITICAL_BG(b) COL_DEF_BG(b)
#define COL_CRITICAL COL_CRITICAL_FG(FG) COL_CRITICAL_BG(BG)

#define COL_NORMAL_FG(b) "\x1b[0;" b "7m"
/*#define COL_NORMAL_BG(b) COL_DEF_FG(b)*/
#define COL_NORMAL_BG(b) COL_DEF_BG(b)
#define COL_NORMAL COL_NORMAL_FG(FG) COL_NORMAL_BG(BG)

#define COL_DESC_FG(b) "\x1b[" b "8;5;243m"
#define COL_DESC_BG(b) COL_DEF_BG(b)
#define COL_DESC COL_DESC_FG(FG) COL_DESC_BG(BG)

#define COL_UNIT_FG(b) "\x1b[" b "8;5;39m"
#define COL_UNIT_BG(b) COL_DEF_BG(b)
#define COL_UNIT COL_UNIT_FG(FG) COL_UNIT_BG(BG)

#define COL_SEP_FG(b) "\x1b[" b "8;5;48m"
#define COL_SEP_BG(b) COL_DEF_BG(b)
#define COL_SEP COL_SEP_FG(FG) COL_SEP_BG(BG)

#define COL_IP_FG(b) "\x1b[" b "8;5;103m"
#define COL_IP_BG(b) COL_DEF_BG(b)
#define COL_IP COL_IP_FG(FG) COL_IP_BG(BG)

/*#define COL_RESET "\x1b[0m"*/
#define COL_RESET COL_DEF_FG(FG) COL_DEF_BG(BG)

#define START(status) add_start(status, COL_NORMAL_BG(BG), COL_NORMAL);
#define SEP(status) add_sep(status);
#define END(status) add_end(status);
#define START_WARN(status) add_start(status, COL_WARNING_BG(BG), COL_WARNING);
#define START_CRIT(status) add_start(status, COL_CRITICAL_BG(BG), COL_CRITICAL);

#define _BSTART "\x1b[38;5;49m:" COL_NORMAL
#define _BEND   "\x1b[38;5;49m; " COL_NORMAL
#define _BSEP " "
#define BSTART _BSTART
#define BSEP _BSEP
#define BEND _BEND
/*#define BSTART _BEND*/
/*#define BSEP _BSEP*/
/*#define BEND _BSTART*/

#define SYM_ARCH            "\uE0A1"
#define SYM_ARROW_UP        "\uE060"
#define SYM_ARROW_DOWN      "\uE061"
#define SYM_RAM             "\uE021"
#define SYM_AC_FULL         "\uE041"
#define SYM_AC              "\uE040"
#define SYM_SPEAKER         "\uE04E"
#define SYM_SPEAKER_MUTE    "\uE04F"
#define SYM_MUSIC           "\uE05C"
#define SYM_MUSIC_PLAY      "\uE09A"
#define SYM_MUSIC_PAUSE     "\uE09B"
#define SYM_NET_WIRED       "\uE19C"
#define SYM_PACMAN          "\uE00F" //"\uE00F" // or E0A0
#define SYM_PACMAN_GHOST    "\uE0C8"
#define SYM_PACMAN_FOOD     "\uE190"

#define NET_UP SYM_ARROW_UP
#define NET_DOWN SYM_ARROW_DOWN
#define XAUTOLOCK "\uE027"

#define STATUS_LEN 8192

#define SLEEP_TIME 1.0

static Display *dpy;

int toggle = 0, toggle3 = 0;

enum {BattFull, BattCharging, BattDischarging};
struct BatteryInfo {
    int percent;
    int ac;
    int status;
    int hours;
    int minutes;
    int seconds;
};

struct NetSpeed {
    float wiredUp;
    float wiredDown;
    long int wired_rx;
    long int wired_tx;
    float wirelessUp;
    float wirelessDown;
    long int wireless_rx;
    long int wireless_tx;
};

struct Temperature {
    float temp; /* temp*_input */
    float warn; /* temp*_max */
    float crit; /* temp*_crit */
};

// BEGIN PULSE

#define UNUSED __attribute__((unused))

struct pulseaudio_t {
    pa_context *cxt;
    pa_mainloop *mainloop;

    char *default_sink;
};

static void server_info_cb(pa_context UNUSED *c, const pa_server_info *i, void *raw)
{
    struct pulseaudio_t *pulse = raw;
    pulse->default_sink = strdup(i->default_sink_name);
}

static void connect_state_cb(pa_context *cxt, void *raw)
{
    enum pa_context_state *state = raw;
    *state = pa_context_get_state(cxt);
}


static void pulse_async_wait(struct pulseaudio_t *pulse, pa_operation *op)
{
    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING)
        pa_mainloop_iterate(pulse->mainloop, 1, NULL);
}

static void get_default_sink_volume_cb(pa_context UNUSED *c, const pa_sink_info *i, int eol,
        void *raw) {
    if (eol)
        return;


    if (i->mute) {
        (*(int*)raw) = -1;
    } else {
        (*(int*)raw) = (int)(((double)pa_cvolume_avg(&(i->volume)) * 100)
                / PA_VOLUME_NORM);
    }
}
int get_default_sink_volume(struct pulseaudio_t *pulse) {
    pa_operation *op;
    int volume = -2;

    op = pa_context_get_sink_info_by_name(pulse->cxt, pulse->default_sink, get_default_sink_volume_cb, &volume);

    pulse_async_wait(pulse, op);
    pa_operation_unref(op);

    return volume;
}

static int pulse_init(struct pulseaudio_t *pulse)
{
    pa_operation *op;
    enum pa_context_state state = PA_CONTEXT_CONNECTING;

    pulse->mainloop = pa_mainloop_new();
    pulse->cxt = pa_context_new(pa_mainloop_get_api(pulse->mainloop), "bestpony");
    pulse->default_sink = NULL;

    pa_context_set_state_callback(pulse->cxt, connect_state_cb, &state);
    pa_context_connect(pulse->cxt, NULL, PA_CONTEXT_NOFLAGS, NULL);
    while (state != PA_CONTEXT_READY && state != PA_CONTEXT_FAILED)
        pa_mainloop_iterate(pulse->mainloop, 1, NULL);

    if (state != PA_CONTEXT_READY) {
        fprintf(stderr, "failed to connect to pulse daemon: %s\n",
                pa_strerror(pa_context_errno(pulse->cxt)));
        return 1;
    }

    op = pa_context_get_server_info(pulse->cxt, server_info_cb, pulse);
    pulse_async_wait(pulse, op);
    pa_operation_unref(op);
    return 0;
}

static void pulse_deinit(struct pulseaudio_t *pulse)
{
    pa_context_disconnect(pulse->cxt);
    pa_mainloop_free(pulse->mainloop);
    free(pulse->default_sink);
}

// END PULSE

void setstatus(char *str) {
    XStoreName(dpy, DefaultRootWindow(dpy), str);
    XSync(dpy, False);
}
void readfilef(char* filename, float *var) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%f", var);
    fclose(fd);
}
void readfileli(char* filename, long int *var) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%ld", var);
    fclose(fd);
}
void readfilei(char* filename, int *var) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%d", var);
    fclose(fd);
}
void readfiles(char* filename, char var[]) {
    FILE *fd;
    fd = fopen(filename, "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", filename, strerror(errno));
        return;
    }
    fscanf(fd, "%s", var);
    fclose(fd);
}

pid_t pidof(const char* pname) {
//pid_t proc_find(const char* name) 
//{
    DIR* dir;
    struct dirent* ent;
    char* endptr;
    char buf[100];
    
    if (!(dir = opendir("/proc"))) {
        perror("opendir");
        return -1;
    }

    long lpid = -1;

    while((ent = readdir(dir)) != NULL) {
        /* if endptr is not a null character, the directory is not
         * entirely numeric, so ignore it */
        lpid = strtol(ent->d_name, &endptr, 10);
        if (*endptr != '\0') {
            continue;
        }
        if (lpid < 1000) {
            // Ignore kernel threads
            continue;
        }

        /* try to open the cmdline file */
        snprintf(buf, sizeof(buf), "/proc/%ld/status", lpid);
        FILE* fp = fopen(buf, "r");
        
        if (fp) {
            if (fscanf(fp, "Name: %s ", buf)) {
                if (strcmp(buf, pname) == 0) {
                    fclose(fp);
                    closedir(dir);
                    return (pid_t)lpid;
                }
            }
            fclose(fp);
        }
        
    }
    
    closedir(dir);
    return -1;
}

int wireless_init() {
    int skfd;         /* generic raw socket desc. */

    if((skfd = iw_sockets_open()) < 0) {
        fprintf(stderr, "Cannot open socket\n");
        return -1;
    }
    return skfd;
}
void wireless_close(int skfd) {
    if (skfd == -1) {
        return;
    }
    iw_sockets_close(skfd);
}

int getwireless_essid(int skfd, char essid[IW_ESSID_MAX_SIZE + 1]) {
    struct iwreq wrq;

    if (skfd == -1) {
        return 1;
    }

    /* Make sure ESSID is always NULL terminated */
    memset(essid, 0, IW_ESSID_MAX_SIZE + 1);

    /* Get ESSID */
    //wrq.u.essid.pointer = (caddr_t)essid;
    wrq.u.essid.pointer = essid;
    wrq.u.essid.length = IW_ESSID_MAX_SIZE + 1;
    wrq.u.essid.flags = 0;
    if(iw_get_ext(skfd, WIRELESS_DEV, SIOCGIWESSID, &wrq) < 0) {
        perror("iw_get_ex");
        return 1;
    }

    return 0; // succeeded
}

float getwireless_strength(int skfd) {
    iwstats stats;
    iwrange	range;
    if(iw_get_range_info(skfd, WIRELESS_DEV, &range) >= 0) {
        if(iw_get_stats(skfd, WIRELESS_DEV, &stats,
                    &range, 1) >= 0) {
            return (float)stats.qual.qual / (float)range.max_qual.qual;
        }
    }
    return 0;
}

char *getip(const char *interface) {
    struct ifaddrs *ifaddr, *ifa;
    int family, s;
    char host[1025]; /* NI_MAXHOST */
    char *ret = NULL;

    if (getifaddrs(&ifaddr) == -1) {
        perror("getifaddrs");
        return "";
    }

    /* Walk through linked list, maintaining head pointer so we
       can free list later */

    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL)
            continue;
        if (strcmp(ifa->ifa_name, interface) != 0)
            continue;

        family = ifa->ifa_addr->sa_family;

        /* Display interface name and family (including symbolic
           form of the latter for the common families) */

        if (family == AF_INET) {

            /* For an AF_INET* interface address, display the address */
            s = getnameinfo(ifa->ifa_addr,
                    sizeof(struct sockaddr_in),
                    host, 1025, NULL, 0, NI_NUMERICHOST); /* 1025 = NI_MAXHOST */
            if (s != 0) {
                return "";
            }
            ret = strdup(host);
        }
    }

    freeifaddrs(ifaddr);
    return ret;
}
int getwired() {
    int isup;
    readfilei("/sys/class/net/" WIRED_DEV "/carrier", &isup);
    return isup;
}
int iswifi() {
    FILE *fd;
    fd = fopen("/sys/class/net/" WIRELESS_DEV "/carrier", "r");
    if(fd == NULL) {
        return 0;
    }
    fclose(fd);
    return 1;
}
int isbonded() {
    FILE *fd;
    fd = fopen("/sys/class/net/" BONDED_DEV "/carrier", "r");
    if(fd == NULL) {
        return 0;
    }
    fclose(fd);
    return 1;
}

/*
 * Returns in MHz
 */
float getfreqf(int cpu) {
    float freq = 0;

    switch (cpu) {
        case 0:
            readfilef("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq);
            break;
        case 1:
            readfilef("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", &freq);
            break;
        case 2:
            readfilef("/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq", &freq);
            break;
        case 3:
            readfilef("/sys/devices/system/cpu/cpu3/cpufreq/scaling_cur_freq", &freq);
            break;
    }

    freq /= 1000;
    freq /= 1000;

    return freq;;
}
/*
 * Returns in Hz
 */
int getfreqi(int cpu) {
    int freq = 0;

    switch (cpu) {
        case 0:
            readfilei("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", &freq);
            break;
        case 1:
            readfilei("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq", &freq);
            break;
        case 2:
            readfilei("/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq", &freq);
            break;
        case 3:
            readfilei("/sys/devices/system/cpu/cpu3/cpufreq/scaling_cur_freq", &freq);
            break;
    }

    freq /= 1000;

    return freq;;
}

struct BatteryInfo getbattery() {
    struct BatteryInfo info;
    int charge_now, charge_full, current;
    char status[30];

    readfilei("/sys/class/power_supply/BAT0/charge_now", &charge_now);
    readfilei("/sys/class/power_supply/BAT0/charge_full", &charge_full);
    readfilei("/sys/class/power_supply/BAT0/current_now", &current);
    readfiles("/sys/class/power_supply/BAT0/status", status);

    readfilei("/sys/class/power_supply/AC/online", &(info.ac));

    if (!strcasecmp(status, "discharging")) {
        // Battery is discharging
        info.status = BattDischarging;
        info.seconds = (float)3600 * (float)charge_now / (float)current;
    } else if (!strcasecmp(status, "charging")) {
        // Battery is charging
        info.status = BattCharging;
        info.seconds = (float)3600 * (float)(charge_full - (float)charge_now) / (float)current;
    } else if (!strcasecmp(status, "full")) {
        info.status = BattFull;
        info.seconds = 0;
        info.percent = 100;
        return info;
    } else {
        fprintf(stderr,"Unknown battery status: %s\n", status);
        // Not sure what's happening
        return info;
    }


    info.percent = ((float)charge_now) / ((float)charge_full) * (float)100;

    // Assign hours and minutes, and decrement seconds at the same time
    info.seconds -= (info.hours = info.seconds / 3600) * 3600; // integer division
    info.seconds -= (info.minutes = info.seconds / 60) * 60; // integer division

    return info;
}

int getram() {
    FILE *fd;
    long int tot, free, buf, cache;
    // TODO: Only open this once...
    fd = fopen("/proc/meminfo", "r");
    if(fd == NULL) {
        fprintf(stderr, "Error opening %s: %s.\n", "/proc/meminfo", strerror(errno));
        return 0;
    }
    fscanf(fd, "MemTotal: %li kB MemFree: %li kB Buffers: %li kB Cached: %li kB", &tot, &free, &buf, &cache);
    fclose(fd);
    return (tot - free - buf - cache) / 1024;
}

struct NetSpeed getnetspeed(struct NetSpeed last, float timediff) {
    long int wired_rx, wired_tx, wireless_rx, wireless_tx;
    readfileli("/sys/class/net/" WIRED_DEV "/statistics/rx_bytes", &wired_rx);
    readfileli("/sys/class/net/" WIRED_DEV "/statistics/tx_bytes", &wired_tx);
    readfileli("/sys/class/net/" WIRELESS_DEV "/statistics/rx_bytes", &wireless_rx);
    readfileli("/sys/class/net/" WIRELESS_DEV "/statistics/tx_bytes", &wireless_tx);
    last.wiredDown = (float)(wired_rx - last.wired_rx)/timediff;
    last.wiredUp = (float)(wired_tx - last.wired_tx)/timediff;
    last.wirelessDown = (float)(wireless_rx - last.wireless_rx)/timediff;
    last.wirelessUp = (float)(wireless_tx - last.wireless_tx)/timediff;
    last.wired_rx = wired_rx;
    last.wired_tx = wired_tx;
    last.wireless_rx = wireless_rx;
    last.wireless_tx = wireless_tx;
    return last;
}

struct Temperature gettemp(int n) {
    struct Temperature ret;
    char tmp[ strlen("/sys/devices/platform/coretemp.0/temp%d_crit_alarm") ];
    int temperature;

    sprintf(tmp, "/sys/devices/platform/coretemp.0/temp%d_input", n);
    readfilei(tmp, &temperature);
    ret.temp = (float)temperature/1000;

    sprintf(tmp, "/sys/devices/platform/coretemp.0/temp%d_max", n);
    readfilei(tmp, &temperature);
    ret.warn = (float)temperature/1000;

    sprintf(tmp, "/sys/devices/platform/coretemp.0/temp%d_crit", n);
    readfilei(tmp, &temperature);
    ret.crit = (float)temperature/1000;

    return ret;
}

/*[> If fg == bg == -5, then swap them (for BEND) <]*/
/*void add_color(char *status, int fg, int bg) {*/
    /*if (fg == bg == -5) {*/
        /*sprintf(status + strlen(status), "\e[1;%dm\e[1;%dm", color_bg + 30, color_fg + 40);*/
        /*return;*/
    /*}*/
    /*if (fg == bg == -2) {*/
        /*sprintf(status + strlen(status), "\e[1;%dm\e[1;%dm", color_bg + 40, color_fg + 30);*/
        /*return;*/
    /*}*/
    /*if (fg < 0 && bg < 0) {*/
        /*fg = 0;*/
        /*bg = 0;*/
    /*}*/
    /*if (fg >= 0 && fg <= 7) {*/
        /*color_fg = fg;*/
        /*sprintf(status + strlen(status), "\e[1;%dm", color_fg + 30);*/
    /*}*/
    /*if (bg >= 0 && bg <= 7) {*/
        /*color_bg = bg;*/
        /*sprintf(status + strlen(status), "\e[1;%dm", color_bg + 40);*/
    /*}*/
/*}*/

void add_start(char *status, char *color_fg, char *color_after) {
    strcat(status, COL_DEF_BG(FG));
    strcat(status, color_fg);
    strcat(status, BSTART);
    strcat(status, color_after);
}
void add_sep(char *status) {
    strcat(status, BSEP);
}
void add_end(char *status) {
    strcat(status, COL_DEF_BG(FG));
    strcat(status, BEND);
    strcat(status, COL_RESET);
}

void add_networking_speed(char * status, float speed, int up, int hideIfZero) {
    int megabytes = 0;
    speed /= 1024;
    if (speed >= 1024) {
        speed /= 1024;
        megabytes = 1;
    }
    if (speed >= 1000) {
        speed = truncf(speed);
    } else {
        speed = truncf(10.*speed)/10;
    }
    if (speed) {
        SEP(status);
        if (megabytes) {
            sprintf(status + strlen(status), COL_DESC "%s" COL_NORMAL "%-2.1f" COL_UNIT "M" COL_NORMAL, (up ? NET_UP : NET_DOWN), speed );
        } else {
            sprintf(status + strlen(status), COL_DESC "%s" COL_NORMAL "%-5.0f", (up ? NET_UP : NET_DOWN), speed );
        }
    } else {
        if (hideIfZero) {
        } else {
            SEP(status);
            strcat(status, COL_DESC);
            strcat(status, (up ? NET_UP : NET_DOWN));
            strcat(status, "     " COL_NORMAL);
        }
    }
}
char *add_networking_ip(char *status, char *ip) {
    if (ip != NULL) {
        if (strncmp(ip, "172.17.1.", strlen("172.17.1.")) == 0) {
            strcat(status, COL_IP "172.." COL_NORMAL);
            strcat(status, ip + strlen("172.17.1."));
        } else if (strncmp(ip, "192.168.1.", strlen("192.168.1.")) == 0) {
            strcat(status, COL_IP "192.." COL_NORMAL);
            strcat(status, ip + strlen("192.168.1."));
        } else if (strncmp(ip, "10.10.1.", strlen("10.10.1.")) == 0) {
            strcat(status, COL_IP "10.." COL_NORMAL);
            strcat(status, ip + strlen("10.10.1."));
        } else {
            const char *ldot = rindex(ip, '.');
            if (ldot != NULL) {
                strcat(status, COL_IP);
                strncat(status, ip, (ldot - ip) + 1);
                strcat(status, COL_NORMAL);
                strcat(status, ldot + 1);
            } else {
                strcat(status, ip);
            }
        }
        free(ip);
        ip = NULL;
    }
    return ip;
}
void add_networking(char *status) {
    static struct NetSpeed netspeed;
    netspeed = getnetspeed(netspeed, SLEEP_TIME);
    static int first = 1;

    int wired = 0;
    int wireless = 0;
    int bonded = 0;

    char essid[IW_ESSID_MAX_SIZE + 1];
    float wifi_qual;
    static int wifi_skfd;

    if (status == NULL) {
        wireless_close(wifi_skfd);
        first = 1;
        return;
    }

    if (first) {
        wifi_skfd = wireless_init();
    }

    if (getwired()) {
        wired = 1;
    }
    if (isbonded()) {
        bonded = 1;
    }
    if (!getwireless_essid(wifi_skfd, essid) && strlen(essid)) {
        wireless = 1;
    }

    if (!wired && !wireless) {
        first = 0;
        return;
    }

    START(status);
    if (bonded) {
        strcat(status, COL_DESC "BOND " COL_NORMAL);
        add_networking_ip(status, getip(BONDED_DEV));
        strcat(status, " ");
    }

    if (wireless) {
        if (!bonded) {
            strcat(status, COL_DESC "WIFI " COL_NORMAL);
        }
        wifi_qual = getwireless_strength(wifi_skfd);
        int len = strlen(essid);
        strcat(status, "\033[38;5;034m");
        for (int i = 0; i < len; i ++) {
            if (i > (int)((float)len * wifi_qual)) {
                strcat(status, COL_NORMAL);
            }
            strncat(status, &essid[i], 1);
        }
        strcat(status, COL_NORMAL);
        if (!bonded) {
            strcat(status, COL_IP "/" COL_NORMAL);
            add_networking_ip(status, getip(WIRELESS_DEV));
        }
        add_networking_speed(status, !first * netspeed.wirelessDown, 0, (bonded && wired));
        add_networking_speed(status, !first * netspeed.wirelessUp, 1, (bonded && wired));
        if (bonded && wired) {
            // Only need a sep (bonded, both are connected
            strcat(status, " ");
            strcat(status, COL_SEP SYM_NET_WIRED COL_NORMAL);
        } else if (wired) {
            END(status);
            START(status);
            strcat(status, COL_DESC SYM_NET_WIRED " " COL_NORMAL);
        }
    }

    if (wired) {
        if (!bonded) {
            strcat(status, COL_DESC "WIRED" COL_IP "/" COL_NORMAL);
            add_networking_ip(status, getip(WIRED_DEV));
            strcat(status, " " COL_NORMAL);
        }
        add_networking_speed(status, !first * netspeed.wiredDown, 0, 0);
        add_networking_speed(status, !first * netspeed.wiredUp, 1, 0);
    }

    END(status);

    first = 0;
}

void add_volume(char *status) {
    static struct pulseaudio_t pulse;
    static int pulseready = -1;
    int vol = -1;
    if (status == NULL) {
        if (pulseready > 0) {
            pulse_deinit(&pulse);
            pulseready = -1;
        }
        return;
    }
    /* initialize pulse */
    if (pulseready == -1) {
        if (pulse_init(&pulse) == 0) {
            pulseready = 1;
        } else {
            pulseready = 0;
        }
    }

    /*add_start(status, COL_WARNING_BG(BG), COL_WARNING);*/
    START(status);

    if (pulseready > 0) {
        vol = get_default_sink_volume(&pulse);
        if (vol == -2) {
            /* Error connecting */
            pulse_deinit(&pulse);
            pulseready = 0;
        } else if (vol == -1) {
            strcat(status, COL_DESC SYM_SPEAKER_MUTE COL_NORMAL);
        } else {
            strcat(status, COL_DESC SYM_SPEAKER COL_NORMAL);
            sprintf(status + strlen(status), "% 3d" COL_UNIT "%%" COL_NORMAL, vol);
        }
    } else {
        strcat(status, COL_DESC SYM_SPEAKER COL_NORMAL);
        if (pulseready == 0) {
            /* initialize pulse */
            if (pulse_init(&pulse) == 0)
                pulseready = 1;
        } else {
            pulseready = - ((-pulseready + 1) % 5);
        }
        switch (toggle3) {
            case 0:
                strcat(status, " -");
                break;
            case 1:
                strcat(status, " |");
                break;
            case 2:
                strcat(status, " +");
                break;
        }
    }
    END(status);
}

void add_screenlocker(char *status) {
    if (pidof("xautolock") == -1) {
        // xautolock NOT running
        START(status);
        strcat(status, COL_WARNING XAUTOLOCK COL_NORMAL);
        END(status);
    }
}

void add_ram(char *status) {
    int ram = getram();
    START(status);
    strcat(status, COL_DESC SYM_RAM COL_NORMAL " ");
    if (ram > 7400) {
        switch (toggle) {
            case 0:
                strcat(status, COL_CRITICAL);
                break;
            case 1:
                strcat(status, COL_WARNING);
                break;
        }
    } else if (ram > 7000) {
        strcat(status, COL_CRITICAL);
    } else if (ram > 6300) {
        strcat(status, COL_WARNING);
    }
    sprintf(status + strlen(status), "%d" COL_UNIT "M" COL_NORMAL, ram);
    END(status);
}

void add_battery(char *status) {
    struct BatteryInfo battinfo = getbattery();
    const char *col = COL_NORMAL;

    START(status);
    strcat(status, COL_DESC "BAT" COL_NORMAL);
    if (battinfo.percent > 90) {
        col = "\x1b[38;5;046m";
    } else if (battinfo.percent > 80) {
        col = "\x1b[38;5;047m";
    } else if (battinfo.percent > 70) {
        col = "\x1b[38;5;084m";
    } else if (battinfo.percent > 60) {
        col = "\x1b[38;5;120m";
    } else if (battinfo.percent > 50) {
        col = "\x1b[38;5;157m";
    } else if (battinfo.percent > 40) {
        col = "\x1b[38;5;156m";
    } else if (battinfo.percent > 30) {
        col = "\x1b[38;5;155m";
    } else if (battinfo.percent > 20) {
        col = "\x1b[38;5;154m";
    } else if (battinfo.percent > 15) {
        col = COL_WARNING;
    } else if (battinfo.percent > 14) {
        col = "\x1b[38;5;220m";
    } else if (battinfo.percent > 13) {
        col = "\x1b[38;5;214m";
    } else if (battinfo.percent > 12) {
        col = "\x1b[38;5;208m";
    } else if (battinfo.percent > 11) {
        col = "\x1b[38;5;202m";
    } else {
        if (battinfo.percent <= 5) {
            switch (toggle3) {
                case 0:
                    col = COL_WARNING;
                    break;
                case 1:
                    col = COL_CRITICAL;
                    break;
                case 2:
                    col = "\x1b[38;5;208m";
                    break;
            }
        } else {
            col = COL_CRITICAL;
        }
    }

    // When charging, display that symbol; when fully charged, and plugged in, display that symbol (never both at the same time)
    if (battinfo.status == BattCharging) {
        strcat(status, " ");
        strcat(status, col);
        /*static const char *chars[] = {".  ", "|  ", "|. ", "|| ", "||.", "|||"};*/
        /*static const char *chars[] = {".  ", ".. ", "...", "..|", ".||", "|||"};*/
        static const char *chars[] = {".|:", ":.|", "|:.", ".|:", ":.|", "|:."};
        switch ((toggle3 << 1) + toggle) {
            case 5:
                strcat(status, chars[0]);
                break;
            case 0:
                strcat(status, chars[1]);
                break;
            case 3:
                strcat(status, chars[2]);
                break;
            case 4:
                strcat(status, chars[3]);
                break;
            case 1:
                strcat(status, chars[4]);
                break;
            case 2:
                strcat(status, chars[5]);
                break;
        }
    } else if (battinfo.ac) {
        strcat(status, col);
        strcat(status, " |||");
    }
    if (!battinfo.ac || battinfo.status == BattCharging) {
        strcat(status, " ");
        sprintf(status + strlen(status),
                "%s%i" COL_UNIT "%%%s"
                " "
                "%02d" COL_SEP ":%s%02d" COL_SEP ":%s%02d"
                , col, battinfo.percent, col,
                battinfo.hours, col, battinfo.minutes, col, battinfo.seconds);
    }
    END(status);
}

void add_temperature(char *status) {
    int color = 0;
    const char *col = COL_NORMAL;
    const int max_i = 3;
    struct Temperature temp[max_i];
    for (int i = 1; i <= max_i; i ++) {
        temp[i-1] = gettemp(i);
        if (temp[i-1].temp >= 0.96*temp[i-1].crit) {
            if (color < 2) { color = 2; }
        } else if (temp[i-1].temp >= temp[i-1].warn) {
            if (color < 1) { color = 1; }
        }
    }

    START(status);
    switch (color) {
        case 0:
            col = COL_NORMAL;
            break;
        case 1:
            col = COL_WARNING;
            break;
        case 2:
            col = COL_CRITICAL;
            break;
    }

    for (int i = 0; i < max_i; i ++) {
        if ((i - 1) >= 0) {
            strcat(status, " ");
        }

        sprintf(status + strlen(status), "%s%.0f" COL_UNIT "C%s", col, temp[i].temp, col);
    }

    END(status);
}

void add_datetime(char *status) {
    char datetime[30];
    time_t result;
    struct tm *resulttm;

    result = time(NULL);
    resulttm = localtime(&result);
    if(resulttm == NULL) {
        return;
    }
    START(status);

    strcat(status, COL_IP);
    strftime(datetime, sizeof(datetime)/sizeof(datetime[0])-1, "%b %d", resulttm);
    strcat(status, datetime);

    strcat(status, COL_SEP " | " COL_NORMAL);

    strftime(datetime, sizeof(datetime)/sizeof(datetime[0])-1, "%H", resulttm);
    strcat(status, datetime);
    strcat(status, COL_SEP ":" COL_NORMAL);

    strftime(datetime, sizeof(datetime)/sizeof(datetime[0])-1, "%M", resulttm);
    strcat(status, datetime);
    strcat(status, COL_SEP ":" COL_NORMAL);

    strftime(datetime, sizeof(datetime)/sizeof(datetime[0])-1, "%S", resulttm);
    strcat(status, datetime);

    END(status);
}

void add_cpufreq(char *status) {
    int freq0, freq1, freq2, freq3, freqavg;
    static int count = 0;
    if (count > 1) {
        return;
    }
    freq0 = getfreqi(0);
    freq1 = getfreqi(1);
    freq2 = getfreqi(2);
    freq3 = getfreqi(3);
    freqavg = (freq0+freq1+freq2+freq3)/4;
    if (freqavg == 0) {
        count ++;
    } else {
        START(status);
        strcat(status, COL_DESC "FREQ " COL_NORMAL);
        sprintf(status + strlen(status), "%d", freqavg);
        END(status);
    }
}
void add_loadavg(char *status) {
    double tmp[3] = {0};
    if (getloadavg(tmp,3) == -1) {
        return;
    }

    START(status);
    strcat(status, COL_DESC "LOAD " COL_NORMAL);
    sprintf(status + strlen(status), "%.2f %.2f", tmp[0], tmp[1]);
    END(status);
}

void add_mpdsong(char *status) {
	static struct mpd_connection *conn = NULL;
    struct mpd_status *mpdstatus = NULL;
    enum mpd_state state;
    struct mpd_song *song;
    const char *title, *artist, *album;

    if (conn == NULL) {
        conn = mpd_connection_new(NULL, 0, 0);
        if (conn == NULL) {
            fputs("Out of memory\n", stderr);
            exit(1);
        }
        if (mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS) {
            mpd_connection_free(conn);
            conn = NULL;
            return;
        }
    }

    if (conn != NULL) {
        mpdstatus = mpd_run_status(conn);
        if (mpdstatus == NULL) {
            return;
        }
        state = mpd_status_get_state(mpdstatus);
        if(state == MPD_STATE_STOP || state == MPD_STATE_UNKNOWN){
            mpd_status_free(mpdstatus);
            return;
        }

        if (state == MPD_STATE_PLAY || state == MPD_STATE_PAUSE) {
            song = mpd_run_current_song(conn);

            if(song == NULL){
                mpd_status_free(mpdstatus);
            }
            title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
            artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
            album = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
            if (title != NULL) {
                START(status);
                if (state == MPD_STATE_PLAY) {
                    strcat(status, COL_DESC SYM_MUSIC_PLAY " " COL_NORMAL);
                } else {
                    strcat(status, COL_DESC SYM_MUSIC_PAUSE " " COL_NORMAL);
                }

                sprintf(status + strlen(status), COL_DESC "[" "\x1b[38;5;147m" "%i" COL_SEP "/" "\x1b[38;5;147m" "%u" COL_DESC "]" COL_NORMAL " ", mpd_status_get_song_pos(mpdstatus) + 1, mpd_status_get_queue_length(mpdstatus));

                strcat(status, "\x1b[38;5;121m");
                strcat(status, title);

                strcat(status, COL_DESC " (");
                strcat(status, "\x1b[38;5;063m");
                strcat(status, album);
                strcat(status, COL_DESC ") | ");

                strcat(status, "\x1b[38;5;103m");
                strcat(status, artist);
                strcat(status, COL_NORMAL);

                strcat(status, COL_DESC " | " COL_NORMAL);

                sprintf(status + strlen(status),
                        "\x1b[38;5;083m" "%i" COL_SEP ":"
                        "\x1b[38;5;083m" "%02i" COL_SEP "/"
                        "\x1b[38;5;083m" "%i" COL_SEP ":"
                        "\x1b[38;5;083m" "%02i" COL_NORMAL,
                        mpd_status_get_elapsed_time(mpdstatus) / 60,
                        mpd_status_get_elapsed_time(mpdstatus) % 60,
                        mpd_status_get_total_time(mpdstatus) / 60,
                        mpd_status_get_total_time(mpdstatus) % 60);

                END(status);
            }

            mpd_song_free(song);
            mpd_status_free(mpdstatus);

        }
    }
}

void add_kernelinfo(char *status) {
    static struct utsname uts;
    static int first = 1;
    if (first) {
        if (0 == uname(&uts)) {
            first = 0;
        }
    }
    if (!first) { /* Only need to get info once, as the runinng kernel is unlikely to change */
        START(status);
        strcat(status, "\x1b[38;5;027m");
        strcat(status, SYM_ARCH);
        strcat(status, " ");
        strcat(status, COL_DESC);
        strcat(status, uts.sysname);
        strcat(status, " ");
        strcat(status, COL_IP);
        strcat(status, uts.release);
        strcat(status, COL_NORMAL);
        END(status);
    }
}

void add_uptime(char *status) {
    static struct sysinfo s_info;
    unsigned long sec, min, hour, day;
    if (0 == sysinfo(&s_info)) { /* Only need to get info once, as the runinng kernel is unlikely to change */
        START(status);
        strcat(status, COL_DESC "UPTIME ");

        sec = s_info.uptime;
        min = sec / 60;
        sec %= 60;
        hour = min / 60;
        min %= 60;
        day = hour / 24;
        hour %= 24;

        if (day > 0) {
            sprintf(status + strlen(status), COL_NORMAL "%ld" COL_UNIT "d ", day);
        }
        if (day > 0 || hour > 0) {
            sprintf(status + strlen(status), COL_NORMAL "%ld" COL_UNIT "h ", hour);
        }
        sprintf(status + strlen(status), COL_NORMAL "%2ld" COL_UNIT "m ", min);

        END(status);
    }
}

void add_dropbox(char *status) {
    static struct sockaddr_un sock;
    static int sockfd;
    static int first = 1;
    static int adding = 0;

    if (DROPBOX_SOCKET != NULL) {
        if (first) {
            if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
                perror("add_dropbox");
                return;
            }
            sock.sun_family = AF_UNIX;
            strcpy(sock.sun_path, DROPBOX_SOCKET);
            if (connect(sockfd, (struct sockaddr *)&sock, sizeof(sock)) == -1) {
                perror("add_dropbox");
                close(sockfd);
                return;
            }

            struct timeval tv;
            tv.tv_sec = 1;  /* 30 Secs Timeout */
            tv.tv_usec = 0;  // Not init'ing this can cause strange errors
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(struct timeval));

            first = 0;
        }

        if (!first) {
            char buf[512];
            strcpy(buf, "get_dropbox_status\ndone\n");
            if (send(sockfd, buf, strlen(buf), 0) == -1) {
                perror("add_dropbox");
                return;
            }
            int t;
            if ((t = recv(sockfd, buf, sizeof(buf), 0)) > 0) {
                /*buf[t] = '\0';*/
                char *tmp = buf;
                if (strncmp(tmp, "ok\nstatus", strlen("ok\nstatus")) == 0) {
                    tmp += 3 + strlen("status");
                    while (isspace(*tmp)) {
                        tmp++;
                    }
                    START(status);
                    strcat(status, COL_DESC "DROPBOX ");
                    if (strncmp(tmp, "done", strlen("done")) == 0) {
                        strcat(status, COL_IP);
                        strcat(status, "Idle");
                    } else {
                        strcat(status, "\x1b[38;5;039m");
                        // They're ok...
                        char *idx = tmp;
                        int f = 1;
                        char *newln = index(idx, '\n');
                        while (idx != NULL && f < 5) {
                            char *tab = index(idx, '\t');
                            if (tab > newln || tab == NULL) {
                                tab = newln;
                            }
                            if (tab != NULL && (tab - idx)) {
                                if (f > 1) {
                                    strcat(status, " - ");
                                }
                                f ++;
                                strncat(status, idx, tab - idx);
                                idx = tab + 1;
                            } else {
                                break;
                            }
                            if (tab >= newln) {
                                break;
                            }
                        }
                    }
                    END(status);
                    adding = 0; /* Reset on a successful run */
                }
            } else {
                if (t == 0) {
                    close(sockfd);
                    sockfd = 0;
                    first = 1;
                }
                if (adding == 0) {
                    adding = 1;
                    add_dropbox(status);
                    adding = 0;
                }
                return;
            }

        }

    }
}

void add_pacman(char *status) {
    static int pac_toggle = 0, flip = 0;
    const int max = 10;
    int i;
    START(status);
    strcat(status, COL_NORMAL COL_NORMAL_BG("3"));
    for (i = 0; i < max; i ++) {
        if (0) {
            if (i < pac_toggle) {
                strcat(status, SYM_PACMAN);
            } else if (i == pac_toggle) {
                strcat(status, COL_NORMAL);
                strcat(status, SYM_PACMAN);
            } else {
                strcat(status, SYM_PACMAN_FOOD);
            }
        } else {
            if (i == pac_toggle && i == ((max-1) - pac_toggle)) {
                strcat(status, "\x1b[38;5;89m" SYM_PACMAN_GHOST COL_NORMAL_BG("3"));
            } else if (i == pac_toggle) {
                if (flip) {
                    strcat(status, "\x1b[38;5;38m" SYM_PACMAN_GHOST COL_NORMAL_BG("3"));
                } else {
                    strcat(status, "\x1b[38;5;178m" SYM_PACMAN_GHOST COL_NORMAL_BG("3"));
                }
            } else if (i == ((max-1) - pac_toggle)) {
                if (flip) {
                    strcat(status, "\x1b[38;5;178m" SYM_PACMAN_GHOST COL_NORMAL_BG("3"));
                } else {
                    strcat(status, "\x1b[38;5;38m" SYM_PACMAN_GHOST COL_NORMAL_BG("3"));
                }
            } else if ((i > pac_toggle && i < ((max-1) - pac_toggle))
                    || (i < pac_toggle && i > ((max-1) - pac_toggle))) {
                strcat(status, COL_DESC SYM_PACMAN_FOOD COL_NORMAL_BG("4") COL_NORMAL_BG("3"));
            } else {
                strcat(status, SYM_PACMAN_GHOST);
            }
        }
    }
    pac_toggle ++;
    pac_toggle %= (max-1);
    if (pac_toggle == 0) {
        flip = !flip;
    }
    END(status);
}

int main(int argc, char * argv[]) {
    char status[STATUS_LEN];
    int runonce = 0;

    if (argc >= 2) {
        if (!strcmp(argv[1], "once")) {
            runonce = 1;
        }
    }


    // TODO: Touchpad
    //

    if (!runonce) {
        if (!(dpy = XOpenDisplay(NULL))) {
            fprintf(stderr, "Cannot open display.\n");
            return 1;
        }
    }

    for ( ; ; usleep(SLEEP_TIME*1e6)) {

        toggle = ! toggle;
        toggle3 = (toggle3 + 1) % 3;

        strcat(status, COL_RESET);

        add_networking(status);
        add_volume(status);
        add_ram(status);
        add_screenlocker(status);
        add_battery(status);
        add_temperature(status);
        add_loadavg(status);
        add_cpufreq(status);
        add_datetime(status);

        strcat(status, "\1"); /* Switch to the bottom bar */

        add_kernelinfo(status);
        add_uptime(status);
        add_mpdsong(status);
        add_dropbox(status);

        if (runonce) {
            break;
        }

        setstatus(status);
        status[0] = '\0';
    }

    if (!runonce) {
        if (dpy) {
            XCloseDisplay(dpy);
        }
    }

    if (runonce) {
        printf("%s\n", status);
    }

    add_networking(NULL);
    add_volume(NULL);

    return 0;
}
