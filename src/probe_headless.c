// t4 probe: can WPE 2.38.6 create a view, load a page and run JS with no
// DISPLAY / WAYLAND_DISPLAY?
//
// Design: the web process must not be launched until we have set the
// WebKitWebViewBackend, so the UI-process side never falls back to a
// dlopen()ed default backend. A one-shot idle callback performs the setup.
#include <wpe/webkit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GMainLoop* loop = NULL;
static const char* g_url = NULL;

static const char* script =
    "document.title + '|h1=' + (document.querySelector('h1')||{textContent:'none'}).textContent"
    " + '|ua=' + navigator.userAgent"
    " + " + navigator.webdriver
    " + '|hw=' + navigator.hardwareConcurrency"
    " + '|win=' + window.innerWidth + 'x' + window.innerHeight";

static gboolean quit_loop(gpointer)
{
    if (loop && g_main_loop_is_running(loop)) g_main_loop_quit(loop);
    return G_SOURCE_REMOVE;
}

static void on_js(WebKitWebView* v, GAsyncResult* res, gpointer data)
{
    (void)data;
    GError* error = NULL;
    WebKitJavascriptResult* r = webkit_web_view_run_javascript_finish(v, res, &error);
    if (error) { printf("JS ERROR: %s\n", error->message); g_error_free(error); }
    else if (r) {
        JSCValue* val = webkit_javascript_result_get_js_value(r);
        JSCContext* ctx = jsc_context_get_current();
        char* s = jsc_value_to_string(jsc_context_get_value(ctx, val));
        printf("JS: %s\n", s ? s : "(null)");
        g_free(s);
        webkit_javascript_result_unref(r);
    }
    quit_loop(NULL);
}

static gboolean on_load_changed(WebKitWebView* webview, WebKitLoadEvent event, void*)
{
    if (event == WEBKIT_LOAD_FINISHED) {
        printf("load: uri=%s title=%s\n",
            webkit_web_view_get_uri(webview), webkit_web_view_get_title(webview));
        webkit_web_view_run_javascript(webview, script, NULL, on_js, NULL);
    }
    return TRUE;
}

static gboolean on_load_failed(WebKitWebView* webview, WebKitLoadEvent event,
    const gchar* failing_uri, GError* error, void*)
{
    (void)webview; (void)event;
    printf("LOAD FAILED: uri=%s error=%s\n", failing_uri ? failing_uri : "(null)",
        error && error->message ? error->message : "(none)");
    quit_loop(NULL);
    return TRUE;
}



static gboolean timed_out(gpointer)
{
    printf("TIMEOUT: no completion within 25s\n");
    quit_loop(NULL);
    return G_SOURCE_REMOVE;
}

static gboolean setup(gpointer unused)
{
    (void)unused;
    printf("UI process: creating web view with null view backend\n");

    WebKitWebContext* ctx = webkit_web_context_new();
    if (!ctx) { printf("FAIL: no web context\n"); quit_loop(NULL); return G_SOURCE_REMOVE; }

    // Headless: present nothing. See null_view_backend.c.
    extern struct wpe_view_backend* null_view_backend_create(void);
    struct wpe_view_backend* vb = null_view_backend_create();
    if (!vb) { printf("FAIL: no view backend\n"); quit_loop(NULL); return G_SOURCE_REMOVE; }
    WebKitWebViewBackend* backend = webkit_web_view_backend_new(vb, NULL, NULL);
    if (!backend) { printf("FAIL: no webview backend\n"); quit_loop(NULL); return G_SOURCE_REMOVE; }

    WebKitWebView* webview = webkit_web_view_new_with_context(backend, ctx);
    if (!webview) { printf("FAIL: no web view\n"); quit_loop(NULL); return G_SOURCE_REMOVE; }

    g_signal_connect(webview, "load-changed", G_CALLBACK(on_load_changed), NULL);
    g_signal_connect(webview, "load-failed", G_CALLBACK(on_load_failed), NULL);
    g_timeout_add_seconds(25, timed_out, NULL);

    // Probe JS before navigating so no page content is required.
    printf("pre-nav JS probe...\n");
    webkit_web_view_run_javascript(webview, script, NULL, on_js, NULL);

    printf("navigating: %s\n", g_url);
    webkit_web_view_load_uri(webview, g_url);
    return G_SOURCE_REMOVE;
}

int main(int argc, char** argv)
{
    g_url = argc > 1 ? argv[1] : "data:text/html,<h1>hello</h1><title>T</title>";
    printf("DISPLAY='%s' WAYLAND_DISPLAY='%s'\n",
        getenv("DISPLAY") ? getenv("DISPLAY") : "",
        getenv("WAYLAND_DISPLAY") ? getenv("WAYLAND_DISPLAY") : "");
    printf("engine version: %d.%d.%d\n",
        webkit_get_major_version(), webkit_get_minor_version(),
        webkit_get_micro_version());

    loop = g_main_loop_new(NULL, FALSE);
    g_idle_add(setup, NULL);
    g_main_loop_run(loop);
    printf("exited cleanly\n");
    return 0;
}