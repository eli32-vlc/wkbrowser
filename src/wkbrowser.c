// wkbrowser CLI: headless WebKit automation browser (WebKitGTK + Xvfb).
//
// Usage:
//   wkbrowser --url URL [--dump] [--probe] [profile and network options]
//
// Runs with no visible display. Xvfb supplies the X display WebKitGTK needs.
#include "wkb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static GMainLoop* loop = NULL;
static int dump_mode = 0, probe_mode = 0;
static WkbProfile* profile = NULL;

static gboolean quit_loop(gpointer)
{
    if (loop && g_main_loop_is_running(loop)) g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void on_js(WebKitWebView* v, GAsyncResult* res, gpointer data)
{
    (void)data;
    GError* error = NULL;
    JSCValue* val = webkit_web_view_evaluate_javascript_finish(v, res, &error);
    if (error) { g_printerr("[wkb] JS error: %s\n", error->message); g_error_free(error); }
    else if (val) {
        char* s = jsc_value_to_string(val);
        g_print("%s\n", s ? s : "");
        g_free(s);
        g_clear_object(&val);
    }
    quit_loop(NULL);
}

static const char* DUMP_JS = "document.body ? document.body.innerText : ''";

static gboolean on_load_changed(WebKitWebView* v, WebKitLoadEvent ev, gpointer)
{
    if (ev != WEBKIT_LOAD_FINISHED) return TRUE;
    g_print("[wkb] loaded %s\n", webkit_web_view_get_uri(v));
    webkit_web_view_evaluate_javascript(v, probe_mode ? wkb_probe_js() : DUMP_JS, -1,
                                       NULL, NULL, NULL, on_js, NULL);
    return TRUE;
}

static gboolean on_load_failed(WebKitWebView* v, WebKitLoadEvent ev,
    const gchar* failing_uri, GError* error, gpointer)
{
    (void)v; (void)ev;
    printf("LOAD_FAILED uri=%s error=%s\n", failing_uri ? failing_uri : "",
           error && error->message ? error->message : "");
    quit_loop(NULL);
    return TRUE;
}

static gboolean on_chooser(WebKitWebView* v, WebKitFileChooserRequest* req, gpointer data)
{
    return wkb_files_handle_chooser(req, (WkbProfile*)data);
}

// Cancel subresources we do not want. Returning TRUE drops the request.
// Note: WebKitGTK 2.50.6's request carries no resource type, so wkb_net
// classifies by URI extension instead. See the header comment there.
static gboolean on_resource_load(WebKitWebView* v, WebKitWebResource* resource,
                                 WebKitURIRequest* request, gpointer data)
{
    (void)v; (void)resource;
    const char* uri = webkit_uri_request_get_uri(request);
    return wkb_should_block_resource((WkbProfile*)data, uri, FALSE);
}

static gboolean timed_out(gpointer)
{
    printf("TIMEOUT\n");
    quit_loop(NULL);
    return G_SOURCE_REMOVE;
}

static void usage(void)
{
    printf(
    "wkbrowser --url URL [options]\n"
    "\n"
    "output:\n"
    "  --dump              print document.body.innerText\n"
    "  --probe             print the fingerprint vector set as JSON\n"
    "\n"
    "profile:\n"
    "  --file PATH         register PATH as the file the picker returns\n"
    "  --screen WxH        fake screen dimensions\n"
    "  --geo LAT,LON       fake geolocation\n"
    "  --cores N           fake navigator.hardwareConcurrency\n"
    "  --memory GB         fake navigator.deviceMemory\n"
    "  --ua STRING         fake navigator.userAgent\n"
    "  --tz STRING         fake Intl timezone\n"
    "\n"
    "network:\n"
    "  --block-assets      cancel images, fonts and media\n"
    "  --block-styles      also cancel stylesheets\n"
    "  --blocklist FILE    blocklist file (default share/wkb-blocklist.txt)\n"
    "  --no-blocklist      disable blocking entirely\n"
    "  --block DOMAIN      block one domain substring (repeatable)\n"
    "  --persist           keep cookies/storage instead of an ephemeral session\n"
    "\n"
    "misc:\n"
    "  --stats             print blocked/allowed resource counts on exit\n"
    "  --timeout SEC       navigation timeout (default 45)\n");
}

int main(int argc, char** argv)
{
    const char* url = NULL;
    const char* blocklist = "share/wkb-blocklist.txt";
    int use_blocklist = 1, want_stats = 0, timeout = 45;
    GPtrArray* extra_blocks = g_ptr_array_new();

    profile = wkb_profile_new();

    #define NEXT() (i + 1 < argc ? argv[++i] : NULL)
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if (!strcmp(a, "--url")) url = NEXT();
        else if (!strcmp(a, "--dump")) dump_mode = 1;
        else if (!strcmp(a, "--probe")) probe_mode = 1;
        else if (!strcmp(a, "--file")) wkb_profile_set_default_file(profile, NEXT());
        else if (!strcmp(a, "--ua")) wkb_profile_set_user_agent(profile, NEXT());
        else if (!strcmp(a, "--tz")) wkb_profile_set_timezone(profile, NEXT());
        else if (!strcmp(a, "--block-assets")) wkb_profile_set_block_assets(profile, TRUE);
        else if (!strcmp(a, "--block-styles")) wkb_profile_set_block_stylesheets(profile, TRUE);
        else if (!strcmp(a, "--no-blocklist")) use_blocklist = 0;
        else if (!strcmp(a, "--blocklist")) blocklist = NEXT();
        else if (!strcmp(a, "--block")) g_ptr_array_add(extra_blocks, (gpointer)NEXT());
        else if (!strcmp(a, "--persist")) wkb_profile_set_ephemeral(profile, FALSE);
        else if (!strcmp(a, "--stats")) want_stats = 1;
        else if (!strcmp(a, "--timeout")) timeout = atoi(NEXT());
        else if (!strcmp(a, "--screen")) {
            int w = 0, h = 0; const char* v = NEXT();
            if (v && sscanf(v, "%dx%d", &w, &h) == 2)
                wkb_profile_set_screen(profile, w, h, w, h, 24, 1.0);
            else { fprintf(stderr, "bad --screen\n"); return 2; }
        } else if (!strcmp(a, "--geo")) {
            double lat, lon; const char* v = NEXT();
            if (v && sscanf(v, "%lf,%lf", &lat, &lon) == 2)
                wkb_profile_set_geolocation(profile, lat, lon, 0, 10);
            else { fprintf(stderr, "bad --geo\n"); return 2; }
        } else if (!strcmp(a, "--cores")) wkb_profile_set_hardware(profile, atoi(NEXT()), 0);
        else if (!strcmp(a, "--memory")) wkb_profile_set_hardware(profile, 0, atof(NEXT()));
        else if (!strcmp(a, "--help") || !strcmp(a, "-h")) { usage(); return 0; }
        else { fprintf(stderr, "unknown arg: %s\n", a); return 2; }
    }

    if (!url) { fprintf(stderr, "--url required\n"); usage(); return 2; }

    if (use_blocklist) {
        GError* err = NULL;
        int n = wkb_profile_load_blocklist(profile, blocklist, &err);
        if (err) {
            fprintf(stderr, "[wkb] blocklist %s: %s\n", blocklist, err->message);
            g_error_free(err);
            g_clear_error(&err);
        } else {
            fprintf(stderr, "[wkb] loaded %d blocklist entries from %s\n", n, blocklist);
        }
    }
    for (guint k = 0; k < extra_blocks->len; k++)
        wkb_profile_block_domain(profile, (const char*)g_ptr_array_index(extra_blocks, k));
    g_ptr_array_free(extra_blocks, TRUE);

    gtk_init(NULL, NULL);   // needs an X display; Xvfb provides it

    WebKitWebContext* ctx = wkb_profile_make_context(profile);
    if (!ctx) { fprintf(stderr, "[wkb] no web context\n"); return 3; }

    WebKitWebView* view = webkit_web_view_new_with_context(ctx);
    if (!view) { fprintf(stderr, "[wkb] no web view\n"); return 3; }

    WebKitSettings* s = webkit_web_view_get_settings(view);
    webkit_settings_set_enable_developer_extras(s, FALSE);
    webkit_settings_set_enable_write_console_messages_to_stdout(s, TRUE);

    wkb_profile_apply(profile, view);

    g_signal_connect(view, "load-changed", G_CALLBACK(on_load_changed), NULL);
    g_signal_connect(view, "load-failed", G_CALLBACK(on_load_failed), NULL);
    g_signal_connect(view, "run-file-chooser", G_CALLBACK(on_chooser), profile);
    g_signal_connect(view, "resource-load-started", G_CALLBACK(on_resource_load), profile);

    g_timeout_add_seconds(timeout, timed_out, NULL);

    loop = g_main_loop_new(NULL, FALSE);
    fprintf(stderr, "[wkb] %s navigation %s\n",
            profile->ephemeral ? "ephemeral" : "persistent", url);
    webkit_web_view_load_uri(view, url);
    g_main_loop_run(loop);

    if (want_stats)
        fprintf(stderr, "[wkb] resources: blocked=%ld allowed=%ld\n",
                wkb_profile_blocked_count(profile),
                wkb_profile_allowed_count(profile));

    wkb_profile_free(profile);
    return 0;
}