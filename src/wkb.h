// wkbrowser — a headless WebKit browser profile for automation.
//
// Architecture: one process, one WebKitWebView, one synthetic-value profile
// applied as DOCUMENT-END user scripts so pages observe consistent values
// from their first line of execution.
#ifndef WKB_H
#define WKB_H

#include <glib.h>
#include <webkit2/webkit2.h>

// Profile state. Public so wkb_files.c and wkb_net.c can consult it.
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

    // Network posture (Phase 2)
    int block_assets;            // cancel image/font/media
    int block_stylesheets;
    GHashTable* blocklist;       // lowercase substring set
    gboolean ephemeral;          // ephemeral session vs persistent
    long blocked_count;
    long allowed_count;
} WkbProfile;

WkbProfile* wkb_profile_new(void);
void wkb_profile_free(WkbProfile* p);

/* File registry ---------------------------------------------------------- */
void wkb_profile_set_default_file(WkbProfile* p, const char* path);
void wkb_profile_register_file(WkbProfile* p, const char* selector, const char* path);

/* Synthetic values ------------------------------------------------------- */
void wkb_profile_set_geolocation(WkbProfile* p, double lat, double lon, double alt, double acc);
void wkb_profile_set_screen(WkbProfile* p, int w, int h, int aw, int ah, int depth, double dpr);
void wkb_profile_set_hardware(WkbProfile* p, int cores, double memory_gb);
void wkb_profile_set_user_agent(WkbProfile* p, const char* ua);
void wkb_profile_set_timezone(WkbProfile* p, const char* tz);
void wkb_profile_set_vendor(WkbProfile* p, const char* platform, const char* vendor, const char* sub);
void wkb_profile_apply(WkbProfile* p, WebKitWebView* view);

/* File chooser ----------------------------------------------------------- */
gboolean wkb_files_handle_chooser(WebKitFileChooserRequest* req, WkbProfile* p);

/* Network ---------------------------------------------------------------- */
void wkb_profile_set_ephemeral(WkbProfile* p, gboolean ephemeral);
void wkb_profile_set_block_assets(WkbProfile* p, gboolean on);
void wkb_profile_set_block_stylesheets(WkbProfile* p, gboolean on);
void wkb_profile_block_domain(WkbProfile* p, const char* needle);
int  wkb_profile_load_blocklist(WkbProfile* p, const char* path, GError** err);
gboolean wkb_profile_uri_blocked(WkbProfile* p, const char* uri);
gboolean wkb_should_block_resource(WkbProfile* p, const char* uri,
                                   gboolean is_navigation);
typedef enum { WKB_ASSET_NONE = 0, WKB_ASSET_IMAGE, WKB_ASSET_FONT,
               WKB_ASSET_MEDIA, WKB_ASSET_STYLESHEET } WkbAssetKind;
const char* wkb_asset_kind_name(WkbAssetKind k);
long wkb_profile_blocked_count(WkbProfile* p);
long wkb_profile_allowed_count(WkbProfile* p);
WebKitWebContext* wkb_profile_make_context(WkbProfile* p);

/* Fingerprint probe ------------------------------------------------------ */
const char* wkb_probe_js(void);

#endif