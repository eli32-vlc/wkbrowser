// wkb_probe: the fingerprint vector set, emitted as JSON.
//
// This is the regression detector for everything in PHASE2.md. Any change to
// the fake-value layer or the network posture must be reviewed against
// tests/expected_fingerprint.json -- a silent shift here is a detection risk.
//
// Deliberately includes vectors we do NOT control (plugin count, pdfViewer,
// WebGL renderer) because their baseline value is part of the fingerprint.
#include "wkb.h"
#include <string.h>

static const char* s_probe =
"JSON.stringify({"
  "  ua: navigator.userAgent,"
  "  uaData: typeof navigator.userAgentData,"
  "  webdriver: navigator.webdriver,"
  "  cores: navigator.hardwareConcurrency,"
  "  memory: navigator.deviceMemory,"
  "  platform: navigator.platform,"
  "  vendor: navigator.vendor,"
  "  languages: (navigator.languages||[]).join(','),"
  "  screen: [screen.width, screen.height, screen.availWidth, screen.availHeight],"
  "  colorDepth: screen.colorDepth,"
  "  dpr: window.devicePixelRatio,"
  "  inner: [window.innerWidth, window.innerHeight],"
  "  tz: (function(){try{return Intl.DateTimeFormat().resolvedOptions().timeZone}catch(e){return 'ERR'}})(),"
  "  tzOffset: new Date().getTimezoneOffset(),"
  "  notif: (typeof Notification !== 'undefined') ? Notification.permission : 'absent',"
  "  plugins: navigator.plugins.length,"
  "  mimeTypes: navigator.mimeTypes.length,"
  "  pdfViewer: navigator.pdfViewerEnabled,"
  "  chromeObj: typeof window.chrome,"
  "  webgl: (function(){try{var c=document.createElement('canvas');"
  "    var g=c.getContext('webgl')||c.getContext('experimental-webgl');"
  "    if(!g) return 'none';"
  "    var d=g.getExtension('WEBGL_debug_renderer_info');"
  "    return d ? g.getParameter(d.UNMASKED_RENDERER_WEBGL)+' | '+g.getParameter(d.UNMASKED_VENDOR_WEBGL)"
  "             : g.getParameter(g.RENDERER);"
  "  }catch(e){return 'ERR:'+e.name}})(),"
  "  fnStr: String(Object.keys),"
  "  errStr: (function(){try{null.x}catch(e){return e.constructor.name}})(),"
  "  intl: (typeof Intl !== 'undefined' && typeof Intl.Segmenter !== 'undefined')"
  "})";

const char* wkb_probe_js(void)
{
    return s_probe;
}