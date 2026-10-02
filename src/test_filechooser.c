#include "wkb.h"
#include <stdio.h>
static GMainLoop* loop=NULL;
static void report(WebKitWebView*v,GAsyncResult*r,gpointer d){(void)d;
  GError*e=NULL; JSCValue*val=webkit_web_view_evaluate_javascript_finish(v,r,&e);
  if(e){printf("JS ERROR: %s\n",e->message);}
  else if(val){char*s=jsc_value_to_string(val);printf("RESULT: %s\n",s?s:"");fflush(stdout);g_free(s);g_clear_object(&val);}
  /* do not quit here: later timers still need the loop alive */ }
static void run(WebKitWebView*v,const char*js){
  webkit_web_view_evaluate_javascript(v,js,-1,NULL,NULL,NULL,report,NULL);}
static gboolean click_file(gpointer d){WebKitWebView*v=d;
  run(v,"var i=document.getElementById('f');i.click();'clicked'");return G_SOURCE_REMOVE;}
static gboolean submit(gpointer d){WebKitWebView*v=d;
  run(v,"document.getElementById('f').form.submit();'submitted'");return G_SOURCE_REMOVE;}
static gboolean check(gpointer d){WebKitWebView*v=d;
  run(v,"var i=document.getElementById('f');JSON.stringify({count:i.files.length,name:i.files[0]?i.files[0].name:null,size:i.files[0]?i.files[0].size:null})");return G_SOURCE_REMOVE;}
static gboolean finish(gpointer d){(void)d;if(loop&&g_main_loop_is_running(loop))g_main_loop_quit(loop);return G_SOURCE_REMOVE;}
static gboolean on_load(WebKitWebView*v,WebKitLoadEvent ev,gpointer){
  printf("[t7] load event %d\n",(int)ev);
  if(ev!=WEBKIT_LOAD_FINISHED)return TRUE;
  printf("[t7] page loaded\n");
  g_timeout_add(300,click_file,v);   // trigger the picker
  g_timeout_add(1500,check,v);       // assert files[] BEFORE submitting
  g_timeout_add(4000,submit,v);      // then upload
  g_timeout_add(9000,finish,v);      // and hold the loop until it lands
  return TRUE;}
static gboolean on_chooser(WebKitWebView*v,WebKitFileChooserRequest*req,gpointer p){
  printf("[t7] run-file-chooser fired (NO dialog)\n");
  return wkb_files_handle_chooser(req,(WkbProfile*)p);}
static gboolean to(gpointer){printf("[t7] TIMEOUT\n");if(loop&&g_main_loop_is_running(loop))g_main_loop_quit(loop);return G_SOURCE_REMOVE;}
int main(int c,char**v){gtk_init(NULL,NULL);
  WkbProfile*p=wkb_profile_new();wkb_profile_set_default_file(p,v[1]);
  WebKitWebView*wv=webkit_web_view_new_with_context(webkit_web_context_new());
  wkb_profile_apply(p,wv);
  g_signal_connect(wv,"load-changed",G_CALLBACK(on_load),NULL);
  g_signal_connect(wv,"run-file-chooser",G_CALLBACK(on_chooser),p);
  g_timeout_add_seconds(30,to,NULL);
  loop=g_main_loop_new(NULL,FALSE);
  webkit_web_view_load_uri(wv,v[2]);
  g_main_loop_run(loop);return 0;}
