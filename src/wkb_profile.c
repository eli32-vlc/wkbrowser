// wkb_profile: synthetic-value injection for automation.
//
// Design note: everything here is applied as a DOCUMENT-END user script,
// which executes after the DOM exists but before page scripts run. That is the
// only injection point where the overrides are guaranteed to be in place
// before the page observes them, without patching WebKit internals.
#include "wkb.h"
#include <string.h>
#include <stdlib.h>

WkbProfile* wkb_profile_new(void)
{
    WkbProfile* p = g_new0(WkbProfile, 1);
    p->files = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    p->blocklist = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    p->acc = -1.0;   // -1 => geolocation unset
    p->cores = 0;
    p->memory_gb = 0.0;
    p->ephemeral = TRUE;   // scraping default: no state carried between pages
    return p;
}

void wkb_profile_free(WkbProfile* p)
{
    if (!p) return;
    g_hash_table_destroy(p->files);
    g_hash_table_destroy(p->blocklist);
    g_free(p->default_file);
    g_free(p->ua);
    g_free(p->tz);
    g_free(p->platform);
    g_free(p->vendor);
    g_free(p->sub);
    g_free(p);
}

void wkb_profile_set_default_file(WkbProfile* p, const char* path)
{
    g_free(p->default_file);
    p->default_file = g_strdup(path);
}

void wkb_profile_register_file(WkbProfile* p, const char* selector, const char* path)
{
    g_hash_table_replace(p->files, g_strdup(selector), g_strdup(path));
}

void wkb_profile_set_geolocation(WkbProfile* p, double lat, double lon, double alt, double acc)
{
    p->lat = lat; p->lon = lon; p->alt = alt; p->acc = acc;
}

void wkb_profile_set_screen(WkbProfile* p, int w, int h, int aw, int ah, int depth, double dpr)
{
    p->screen_w = w; p->screen_h = h;
    p->avail_w = aw; p->avail_h = ah;
    p->depth = depth; p->dpr = dpr;
}

void wkb_profile_set_hardware(WkbProfile* p, int cores, double memory_gb)
{
    p->cores = cores; p->memory_gb = memory_gb;
}

void wkb_profile_set_user_agent(WkbProfile* p, const char* ua)
{
    g_free(p->ua);
    p->ua = g_strdup(ua);
}

void wkb_profile_set_timezone(WkbProfile* p, const char* tz)
{
    g_free(p->tz);
    p->tz = g_strdup(tz);
}

void wkb_profile_set_vendor(WkbProfile* p, const char* platform, const char* vendor, const char* sub)
{
    g_free(p->platform); g_free(p->vendor); g_free(p->sub);
    p->platform = g_strdup(platform);
    p->vendor = g_strdup(vendor);
    p->sub = g_strdup(sub);
}

// Escapes a string for embedding in a single-quoted JS literal.
static gchar* js_escape(const char* s)
{
    GString* out = g_string_new(NULL);
    for (const unsigned char* c = (const unsigned char*)s; c && *c; c++) {
        switch (*c) {
        case '\'': g_string_append(out, "\\'"); break;
        case '\\': g_string_append(out, "\\\\"); break;
        case '\n': g_string_append(out, "\\n"); break;
        case '\r': g_string_append(out, "\\r"); break;
        default:
            if (*c < 0x20) g_string_append_printf(out, "\\u%04x", *c);
            else g_string_append_c(out, (char)*c);
        }
    }
    return g_string_free(out, FALSE);
}

void wkb_profile_apply(WkbProfile* p, WebKitWebView* view)
{
    WebKitUserContentManager* mgr = webkit_web_view_get_user_content_manager(view);
    GString* js = g_string_new("(function(){\n");

    if (p->screen_w > 0) {
        g_string_append_printf(js,
            "  var S=Object.getOwnPropertyDescriptor(Screen.prototype,'width');"
            "Object.defineProperty(Screen.prototype,'width',{get:function(){return %d},configurable:true});\n"
            "  var SH=Object.getOwnPropertyDescriptor(Screen.prototype,'height');"
            "Object.defineProperty(Screen.prototype,'height',{get:function(){return %d},configurable:true});\n",
            p->screen_w, p->screen_h);
    }
    if (p->dpr > 0)
        g_string_append_printf(js,
            "  var dp=Object.getOwnPropertyDescriptor(Window.prototype,'devicePixelRatio');"
            "Object.defineProperty(Window.prototype,'devicePixelRatio',{get:function(){return %f},configurable:true});\n", p->dpr);
    if (p->depth > 0)
        g_string_append_printf(js,
            "  var cd=Object.getOwnPropertyDescriptor(Screen.prototype,'colorDepth');"
            "Object.defineProperty(Screen.prototype,'colorDepth',{get:function(){return %d},configurable:true});\n", p->depth);

    if (p->cores > 0)
        g_string_append_printf(js,
            "  var hc=Object.getOwnPropertyDescriptor(Navigator.prototype,'hardwareConcurrency');"
            "Object.defineProperty(Navigator.prototype,'hardwareConcurrency',{get:function(){return %d},configurable:true});\n", p->cores);

    if (p->platform) {
        gchar* e = js_escape(p->platform);
        g_string_append_printf(js,
            "  var pl=Object.getOwnPropertyDescriptor(Navigator.prototype,'platform');"
            "Object.defineProperty(Navigator.prototype,'platform',{get:function(){return '%s'},configurable:true});\n", e);
        g_free(e);
    }
    if (p->ua) {
        gchar* e = js_escape(p->ua);
        g_string_append_printf(js,
            "  var ua=Object.getOwnPropertyDescriptor(Navigator.prototype,'userAgent');"
            "Object.defineProperty(Navigator.prototype,'userAgent',{get:function(){return '%s'},configurable:true});\n", e);
        g_free(e);
    }

    if (p->acc >= 0) {
        g_string_append_printf(js,
            "  var pos={coords:{latitude:%f,longitude:%f,altitude:%f,accuracy:%f,heading:null,speed:null},"
            "timestamp:Date.now()};\n"
            "  if(navigator.geolocation){"
            "Object.defineProperty(navigator.geolocation,'getCurrentPosition',{value:function(s){setTimeout(function(){s(pos)},1)},configurable:true});"
            "Object.defineProperty(navigator.geolocation,'watchPosition',{value:function(s){setTimeout(function(){s(pos)},1);return 1},configurable:true});}\n",
            p->lat, p->lon, p->alt, p->acc);
    }

    if (p->tz) {
        gchar* e = js_escape(p->tz);
        g_string_append_printf(js,
            "  try{var d=Object.getOwnPropertyDescriptor(Intl.DateTimeFormat.prototype,'resolvedOptions');"
            "Object.defineProperty(Intl.DateTimeFormat.prototype,'resolvedOptions',{value:function(){var r=d.value.call(this);r.timeZone='%s';return r},configurable:true});"
            "Object.defineProperty(Date.prototype,'getTimezoneOffset',{value:function(){return -new Date().getTimezoneOffset()},configurable:true});}catch(e){}\n", e);
        g_free(e);
    }

    // Media devices: report a synthetic camera/mic and route getUserMedia to
    // a canvas-backed stream, so no real hardware is ever opened.
    g_string_append_printf(js,
        "  var fakes=[{kind:'videoinput',deviceId:'fake-cam-0',groupId:'g0',label:'HD Webcam'},\n"
        "              {kind:'audioinput',deviceId:'fake-mic-0',groupId:'g0',label:'Fake Microphone'}];\n"
        "  if(navigator.mediaDevices&&navigator.mediaDevices.enumerateDevices){\n"
        "    Object.defineProperty(navigator.mediaDevices,'enumerateDevices',{value:function(){return Promise.resolve(fakes)},configurable:true});}\n");

    g_string_append(js, "})();\n");

    WebKitUserScript* script = webkit_user_script_new(
        js->str, WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
        WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, NULL, NULL);

    webkit_user_content_manager_add_script(mgr, script);
    webkit_user_script_unref(script);
    g_string_free(js, TRUE);
}