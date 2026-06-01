// SPDX-License-Identifier: MIT
//
// A minimal CLAP audio-effect plugin whose UI is an HTML page rendered by
// WebKit2GTK and embedded into the host window via X11.
//
// Embedding strategy: create a plain GtkWindow (POPUP, no decorations), realize
// it to get an X11 window, then XReparentWindow it as a child of the host's
// parent X11 window. This avoids GtkPlug's strict XEmbed handshake requirement
// (GtkPlug assumes the parent is a GtkSocket / XEmbed-compliant widget, which
// a plain Qt QWindow is not).
//
// Purpose: stress-test a CLAP host's GUI embedding against an out-of-process
// webview-based plugin UI. The DSP is a trivial stereo gain controlled by a
// single CLAP parameter, also exposed to JS via webkit_user_content_manager
// so the HTML slider drives the parameter live.

#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <clap/clap.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <webkit2/webkit2.h>
#include <X11/Xlib.h>

namespace {

constexpr uint32_t kInitialWidth = 480;
constexpr uint32_t kInitialHeight = 320;
constexpr uint32_t kMinWidth = 320;
constexpr uint32_t kMinHeight = 200;
constexpr uint32_t kGtkPumpIntervalMs = 16; // ~60 Hz

const char* const kFeatures[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    CLAP_PLUGIN_FEATURE_UTILITY,
    nullptr,
};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION_INIT,
    "audacity.clap.webview-demo",
    "WebView Demo",
    "Audacity",
    "https://github.com/audacity/audacity",
    "",
    "",
    "1.0.0",
    "A CLAP gain plugin whose UI is an HTML page rendered by WebKit2GTK.",
    kFeatures,
};

const char* const kHtml = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><title>WebView Demo</title><style>
  :root { color-scheme: dark; }
  html, body { margin: 0; padding: 0; height: 100%; }
  body { font-family: system-ui, sans-serif; background: #1e1e2e; color: #cdd6f4; padding: 20px; box-sizing: border-box; }
  h1 { font-size: 18px; margin: 0 0 8px; color: #89b4fa; }
  .banner { background: #313244; padding: 10px 12px; border-radius: 6px; font-size: 13px; line-height: 1.4; }
  .banner code { background: #45475a; padding: 1px 5px; border-radius: 3px; }
  .row { display: flex; align-items: center; gap: 10px; margin: 18px 0 6px; }
  label { width: 50px; }
  input[type=range] { flex: 1; }
  .v { width: 60px; text-align: right; font-family: ui-monospace, monospace; color: #f9e2af; }
  .meta { font-size: 11px; color: #6c7086; margin-top: 14px; }
</style></head><body>
  <h1>CLAP WebView Demo</h1>
  <div class="banner">This UI is HTML rendered by <b>WebKit2GTK</b>, embedded inside Audacity through CLAP's <code>clap.gui</code> X11 / XEmbed protocol.</div>
  <div class="row"><label>Gain</label><input id="g" type="range" min="0" max="1" step="0.001" value="0.5"><span id="v" class="v">0.500</span></div>
  <div class="meta">Slider posts to <code>window.webkit.messageHandlers.audacity</code>; the plugin reads the value into a CLAP parameter and applies it to the audio stream.</div>
<script>
  const g = document.getElementById('g'), v = document.getElementById('v');
  function post(x) {
    v.textContent = parseFloat(x).toFixed(3);
    if (window.webkit && window.webkit.messageHandlers && window.webkit.messageHandlers.audacity) {
      window.webkit.messageHandlers.audacity.postMessage(parseFloat(x));
    }
  }
  g.addEventListener('input', () => post(g.value));
  // Receive value updates from the host (for state load / automation).
  window.setGain = (x) => { g.value = x; v.textContent = parseFloat(x).toFixed(3); };
</script></body></html>)HTML";

struct Demo {
    clap_plugin_t plugin {};
    const clap_host_t* host = nullptr;

    // Audio
    double sample_rate = 44100.0;
    std::atomic<float> gain { 0.5f };

    // Host extensions we may use
    const clap_host_timer_support_t* host_timer = nullptr;
    const clap_host_gui_t* host_gui = nullptr;
    clap_id timer_id = static_cast<clap_id>(-1);

    // GUI (GTK3)
    GtkWidget* plug = nullptr;     // GtkPlug
    GtkWidget* webview = nullptr;  // WebKitWebView
    unsigned long parent_window = 0;
    bool gui_created = false;
};

// ----- helpers --------------------------------------------------------------

void apply_param_events(Demo* d, const clap_input_events_t* in_events)
{
    if (!in_events || !in_events->size) {
        return;
    }
    const uint32_t n = in_events->size(in_events);
    for (uint32_t i = 0; i < n; ++i) {
        const clap_event_header_t* hdr = in_events->get(in_events, i);
        if (!hdr || hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) {
            continue;
        }
        if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
            const auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
            if (ev->param_id == 0) {
                d->gain.store(static_cast<float>(ev->value), std::memory_order_relaxed);
            }
        }
    }
}

void on_js_message(WebKitUserContentManager*, WebKitJavascriptResult* result, gpointer user_data)
{
    auto* d = static_cast<Demo*>(user_data);
    JSCValue* v = webkit_javascript_result_get_js_value(result);
    if (v && jsc_value_is_number(v)) {
        const double x = jsc_value_to_double(v);
        d->gain.store(static_cast<float>(x), std::memory_order_relaxed);
    }
}

void on_load_changed(WebKitWebView* view, WebKitLoadEvent ev, gpointer user_data)
{
    const char* name = "?";
    switch (ev) {
        case WEBKIT_LOAD_STARTED: name = "started"; break;
        case WEBKIT_LOAD_REDIRECTED: name = "redirected"; break;
        case WEBKIT_LOAD_COMMITTED: name = "committed"; break;
        case WEBKIT_LOAD_FINISHED: name = "finished"; break;
    }
    std::fprintf(stderr, "[clap-webview-demo] webkit load-changed: %s\n", name);
    if (ev == WEBKIT_LOAD_FINISHED) {
        auto* d = static_cast<Demo*>(user_data);
        // Force a redraw of both the webview and its container after the page is
        // ready; without this the freshly-painted page may sit on the offscreen
        // surface until the next X11 expose event we don't get under a foreign
        // toplevel.
        gtk_widget_queue_draw(GTK_WIDGET(view));
        if (d->plug) {
            gtk_widget_queue_draw(d->plug);
        }
    }
}

gboolean on_web_process_terminated(WebKitWebView*, WebKitWebProcessTerminationReason reason, gpointer)
{
    const char* name = "?";
    switch (reason) {
        case WEBKIT_WEB_PROCESS_CRASHED:                  name = "crashed"; break;
        case WEBKIT_WEB_PROCESS_EXCEEDED_MEMORY_LIMIT:    name = "exceeded-memory-limit"; break;
        case WEBKIT_WEB_PROCESS_TERMINATED_BY_API:        name = "terminated-by-api"; break;
    }
    std::fprintf(stderr, "[clap-webview-demo] webkit WebProcess terminated: %s\n", name);
    return FALSE;
}

// ----- audio-ports ----------------------------------------------------------

uint32_t ap_count(const clap_plugin_t*, bool) { return 1; }

bool ap_get(const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info)
{
    if (index != 0) {
        return false;
    }
    info->id = 0;
    std::snprintf(info->name, sizeof(info->name), "%s", is_input ? "in" : "out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = CLAP_INVALID_ID;
    return true;
}

const clap_plugin_audio_ports_t s_audio_ports = { ap_count, ap_get };

// ----- params ---------------------------------------------------------------

uint32_t pa_count(const clap_plugin_t*) { return 1; }

bool pa_get_info(const clap_plugin_t*, uint32_t index, clap_param_info_t* info)
{
    if (index != 0) {
        return false;
    }
    info->id = 0;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE | CLAP_PARAM_REQUIRES_PROCESS;
    info->cookie = nullptr;
    info->min_value = 0.0;
    info->max_value = 1.0;
    info->default_value = 0.5;
    std::snprintf(info->name, sizeof(info->name), "Gain");
    info->module[0] = '\0';
    return true;
}

bool pa_get_value(const clap_plugin_t* plugin, clap_id id, double* value)
{
    if (id != 0) {
        return false;
    }
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    *value = d->gain.load(std::memory_order_relaxed);
    return true;
}

bool pa_value_to_text(const clap_plugin_t*, clap_id, double value, char* out, uint32_t cap)
{
    std::snprintf(out, cap, "%.3f", value);
    return true;
}

bool pa_text_to_value(const clap_plugin_t*, clap_id, const char* text, double* out)
{
    char* end = nullptr;
    *out = std::strtod(text, &end);
    return end != text;
}

void pa_flush(const clap_plugin_t* plugin, const clap_input_events_t* in, const clap_output_events_t*)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    apply_param_events(d, in);
}

const clap_plugin_params_t s_params = {
    pa_count, pa_get_info, pa_get_value, pa_value_to_text, pa_text_to_value, pa_flush
};

// ----- state ----------------------------------------------------------------

bool st_save(const clap_plugin_t* plugin, const clap_ostream_t* stream)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    double g = d->gain.load();
    return stream->write(stream, &g, sizeof(g)) == static_cast<int64_t>(sizeof(g));
}

bool st_load(const clap_plugin_t* plugin, const clap_istream_t* stream)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    double g = 0.5;
    if (stream->read(stream, &g, sizeof(g)) != static_cast<int64_t>(sizeof(g))) {
        return false;
    }
    d->gain.store(static_cast<float>(g));
    return true;
}

const clap_plugin_state_t s_state = { st_save, st_load };

// ----- timer-support: pump GTK every ~16 ms ---------------------------------

void on_timer(const clap_plugin_t* plugin, clap_id)
{
    // Drain pending GTK events (non-blocking).
    while (g_main_context_iteration(nullptr, FALSE)) {
        // keep draining
    }
    // Force a synchronous paint. queue_draw schedules a paint idle, but on a
    // reparented-into-foreign-toplevel GtkWindow that path doesn't reliably
    // fire across destroy/recreate cycles. invalidate_region + process_updates
    // forces the draw pipeline to run NOW.
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    if (d && d->plug && gtk_widget_get_realized(d->plug)) {
        GdkWindow* gw = gtk_widget_get_window(d->plug);
        if (gw) {
            GtkAllocation a;
            gtk_widget_get_allocation(d->plug, &a);
            cairo_rectangle_int_t rect = { 0, 0, a.width, a.height };
            cairo_region_t* r = cairo_region_create_rectangle(&rect);
            gdk_window_invalidate_region(gw, r, TRUE);
            gdk_window_process_updates(gw, TRUE);
            cairo_region_destroy(r);
        }
    }
}

const clap_plugin_timer_support_t s_timer = { on_timer };

// ----- GUI ------------------------------------------------------------------

bool gui_is_api_supported(const clap_plugin_t*, const char* api, bool is_floating)
{
    return !is_floating && api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
}

bool gui_get_preferred_api(const clap_plugin_t*, const char** api, bool* is_floating)
{
    *api = CLAP_WINDOW_API_X11;
    *is_floating = false;
    return true;
}

bool gui_create(const clap_plugin_t* plugin, const char* api, bool is_floating)
{
    if (is_floating || !api || std::strcmp(api, CLAP_WINDOW_API_X11) != 0) {
        return false;
    }
    auto* d = static_cast<Demo*>(plugin->plugin_data);

    // Force GTK to use the X11 backend, even when running under a Wayland session.
    // entry_init() also sets GDK_BACKEND=x11 in the environment, which is the
    // strongest guarantee since it's processed by GdkDisplay creation. This
    // call is a belt-and-suspenders fallback.
    gdk_set_allowed_backends("x11");

    if (!gtk_init_check(nullptr, nullptr)) {
        std::fprintf(stderr, "[clap-webview-demo] gtk_init_check failed\n");
        return false;
    }

    if (GdkDisplay* disp = gdk_display_get_default()) {
        std::fprintf(stderr, "[clap-webview-demo] GdkDisplay backend: %s\n",
                     G_OBJECT_TYPE_NAME(disp));
    } else {
        std::fprintf(stderr, "[clap-webview-demo] no default GdkDisplay\n");
        return false;
    }

    d->host_gui = static_cast<const clap_host_gui_t*>(d->host->get_extension(d->host, CLAP_EXT_GUI));
    d->host_timer = static_cast<const clap_host_timer_support_t*>(
        d->host->get_extension(d->host, CLAP_EXT_TIMER_SUPPORT));

    // Register a periodic timer with the host to pump GTK's main loop.
    if (d->host_timer && d->host_timer->register_timer) {
        d->host_timer->register_timer(d->host, kGtkPumpIntervalMs, &d->timer_id);
    }

    d->gui_created = true;
    return true;
}

void gui_destroy(const clap_plugin_t* plugin)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    std::fprintf(stderr, "[clap-webview-demo] gui_destroy\n");
    if (d->host_timer && d->timer_id != static_cast<clap_id>(-1)) {
        d->host_timer->unregister_timer(d->host, d->timer_id);
        d->timer_id = static_cast<clap_id>(-1);
    }
    if (d->plug) {
        // The plug owns the webview child; destroying the plug destroys both.
        gtk_widget_destroy(d->plug);
        d->plug = nullptr;
        d->webview = nullptr;
    }
    d->parent_window = 0;
    d->gui_created = false;
    // Drain any pending GTK / GDK events that referenced the just-destroyed
    // widgets, so they don't haunt the next gui_create cycle.
    int drained = 0;
    while (g_main_context_iteration(nullptr, FALSE)) {
        if (++drained > 100) { break; }
    }
    std::fprintf(stderr, "[clap-webview-demo] gui_destroy drained %d events\n", drained);
}

bool gui_set_scale(const clap_plugin_t*, double) { return false; }

bool gui_get_size(const clap_plugin_t*, uint32_t* w, uint32_t* h)
{
    // Always report the constant initial size. Querying the widget's allocation
    // is unreliable: when the widget hasn't been laid out by GTK yet, the
    // allocation is 1x1 and reporting that to the host shrinks the dialog.
    *w = kInitialWidth;
    *h = kInitialHeight;
    return true;
}

bool gui_can_resize(const clap_plugin_t*) { return true; }

bool gui_get_resize_hints(const clap_plugin_t*, clap_gui_resize_hints_t* hints)
{
    hints->can_resize_horizontally = true;
    hints->can_resize_vertically = true;
    hints->preserve_aspect_ratio = false;
    hints->aspect_ratio_width = 0;
    hints->aspect_ratio_height = 0;
    return true;
}

bool gui_adjust_size(const clap_plugin_t*, uint32_t* w, uint32_t* h)
{
    if (*w < kMinWidth) {
        *w = kMinWidth;
    }
    if (*h < kMinHeight) {
        *h = kMinHeight;
    }
    return true;
}

bool gui_set_size(const clap_plugin_t* plugin, uint32_t w, uint32_t h)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    if (!d->plug) {
        return false;
    }
    gtk_window_resize(GTK_WINDOW(d->plug), static_cast<int>(w), static_cast<int>(h));
    return true;
}

bool gui_set_parent(const clap_plugin_t* plugin, const clap_window_t* window)
{
    if (!window->api || std::strcmp(window->api, CLAP_WINDOW_API_X11) != 0) {
        return false;
    }
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    d->parent_window = window->x11;

    std::fprintf(stderr, "[clap-webview-demo] set_parent: parent X11 window = 0x%lx\n",
                 d->parent_window);

    if (d->plug) {
        std::fprintf(stderr, "[clap-webview-demo] already created\n");
        return true;
    }

    // GtkPlug would be the "XEmbed" option, but it strictly requires the
    // parent to be a GtkSocket / XEmbed-compliant widget. Audacity's QWindow
    // is just a raw X11 window, so we follow the same pattern DPF / pugl
    // use: create a borderless top-level, then XReparentWindow it manually
    // into the host's window.
    d->plug = gtk_window_new(GTK_WINDOW_POPUP);
    if (!d->plug) {
        std::fprintf(stderr, "[clap-webview-demo] gtk_window_new failed\n");
        return false;
    }
    gtk_window_set_decorated(GTK_WINDOW(d->plug), FALSE);
    gtk_widget_set_size_request(d->plug, kInitialWidth, kInitialHeight);
    // Critical: a GtkWindow's underlying X11 window is created at 1x1 unless
    // its default size is set. set_size_request is only a *minimum* and isn't
    // applied to the X11 window at realize time.
    gtk_window_set_default_size(GTK_WINDOW(d->plug), kInitialWidth, kInitialHeight);

    // Diagnostic: if WEBVIEW_DEMO_LABEL_ONLY=1, replace the webview with a
    // plain GtkLabel + bright red background. This isolates whether GTK can
    // paint at all into our reparented X11 child (vs the WebKitWebView
    // specifically failing to deliver its rendered pixels).
    const bool labelOnly = std::getenv("WEBVIEW_DEMO_LABEL_ONLY") != nullptr;
    if (labelOnly) {
        std::fprintf(stderr, "[clap-webview-demo] WEBVIEW_DEMO_LABEL_ONLY=1: using GtkLabel\n");
        GtkWidget* eventbox = gtk_event_box_new();
        gtk_widget_set_size_request(eventbox, kInitialWidth, kInitialHeight);

        GdkRGBA red = { 1.0, 0.0, 0.0, 1.0 };
        gtk_widget_override_background_color(eventbox, GTK_STATE_FLAG_NORMAL, &red);

        GtkWidget* label = gtk_label_new("GTK label inside the embedded X11 window. If you see this in red,\nGTK rendering after XReparent works fine.");
        gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
        gtk_container_add(GTK_CONTAINER(eventbox), label);

        // Diagnostic: count how many times GTK actually invokes the draw
        // signal on the eventbox. Fresh open should yield > 0; reopen
        // currently produces black, so we want to see whether the count is 0
        // (no paint triggered) or > 0 (paint triggered but pixels missed).
        g_signal_connect(eventbox, "draw",
            G_CALLBACK(+[](GtkWidget* w, cairo_t*, gpointer) -> gboolean {
                static int n = 0;
                ++n;
                if (n <= 5 || n % 30 == 0) {
                    GtkAllocation a;
                    gtk_widget_get_allocation(w, &a);
                    std::fprintf(stderr, "[clap-webview-demo] draw #%d size=%dx%d\n",
                                 n, a.width, a.height);
                }
                return FALSE; // let GTK keep drawing
            }), nullptr);

        d->webview = eventbox;  // store in same field for cleanup
        gtk_widget_set_hexpand(d->webview, TRUE);
        gtk_widget_set_vexpand(d->webview, TRUE);
        gtk_container_add(GTK_CONTAINER(d->plug), d->webview);

        gtk_widget_realize(d->plug);
        GdkWindow* gdkwin0 = gtk_widget_get_window(d->plug);
        GdkDisplay* gdkdisp0 = gdk_window_get_display(gdkwin0);
        Display* xdpy0 = gdk_x11_display_get_xdisplay(gdkdisp0);
        Window xwid0 = gdk_x11_window_get_xid(gdkwin0);
        auto dump0 = [&](const char* tag) {
            XSync(xdpy0, False);
            XWindowAttributes a;
            XGetWindowAttributes(xdpy0, xwid0, &a);
            std::fprintf(stderr, "[clap-webview-demo] (label) %-22s -> %dx%d+%d+%d map=%d\n",
                         tag, a.width, a.height, a.x, a.y, a.map_state);
        };
        dump0("after realize");
        GdkWindow* host_gdkwin0 = gdk_x11_window_foreign_new_for_display(
            gdkdisp0, static_cast<Window>(d->parent_window));
        if (!host_gdkwin0) {
            std::fprintf(stderr, "[clap-webview-demo] (label) foreign_new failed\n");
            return false;
        }
        gdk_window_reparent(gdkwin0, host_gdkwin0, 0, 0);
        dump0("after reparent");
        gtk_widget_show_all(d->plug);
        dump0("after show_all");
        GtkAllocation alloc0 = { 0, 0, kInitialWidth, kInitialHeight };
        gtk_widget_size_allocate(d->plug, &alloc0);
        dump0("after size_allocate");
        gdk_window_move_resize(gdkwin0, 0, 0, kInitialWidth, kInitialHeight);
        dump0("after gdk_move_resize");
        XMoveResizeWindow(xdpy0, xwid0, 0, 0, kInitialWidth, kInitialHeight);
        XSync(xdpy0, False);
        dump0("after X_move_resize");
        return true;
    }

    d->webview = webkit_web_view_new();

    // Disable hardware acceleration. WebKit2GTK 2.50+ defaults to a DMA-BUF /
    // EGL-based renderer whose presentation surface does not survive being
    // reparented under a foreign-owned (Qt) X11 toplevel: the page paints into
    // an offscreen surface that never reaches the visible window. Falling back
    // to software rendering paints straight into our GdkWindow.
    {
        WebKitSettings* settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(d->webview));
        webkit_settings_set_hardware_acceleration_policy(
            settings, WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    }

    // Match the HTML body's background so there's no flash before the page paints.
    {
        GdkRGBA bg = { 30.0 / 255.0, 30.0 / 255.0, 46.0 / 255.0, 1.0 }; // #1e1e2e
        webkit_web_view_set_background_color(WEBKIT_WEB_VIEW(d->webview), &bg);
    }

    // Ensure the webview has a non-zero allocation. gtk_container_add by itself
    // is not enough on every container: the child may end up at 0x0 if the
    // toplevel's allocation cycle is interrupted by the X reparent we do later.
    gtk_widget_set_size_request(d->webview, kInitialWidth, kInitialHeight);
    gtk_widget_set_hexpand(d->webview, TRUE);
    gtk_widget_set_vexpand(d->webview, TRUE);

    gtk_container_add(GTK_CONTAINER(d->plug), d->webview);

    // Diagnostics: trace WebKit lifecycle so we can tell whether the WebProcess
    // actually starts and the HTML actually loads.
    g_signal_connect(d->webview, "load-changed", G_CALLBACK(on_load_changed), d);
    g_signal_connect(d->webview, "web-process-terminated",
                     G_CALLBACK(on_web_process_terminated), d);

    // Hook a JS->host channel ("audacity") so the slider drives the gain param.
    WebKitUserContentManager* mgr
        = webkit_web_view_get_user_content_manager(WEBKIT_WEB_VIEW(d->webview));
    webkit_user_content_manager_register_script_message_handler(mgr, "audacity");
    g_signal_connect(mgr, "script-message-received::audacity",
                     G_CALLBACK(on_js_message), d);

    webkit_web_view_load_html(WEBKIT_WEB_VIEW(d->webview), kHtml, nullptr);

    // Realize as a normal toplevel, then move the GdkWindow under the host's
    // X11 window via GDK's reparent. After show, force an explicit size
    // allocation so GTK paints into the full region (otherwise the popup's
    // allocation stays at 1x1 because the (foreign) WM doesn't size it).
    gtk_widget_realize(d->plug);
    GdkWindow* gdkwin = gtk_widget_get_window(d->plug);
    if (!gdkwin) {
        std::fprintf(stderr, "[clap-webview-demo] realize: no GdkWindow\n");
        return false;
    }
    GdkDisplay* gdkdisp = gdk_window_get_display(gdkwin);
    GdkWindow* host_gdkwin = gdk_x11_window_foreign_new_for_display(
        gdkdisp, static_cast<Window>(d->parent_window));
    if (!host_gdkwin) {
        std::fprintf(stderr, "[clap-webview-demo] foreign_new_for_display failed\n");
        return false;
    }
    gdk_window_reparent(gdkwin, host_gdkwin, 0, 0);
    std::fprintf(stderr, "[clap-webview-demo] gdk-reparented\n");

    gtk_widget_show_all(d->plug);
    // Force an explicit allocation + X11 size on the now-child window.
    GtkAllocation alloc = { 0, 0, kInitialWidth, kInitialHeight };
    gtk_widget_size_allocate(d->plug, &alloc);
    gdk_window_move_resize(gdkwin, 0, 0, kInitialWidth, kInitialHeight);
    std::fprintf(stderr, "[clap-webview-demo] shown + size_allocate\n");
    return true;
}

bool gui_set_transient(const clap_plugin_t*, const clap_window_t*) { return false; }
void gui_suggest_title(const clap_plugin_t*, const char*) {}

bool gui_show(const clap_plugin_t* plugin)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    if (d->plug) {
        gtk_widget_show_all(d->plug);
    }
    return true;
}

bool gui_hide(const clap_plugin_t* plugin)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    if (d->plug) {
        gtk_widget_hide(d->plug);
    }
    return true;
}

const clap_plugin_gui_t s_gui = {
    gui_is_api_supported,
    gui_get_preferred_api,
    gui_create,
    gui_destroy,
    gui_set_scale,
    gui_get_size,
    gui_can_resize,
    gui_get_resize_hints,
    gui_adjust_size,
    gui_set_size,
    gui_set_parent,
    gui_set_transient,
    gui_suggest_title,
    gui_show,
    gui_hide,
};

// ----- plugin lifecycle -----------------------------------------------------

bool plug_init(const clap_plugin_t*) { return true; }

void plug_destroy(const clap_plugin_t* plugin)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    if (d) {
        delete d;
    }
}

bool plug_activate(const clap_plugin_t* plugin, double sr, uint32_t, uint32_t)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    d->sample_rate = sr;
    return true;
}

void plug_deactivate(const clap_plugin_t*) {}
bool plug_start_processing(const clap_plugin_t*) { return true; }
void plug_stop_processing(const clap_plugin_t*) {}
void plug_reset(const clap_plugin_t*) {}

clap_process_status plug_process(const clap_plugin_t* plugin, const clap_process_t* proc)
{
    auto* d = static_cast<Demo*>(plugin->plugin_data);
    apply_param_events(d, proc->in_events);

    if (proc->audio_inputs_count == 0 || proc->audio_outputs_count == 0) {
        return CLAP_PROCESS_CONTINUE;
    }

    const float gain = d->gain.load(std::memory_order_relaxed);

    const auto& in = proc->audio_inputs[0];
    auto& out = proc->audio_outputs[0];
    const uint32_t nch
        = (in.channel_count < out.channel_count) ? in.channel_count : out.channel_count;

    for (uint32_t c = 0; c < nch; ++c) {
        const float* ic = in.data32[c];
        float* oc = out.data32[c];
        for (uint32_t s = 0; s < proc->frames_count; ++s) {
            oc[s] = ic[s] * gain;
        }
    }
    return CLAP_PROCESS_CONTINUE;
}

const void* plug_get_extension(const clap_plugin_t*, const char* id)
{
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) {
        return &s_audio_ports;
    }
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &s_params;
    }
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) {
        return &s_state;
    }
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) {
        return &s_gui;
    }
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0) {
        return &s_timer;
    }
    return nullptr;
}

void plug_on_main_thread(const clap_plugin_t*) {}

// ----- factory --------------------------------------------------------------

uint32_t factory_get_plugin_count(const clap_plugin_factory_t*) { return 1; }

const clap_plugin_descriptor_t* factory_get_plugin_descriptor(const clap_plugin_factory_t*, uint32_t i)
{
    return i == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t* factory_create_plugin(const clap_plugin_factory_t*, const clap_host_t* host,
                                           const char* plugin_id)
{
    if (!plugin_id || std::strcmp(plugin_id, kDescriptor.id) != 0) {
        return nullptr;
    }
    auto* d = new Demo();
    d->host = host;
    d->plugin.desc = &kDescriptor;
    d->plugin.plugin_data = d;
    d->plugin.init = plug_init;
    d->plugin.destroy = plug_destroy;
    d->plugin.activate = plug_activate;
    d->plugin.deactivate = plug_deactivate;
    d->plugin.start_processing = plug_start_processing;
    d->plugin.stop_processing = plug_stop_processing;
    d->plugin.reset = plug_reset;
    d->plugin.process = plug_process;
    d->plugin.get_extension = plug_get_extension;
    d->plugin.on_main_thread = plug_on_main_thread;
    return &d->plugin;
}

const clap_plugin_factory_t s_factory = {
    factory_get_plugin_count, factory_get_plugin_descriptor, factory_create_plugin
};

// ----- entry ----------------------------------------------------------------

bool entry_init(const char*)
{
    // Force GTK to use the X11 backend. CLAP's gui X11 embedding requires it,
    // and gdk_set_allowed_backends() in gui_create() is too late if anything
    // loaded earlier in this process already initialized a GdkDisplay. The
    // strongest fix is `GDK_BACKEND=x11` in the host environment before
    // Audacity launches; this setenv is a fallback.
    setenv("GDK_BACKEND", "x11", /*overwrite=*/ 1);

    // Force WebKit to use software (non-accelerated) compositing. Accelerated
    // (OpenGL/EGL) compositing often fails inside a reparented X11 child of a
    // foreign-owned (Qt) toplevel because WebKit's GL context setup expects
    // ownership of the X11 connection.
    setenv("WEBKIT_DISABLE_COMPOSITING_MODE", "1", 0);
    return true;
}
void entry_deinit() {}

const void* entry_get_factory(const char* factory_id)
{
    return (factory_id && std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) ? &s_factory : nullptr;
}

} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    entry_init,
    entry_deinit,
    entry_get_factory,
};
