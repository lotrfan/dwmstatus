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

#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/wireless.h>
#include <linux/if.h>
#include <iwlib.h>

#define _GNU_SOURCE
#include <pulse/pulseaudio.h>

#define WIRELESS_DEV "wlp2s0"
#define WIRED_DEV "enp10s0"
#define BONDED_DEV "bond0"

#define COLOR_NORMAL 0
#define COLOR_CRITICAL 5
#define COLOR_WARNING 4


#define FG  "3"
#define BG  "4"
#define COL_DEF_FG(b)  "\x1b[" b "8;5;242m"
#define COL_DEF_BG(b)  "\x1b[" b "8;5;232m"

#define COL_WARNING_FG(b) COL_DEF_BG(b)
#define COL_WARNING_BG(b) "\x1b[1;" b "3m"
#define COL_WARNING COL_WARNING_FG(FG) COL_WARNING_BG(BG)

/*#define COL_CRITICAL_FG(b) "\x1b[" b "8;5;255m"*/
#define COL_CRITICAL_FG(b) "\x1b[1;" b "7;m"
#define COL_CRITICAL_BG(b) "\x1b[1;" b "1m"
#define COL_CRITICAL COL_CRITICAL_FG(FG) COL_CRITICAL_BG(BG)

#define COL_NORMAL_FG(b) COL_DEF_BG(b)
/*#define COL_NORMAL_BG(b) COL_DEF_FG(b)*/
#define COL_NORMAL_BG(b) "\x1b[0;" b "7m"
#define COL_NORMAL COL_NORMAL_FG(FG) COL_NORMAL_BG(BG)

/*#define COL_RESET "\x1b[0m"*/
#define COL_RESET COL_DEF_FG(FG) COL_DEF_BG(BG)

#define START(status) add_start(status, COL_NORMAL_BG(BG), COL_NORMAL);
#define SEP(status) add_sep(status);
#define END(status) add_end(status);
#define START_WARN(status) add_start(status, COL_WARNING_BG(BG), COL_WARNING);
#define START_CRIT(status) add_start(status, COL_CRITICAL_BG(BG), COL_CRITICAL);

#define _BSTART " "
#define _BEND ""
#define _BSEP " "
#define BSTART _BSTART
#define BSEP _BSEP
#define BEND _BEND
/*#define BSTART _BEND*/
/*#define BSEP _BSEP*/
/*#define BEND _BSTART*/

#define WIFI_33 ""
#define WIFI_66 ""
#define WIFI_100 ""
#define WIRED ""
#define NET_UP ""
#define NET_DOWN ""
#define RAM ""
#define VOL_MUTE ""
#define VOL_UNMUTE ""
#define BATT_25 ""
#define BATT_50 ""
#define BATT_75 ""
#define BATT_100 ""
#define BATT_CHARGING ""
#define BATT_AC ""
#define XAUTOLOCK ""
#define STATE_UNKNOWN ""

#define STATUS_LEN 8192

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
    float freq;

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
    int freq;

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

void getdatetime(char buf[], int len) {
    time_t result;
    struct tm *resulttm;

    result = time(NULL);
    resulttm = localtime(&result);
    if(resulttm == NULL) {
        fprintf(stderr, "Error getting localtime.\n");
        exit(1);
    }
    if(!strftime(buf, len-1, "%b %d" _BSEP "%H:%M:%S", resulttm)) {
        fprintf(stderr, "strftime is 0.\n");
    }
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

float getloadavg_min() {
    double tmp = 0;
    if (getloadavg(&tmp,1) == -1) {
        return 0;
    }
    return (float)tmp;
}

void appendNetInfo(char * status, float speed, int up) {
    int megabytes = 0;
    char tmp[15] = "";
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
    if (megabytes) {
        sprintf(status + strlen(status), "%s% 2.1fM", (up ? NET_UP : NET_DOWN), speed );
    } else {
        sprintf(status + strlen(status), "%s% 5.0f", (up ? NET_UP : NET_DOWN), speed );
    }
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

char *add_networking_ip(char *status, char *ip) {
    if (ip != NULL) {
        if (strncmp(ip, "172.17.1.", strlen("172.17.1.")) == 0) {
            strcat(status, "172..");
            strcat(status, ip + strlen("172.17.1."));
        } else if (strncmp(ip, "192.168.1.", strlen("192.168.1.")) == 0) {
            strcat(status, "192..");
            strcat(status, ip + strlen("192.168.1."));
        } else if (strncmp(ip, "10.10.1.", strlen("10.10.1.")) == 0) {
            strcat(status, "10..");
            strcat(status, ip + strlen("10.10.1."));
        } else {
            strcat(status, ip);
        }
        free(ip);
        ip = NULL;
    }
    return ip;
}
void add_networking(char *status) {
    static struct NetSpeed netspeed;
    netspeed = getnetspeed(netspeed, 1);
    static int first = 1;

    int wired = 0;
    int wireless = 0;
    int bonded = 0;

    char essid[IW_ESSID_MAX_SIZE + 1];
    float wifi_qual;
    static int wifi_skfd;

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


    if (wireless) {
        wifi_qual = getwireless_strength(wifi_skfd);
        if (wifi_qual <= 0.33) {
            strcat(status, WIFI_33 " ");
        } else if (wifi_qual <= 0.66) {
            strcat(status, WIFI_66 " ");
        } else {
            strcat(status, WIFI_100 " ");
        }
        strcat(status, essid);
        if (!bonded) {
            strcat(status, " ");
            add_networking_ip(status, getip(WIRELESS_DEV));
        }
        SEP(status);
        appendNetInfo(status, !first * netspeed.wirelessDown, 0);
        SEP(status);
        appendNetInfo(status, !first * netspeed.wirelessUp, 1);
        if (bonded) {
            // Only need a sep (bonded, both are connected
            SEP(status);
        } else if (wired) {
            END(status);
            START(status);
        }
    }

    if (bonded) {
        add_networking_ip(status, getip(BONDED_DEV));
        if (wired) {
            SEP(status);
        }
    }

    if (wired) {
        strcat(status, WIRED);
        if (!bonded) {
            strcat(status, " ");
            add_networking_ip(status, getip(WIRED_DEV));
        }
        add_sep(status);
        appendNetInfo(status, !first * netspeed.wiredDown, 0);
        add_sep(status);
        appendNetInfo(status, !first * netspeed.wiredUp, 1);
    }

    END(status);

    first = 0;
}

void add_volume(char *status) {
    static struct pulseaudio_t pulse;
    static int pulseready = -1;
    int vol = -1;
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
            strcat(status, VOL_MUTE);
        } else {
            sprintf(status + strlen(status), VOL_UNMUTE "% 3d", vol);
        }
    } else {
        if (pulseready == 0) {
            /* initialize pulse */
            if (pulse_init(&pulse) == 0)
                pulseready = 1;
        } else {
            pulseready = - ((-pulseready + 1) % 5);
        }
        switch (toggle3) {
            case 0:
                strcat(status, VOL_MUTE);
                break;
            case 1:
                strcat(status, VOL_UNMUTE);
                break;
            case 2:
                strcat(status, STATE_UNKNOWN);
                break;
        }
    }
    END(status);
}

void add_screenlocker(char *status) {
    if (pidof("xautolock") == -1) {
        // xautolock NOT running
        START(status);
        strcat(status, XAUTOLOCK);
        END(status);
    }
}

void add_ram(char *status) {
    int ram = getram();
    if (ram > 7450) {
        START_CRIT(status);
    } else if (ram > 6700) {
        START_WARN(status);
    } else {
        START(status);
    }
    sprintf(status + strlen(status), RAM "%dM", ram);
    END(status);
}

void add_battery(char *status) {
    struct BatteryInfo battinfo = getbattery();

    if (!battinfo.ac || battinfo.status == BattCharging) {
        if (battinfo.percent > 20) {
            START(status);
        } else if (battinfo.percent > 11) {
            START_WARN(status);
        } else {
            if (battinfo.percent <= 5) {
                if (toggle) {
                    START_CRIT(status);
                } else {
                    START(status);
                }
            } else {
                START_CRIT(status);
            }
        }
    } else {
        START(status);
    }

    // When charging, display that symbol; when fully charged, and plugged in, display that symbol (never both at the same time)
    if (battinfo.status == BattCharging) {
        strcat(status, BATT_CHARGING);
    } else if (battinfo.ac) {
        strcat(status, BATT_AC);
    }
    if (!battinfo.ac || battinfo.status == BattCharging) {
        if (battinfo.percent < 25) {
            strcat(status, BATT_25);
        } else if (battinfo.percent < 50) {
            strcat(status, BATT_50);
        } else if (battinfo.percent < 75) {
            strcat(status, BATT_75);
        } else {
            strcat(status, BATT_100);
        }
        SEP(status);
        sprintf(status + strlen(status), "%i%%", battinfo.percent);
        SEP(status);
        sprintf(status + strlen(status), "%02d:%02d:%02d", battinfo.hours, battinfo.minutes, battinfo.seconds );
    }
    END(status);
}

void add_temperature(char *status) {
    int color = 0;
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

    switch (color) {
        case 0:
            START(status);
            break;
        case 1:
            START_WARN(status);
            break;
        case 2:
            START_CRIT(status);
            break;
    }

    for (int i = 0; i < max_i; i ++) {
        if ((i - 1) >= 0) {
            SEP(status);
        }

        sprintf(status + strlen(status), "%.0fC", temp[i].temp);
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

    strftime(datetime, sizeof(datetime)/sizeof(datetime[0])-1, "%b %d", resulttm);
    strcat(status, datetime);

    SEP(status);

    strftime(datetime, sizeof(datetime)/sizeof(datetime[0])-1, "%H:%M:%S", resulttm);
    strcat(status, datetime);

    END(status);
}

int main(int argc, char * argv[]) {
    int freq0, freq1, freq2, freq3, freqavg;
    char status[STATUS_LEN];

    char tmp[100] = "";
    char tmp1[100] = "";

    //    char _net[IW_ESSID_MAX_SIZE + 1 + 10] = ""; // size of essid + icon + wired

    int runonce = 0;

    if (argc >= 2) {
        if (!strcmp(argv[1], "once")) {
            runonce = 1;
        }
    }


    // TODO: Screenlocking, Touchpad
    //

    if (!runonce) {
        if (!(dpy = XOpenDisplay(NULL))) {
            fprintf(stderr, "Cannot open display.\n");
            return 1;
        }
    }



    for ( ; ; sleep(1)) {

        toggle = ! toggle;
        toggle3 = (toggle3 + 1) % 3;

        strcat(status, COL_RESET);

        add_networking(status);
        add_volume(status);
        add_ram(status);
        add_screenlocker(status);
        add_battery(status);
        add_temperature(status);
        add_datetime(status);

        /*

        [>
        freq0 = getfreqi(0);
        freq1 = getfreqi(1);
        freq2 = getfreqi(2);
        freq3 = getfreqi(3);
        freqavg = (freq0+freq1+freq2+freq3)/4;
        tmp[0] = '\0';
        <]
        //cpustat = getcpuinfo(cpustat);
        //sprintf(tmp, "%4i" _BSEP "%4i" _BSEP "%4i" _BSEP "%4i" _BSEP "%3i%%", freq0, freq1, freq2, freq3, cpustat.usage);
        [>
        sprintf(tmp, "%4i", freqavg);
        appendStatuss(status, tmp, COLOR_NORMAL, 1, 0, 0);
        <]
//        appendStatusi(status, freqavg, 0, 0, "", "", 1, 0, 0);
        //appendStatusf(status, getloadavg_min(), 0, 4, "", "", 0, 1, 1);
        appendStatusf(status, getloadavg_min(), 0, 4, "", "", 1, 1, 0);

//         sprintf(tmp, "%4i" _BSEP "%.2f", freqavg, getloadavg_min());
//         appendStatuss(status, tmp, COLOR_NORMAL, 1, 1, 0);


        /*if (runonce) {*/
            /*break;*/
        /*} else {*/
            setstatus(status);
        /*}*/

        // Reset variables from above
        status[0] = '\0';
        tmp[0] = '\0';
        tmp1[0] = '\0';


    }

    if (dpy) {
        XCloseDisplay(dpy);
    }

    if (runonce) {
        printf("%s%c", status, 0);
    }

    // TODO: pass these NULL to signal a close

    /*wireless_close(wifi_skfd);*/

    /*if (pulseready) {*/
        /*[> shut down <]*/
        /*pulse_deinit(&pulse);*/
    /*}*/

    return 0;
}
