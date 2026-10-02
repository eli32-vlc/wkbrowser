// wkbrowser CLI: headless WebKit automation browser (WebKitGTK + Xvfb).
//
// Usage:
//   wkbrowser --url URL [--file PATH] [--geo LAT,LON] [--screen WxH]
//             [--cores N] [--memory GB] [--ua STRING] [--tz STRING]
//             [--dump] [--probe]
//
// Runs with no visible display. Xvfb supplies the X display WebKitGTK needs.
#include "wkb.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>



static GMainLoop* loop = NULL;
static int dump_mode = 0, probe_mode = 0;

static gboolean quit_loop(gpointer)
{
    if (loop && g_main_loop_is_running(loop)) g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void on_js(WebKitWebView* v, GAsyncResult* res, gpointer data)
{
    (void)data;
    GError* error = NULL;
    // 2.50 renamed this to evaluate_javascript; _finish keeps the same shape.
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

static const char* DUMP_JS =
    "document.body ? document.body.innerText : ''";

static const char* PROBE_JS =
    "JSON.stringify({"
    "ua:navigator.userAgent,"
    "webdriver:navigator.webdriver,"
    "cores:navigator.hardwareConcurrency,"
    "platform:navigator.platform,"
    "screen:[screen.width,screen.height,screen.colorDepth],"
    "dpr:window.devicePixelRatio,"
    "tz:Intl.DateTimeFormat().resolvedOptions().timeZone,"
    "devices:navigator.mediaDevices?navigator.mediaDevices.enumerateDevices():'n/a'})";

static gboolean on_load_changed(WebKitWebView* v, WebKitLoadEvent ev, gpointer)
{
    if (ev != WEBKIT_LOAD_FINISHED) return TRUE;
    g_print("[wkb] loaded %s\n", webkit_web_view_get_uri(v));
    webkit_web_view_evaluate_javascript(v, probe_mode ? PROBE_JS : DUMP_JS, -1,
                                       NULL, NULL, NULL, on_js, NULL);
    return TRUE;
}

static gboolean on_chooser(WebKitWebView* v, WebKitFileChooserRequest* req, gpointer data)
{
    return wkb_files_handle_chooser(req, (WkbProfile*)data);
}

static gboolean timed_out(gpointer)
{
    g_printerr("[wkb] timeout\n");
    quit_loop(NULL);
    return G_SOURCE_REMOVE;
}

int main(int argc, char** argv)
{
    const char* url = NULL;
    WkbProfile* profile = wkb_profile_new();

    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        #define NEXT() (i + 1 < argc ? argv[++i] : NULL)
        if (!strcmp(a, "--url")) url = NEXT();
        else if (!strcmp(a, "--file")) wkb_profile_set_default_file(profile, NEXT());
        else if (!strcmp(a, "--ua")) wkb_profile_set_user_agent(profile, NEXT());
        else if (!strcmp(a, "--tz")) wkb_profile_set_timezone(profile, NEXT());
        else if (!strcmp(a, "--dump")) dump_mode = 1;
        else if (!strcmp(a, "--probe")) probe_mode = 1;
        else if (!strcmp(a, "--screen")) {
            int w = 0, h = 0; const char* v = NEXT();
            if (v && sscanf(v, "%dx%d", &w, &h) == 2) wkb_profile_set_screen(profile, w, h, w, h, 24, 1.0);
            else { g_printerr("bad --screen\n"); return 2; }
        } else if (!strcmp(a, "--geo")) {
            double lat, lon; const char* v = NEXT();
            if (v && sscanf(v, "%lf,%lf", &lat, &lon) == 2) wkb_profile_set_geolocation(profile, lat, lon, 0, 10);
            else { g_printerr("bad --geo\n"); return 2; }
        } else if (!strcmp(a, "--cores")) wkb_profile_set_hardware(profile, atoi(NEXT()), 0);
        else if (!strcmp(a, "--memory")) wkb_profile_set_hardware(profile, 0, atof(NEXT()));
        else if (!strcmp(a, "--help")) {
            printf("wkbrowser --url URL [--file PATH] [--geo LAT,LON] [--screen WxH]\n"
                   "          [--cores N] [--memory GB] [--ua STRING] [--tz STRING]\n"
                   "          [--dump] [--probe]\n");
            return 0;
        } else { g_printerr("unknown arg: %s\n", a); return 2; }
    }

    if (!url) { g_printerr("--url required\n"); return 2; }

    gtk_init(NULL, NULL);   // needs an X display; Xvfb provides it

    WebKitWebContext* ctx = webkit_web_context_new();
    WebKitWebView* view = webkit_web_view_new_with_context(ctx);
    if (!view) { g_printerr("[wkb] failed to create web view\n"); return 3; }

    // Kill features we don't want; keeps the profile coherent.
    WebKitSettings* s = webkit_web_view_get_settings(view);
    webkit_settings_set_enable_webgl(s, TRUE);
    webkit_settings_set_enable_developer_extras(s, FALSE);
    webkit_settings_set_enable_write_console_messages_to_stdout(s, TRUE);

    wkb_profile_apply(profile, view);
    g_signal_connect(view, "load-changed", G_CALLBACK(on_load_changed), NULL);
    g_signal_connect(view, "run-file-chooser", G_CALLBACK(on_chooser), profile);

    g_timeout_add_seconds(45, timed_out, NULL);
    loop = g_main_loop_new(NULL, FALSE);
    g_print("[wkb] navigating %s\n", url);
    webkit_web_view_load_uri(view, url);
    g_main_loop_run(loop);

    wkb_profile_free(profile);
    return 0;
}