// Null view backend for WPE: satisfies the wpe_view_backend_interface that
// WPEWebKit requires, presents nothing, and runs with no display server.
// This is the piece that makes headless WPE possible on 2.38.x (which predates
// the built-in wpe-display-headless backend).
#include <wpe/wpe.h>
#include <stdlib.h>
#include <string.h>

struct null_backend {
    struct wpe_view_backend* backend;
    int renderer_host_fd;
    // Fixed viewport; JS reads window.innerWidth/Height through this.
    int width;
    int height;
};

static void* backend_create(void* data, struct wpe_view_backend* backend)
{
    (void)data;
    struct null_backend* b = calloc(1, sizeof(*b));
    if (!b) return NULL;
    b->backend = backend;
    b->width = 1280;
    b->height = 800;
    b->renderer_host_fd = -1;
    return b;
}

static void backend_destroy(void* data)
{
    free(data);
}

static void backend_initialize(void* data)
{
    (void)data;
}

static int backend_get_renderer_host_fd(void* data)
{
    struct null_backend* b = data;
    return b->renderer_host_fd;
}

static void nop0(void* d) { (void)d; }
static void nop1(void* d) { (void)d; }
static void nop2(void* d) { (void)d; }
static void nop3(void* d) { (void)d; }

static struct wpe_view_backend_interface s_interface = {
    .create = backend_create,
    .destroy = backend_destroy,
    .initialize = backend_initialize,
    .get_renderer_host_fd = backend_get_renderer_host_fd,
    ._wpe_reserved0 = nop0,
    ._wpe_reserved1 = nop1,
    ._wpe_reserved2 = nop2,
    ._wpe_reserved3 = nop3,
};

// Returns a fresh null view backend, or NULL on OOM.
struct wpe_view_backend* null_view_backend_create(void)
{
    return wpe_view_backend_create_with_backend_interface(&s_interface, NULL);
}