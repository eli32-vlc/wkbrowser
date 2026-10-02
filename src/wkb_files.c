// wkb_files: replaces the native file chooser with a registry lookup.
//
// WebKitGTK's "run-file-chooser" signal fires instead of opening a dialog.
// We resolve the request against the profile's registered paths and call
// webkit_file_chooser_request_select_files(). No GTK dialog is ever constructed.
#include "wkb.h"
#include <string.h>

// Returns the path to hand back for this request, or NULL to cancel.
static const char* resolve_file(WkbProfile* p, WebKitFileChooserRequest* req)
{
    // Selector matching by extension/accept types is the simple case; the
    // default file is the fallback.
    (void)req;
    if (p->default_file) return p->default_file;

    if (g_hash_table_size(p->files) > 0) {
        GList* keys = g_hash_table_get_keys(p->files);
        const char* first = g_hash_table_lookup(p->files, keys->data);
        g_list_free(keys);
        return first;
    }
    return NULL;
}

gboolean wkb_files_handle_chooser(WebKitFileChooserRequest* req, WkbProfile* profile)
{
    const char* path = resolve_file(profile, req);
    if (!path) {
        g_printerr("[wkb] no file registered; cancelling chooser\n");
        webkit_file_chooser_request_cancel(req);
        return TRUE;   // handled: we cancelled rather than showing UI
    }

    if (!g_file_test(path, G_FILE_TEST_EXISTS)) {
        g_printerr("[wkb] registered file does not exist: %s\n", path);
        webkit_file_chooser_request_cancel(req);
        return TRUE;
    }

    const char* files[2] = { path, NULL };
    webkit_file_chooser_request_select_files(req, files);
    g_print("[wkb] file chooser satisfied with %s\n", path);
    return TRUE;
}