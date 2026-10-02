// wkbrowser — a headless WebKit browser profile for automation.
// Phase 1: fake-value injection + file-picker API.
//
// Architecture: a single process drives one WebKitWebView. Every synthetic
// value is applied via a DOCUMENT-END user script, which runs before any
// page script, so pages observe a consistent profile from the first line.
#ifndef WKB_H
#define WKB_H

#include <glib.h>
#include <webkit2/webkit2.h>

// Profile state. Public so wkb_files.c can consult the file registry.
typedef struct {
    GHashTable* files;       // selector -> path
    char* default_file;

    double lat, lon, alt, acc;   // acc < 0 => geolocation unset
    int screen_w, screen_h, avail_w, avail_h, depth;
    double dpr;
    int cores;                   // 0 => unset
    double memory_gb;            // 0 => unset
    char* ua;
    char* tz;
    char* platform, *vendor, *sub;
} WkbProfile;

WkbProfile* wkb_profile_new(void);
void wkb_profile_free(WkbProfile* p);

// File registry: paths registered here are what the file picker hands back.
void wkb_profile_set_default_file(WkbProfile* p, const char* path);
void wkb_profile_register_file(WkbProfile* p, const char* selector, const char* path);

// Synthetic values (all optional; unset values are left at native defaults).
void wkb_profile_set_geolocation(WkbProfile* p, double lat, double lon, double alt, double acc);
void wkb_profile_set_screen(WkbProfile* p, int w, int h, int aw, int ah, int depth, double dpr);
void wkb_profile_set_hardware(WkbProfile* p, int cores, double memory_gb);
void wkb_profile_set_user_agent(WkbProfile* p, const char* ua);
void wkb_profile_set_timezone(WkbProfile* p, const char* tz);
void wkb_profile_set_vendor(WkbProfile* p, const char* platform, const char* vendor, const char* sub);

// Applies the profile to a web view. Must be called before load.
void wkb_profile_apply(WkbProfile* p, WebKitWebView* view);

// File-chooser handler: satisfies the request from the registry, never opens
// a dialog. Returns TRUE (always handled).
gboolean wkb_files_handle_chooser(WebKitFileChooserRequest* req, WkbProfile* profile);

#endif