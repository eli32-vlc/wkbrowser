// wkb_net: scraping-oriented network controls for the embedder.
//
// Three concerns, all implemented without patching WebKit:
//   - ephemeral context, so cookies/storage never leak between navigations
//   - asset blocking (images/fonts/media) for speed and bandwidth
//   - domain blocklist to kill analytics and beacon noise
//
// TWO UPSTREAM LIMITATIONS, both verified against the installed headers:
//
// 1. No content-filter API. WebKitGTK 2.50.6 exposes no public constructor for
//    WebKitUserContentFilter -- only ref/unref/get_identifier exist as symbols.
//    That API is WebKit-internal. So the blocklist is enforced by cancelling
//    matching subresource loads rather than by a JSON rule set.
//
// 2. No resource type on the request. The resource-load-started signal passes a
//    WebKitURIRequest, which in 2.50.6 has no get_resource_type(); the richer
//    WebKitURIResourceRequest type does not exist yet. So asset blocking
//    classifies by file extension rather than by the resource type WebKit
//    actually assigned. Extension matching covers the large majority of CDN
//    subresources but will miss extensionless URLs.
//
// Both are recorded in PHASE2.md as the reason P2-3 deviates from its original
// design.
#include "wkb.h"
#include <string.h>
#include <stdlib.h>

/* ---------------------------------------------------------------- network */

void wkb_profile_set_ephemeral(WkbProfile* p, gboolean ephemeral)
{
    p->ephemeral = ephemeral;
}

void wkb_profile_set_block_assets(WkbProfile* p, gboolean on)
{
    p->block_assets = on;
}

void wkb_profile_set_block_stylesheets(WkbProfile* p, gboolean on)
{
    p->block_stylesheets = on;
}

// Each entry is a lowercase substring matched against the request URI.
void wkb_profile_block_domain(WkbProfile* p, const char* needle)
{
    if (!needle || !*needle) return;
    gchar* low = g_ascii_strdown(needle, -1);
    g_hash_table_add(p->blocklist, low);
}

int wkb_profile_load_blocklist(WkbProfile* p, const char* path, GError** err)
{
    gchar* contents = NULL;
    gsize len = 0;
    if (!g_file_get_contents(path, &contents, &len, err))
        return 0;

    int added = 0;
    char** lines = g_strsplit(contents, "\n", -1);
    for (char** l = lines; l && *l; l++) {
        char* s = g_strstrip(*l);
        if (!*s || *s == '#') continue;
        char* hash = strchr(s, '#');
        if (hash) *hash = '\0';
        s = g_strstrip(s);
        if (*s) { wkb_profile_block_domain(p, s); added++; }
    }
    g_strfreev(lines);
    g_free(contents);
    return added;
}

// Substring match is intentional: it catches subdomains without shipping a
// public-suffix list.
gboolean wkb_profile_uri_blocked(WkbProfile* p, const char* uri)
{
    if (!uri || g_hash_table_size(p->blocklist) == 0) return FALSE;
    gchar* low = g_ascii_strdown(uri, -1);
    gboolean hit = FALSE;
    GHashTableIter it;
    gpointer k;
    g_hash_table_iter_init(&it, p->blocklist);
    while (g_hash_table_iter_next(&it, &k, NULL)) {
        if (strstr(low, (const char*)k)) { hit = TRUE; break; }
    }
    g_free(low);
    return hit;
}

/* --------------------------------------------- WebKit resource interception */

static const struct { const char* ext; WkbAssetKind kind; } s_assets[] = {
    // images
    { ".png",  WKB_ASSET_IMAGE }, { ".jpg",  WKB_ASSET_IMAGE },
    { ".jpeg", WKB_ASSET_IMAGE }, { ".gif",  WKB_ASSET_IMAGE },
    { ".webp", WKB_ASSET_IMAGE }, { ".avif", WKB_ASSET_IMAGE },
    { ".svg",  WKB_ASSET_IMAGE }, { ".ico",  WKB_ASSET_IMAGE },
    { ".bmp",  WKB_ASSET_IMAGE },
    // fonts
    { ".woff",  WKB_ASSET_FONT }, { ".woff2", WKB_ASSET_FONT },
    { ".ttf",   WKB_ASSET_FONT }, { ".otf",   WKB_ASSET_FONT },
    { ".eot",   WKB_ASSET_FONT },
    // media
    { ".mp4",  WKB_ASSET_MEDIA }, { ".webm", WKB_ASSET_MEDIA },
    { ".mp3",  WKB_ASSET_MEDIA }, { ".m4a",  WKB_ASSET_MEDIA },
    { ".ogg",  WKB_ASSET_MEDIA }, { ".wav",  WKB_ASSET_MEDIA },
    { ".avi",  WKB_ASSET_MEDIA }, { ".mov",  WKB_ASSET_MEDIA },
    // stylesheets
    { ".css",  WKB_ASSET_STYLESHEET },
};

// Classify by URI extension. Strips query/fragment first so
// "logo.png?v=3" still matches.
static WkbAssetKind classify_uri(const char* uri)
{
    if (!uri) return WKB_ASSET_NONE;

    // Skip the scheme's authority and the path separators, we only want the
    // last path component.
    const char* path = strstr(uri, "://");
    path = path ? strchr(path + 3, '/') : uri;
    if (!path) return WKB_ASSET_NONE;

    char* tail = g_ascii_strdown(path, -1);
    char* q = strpbrk(tail, "?#");
    if (q) *q = '\0';

    WkbAssetKind kind = WKB_ASSET_NONE;
    for (size_t i = 0; i < G_N_ELEMENTS(s_assets); i++) {
        size_t el = strlen(s_assets[i].ext);
        size_t tl = strlen(tail);
        if (tl >= el && strcmp(tail + tl - el, s_assets[i].ext) == 0) {
            kind = s_assets[i].kind;
            break;
        }
    }
    g_free(tail);
    return kind;
}

const char* wkb_asset_kind_name(WkbAssetKind k)
{
    switch (k) {
    case WKB_ASSET_IMAGE:     return "image";
    case WKB_ASSET_FONT:      return "font";
    case WKB_ASSET_MEDIA:     return "media";
    case WKB_ASSET_STYLESHEET:return "stylesheet";
    default:                  return "none";
    }
}

// Decides whether a subresource load should be cancelled. Wired to
// WebKitWebView::resource-load-started; returning TRUE drops the request.
gboolean wkb_should_block_resource(WkbProfile* p, const char* uri,
                                   gboolean is_navigation)
{
    // Never block the top-level document: it carries the content.
    if (is_navigation) { p->allowed_count++; return FALSE; }

    // Script and XHR bodies are never blocked by extension. Anything without a
    // recognisable asset extension falls through to the allow path.
    if (wkb_profile_uri_blocked(p, uri)) {
        p->blocked_count++;
        return TRUE;
    }

    WkbAssetKind kind = classify_uri(uri);
    if ((p->block_assets && (kind == WKB_ASSET_IMAGE ||
                             kind == WKB_ASSET_FONT ||
                             kind == WKB_ASSET_MEDIA)) ||
        (p->block_stylesheets && kind == WKB_ASSET_STYLESHEET)) {
        p->blocked_count++;
        return TRUE;
    }

    p->allowed_count++;
    return FALSE;
}

long wkb_profile_blocked_count(WkbProfile* p) { return p->blocked_count; }
long wkb_profile_allowed_count(WkbProfile* p) { return p->allowed_count; }

// Honours the ephemeral/persist choice.
WebKitWebContext* wkb_profile_make_context(WkbProfile* p)
{
    if (p->ephemeral) {
        // Ephemeral session: cookies and website data live only as long as
        // this context and never touch disk between runs.
        return webkit_web_context_new_ephemeral();
    }
    return webkit_web_context_new();
}