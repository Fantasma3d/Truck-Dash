/* TruckDash - ponto de entrada Android (NativeActivity, API 9+), sem Java e sem NDK.
 * Thread de render: desenha no ANativeWindow por software e trata toques. */
#include "app.h"
#include "plat_sys.h"

/* ---------- declaracoes minimas do <android/...> ---------- */
typedef struct ANativeWindow ANativeWindow;
typedef struct AInputQueue AInputQueue;
typedef struct AInputEvent AInputEvent;
typedef struct ALooper ALooper;
typedef struct ARect { int32_t left, top, right, bottom; } ARect;
typedef void *JNIEnvTable;          /* const struct JNINativeInterface* */
typedef JNIEnvTable *JNIEnv;
typedef void *jobject;
typedef void *jmethodID;
typedef struct ANativeActivity ANativeActivity;
typedef struct ANativeActivityCallbacks {
    void (*onStart)(ANativeActivity *);
    void (*onResume)(ANativeActivity *);
    void *(*onSaveInstanceState)(ANativeActivity *, size_t *);
    void (*onPause)(ANativeActivity *);
    void (*onStop)(ANativeActivity *);
    void (*onDestroy)(ANativeActivity *);
    void (*onWindowFocusChanged)(ANativeActivity *, int);
    void (*onNativeWindowCreated)(ANativeActivity *, ANativeWindow *);
    void (*onNativeWindowResized)(ANativeActivity *, ANativeWindow *);
    void (*onNativeWindowRedrawNeeded)(ANativeActivity *, ANativeWindow *);
    void (*onNativeWindowDestroyed)(ANativeActivity *, ANativeWindow *);
    void (*onInputQueueCreated)(ANativeActivity *, AInputQueue *);
    void (*onInputQueueDestroyed)(ANativeActivity *, AInputQueue *);
    void (*onContentRectChanged)(ANativeActivity *, const ARect *);
    void (*onConfigurationChanged)(ANativeActivity *);
    void (*onLowMemory)(ANativeActivity *);
} ANativeActivityCallbacks;
struct ANativeActivity {
    ANativeActivityCallbacks *callbacks;
    void *vm;
    JNIEnv *env;
    jobject clazz;
    const char *internalDataPath;
    const char *externalDataPath;
    int32_t sdkVersion;
    void *instance;
    void *assetManager;
    const char *obbPath;
};
typedef struct ANativeWindow_Buffer {
    int32_t width, height, stride, format;
    void *bits;
    uint32_t reserved[6];
} ANativeWindow_Buffer;

int32_t ANativeWindow_setBuffersGeometry(ANativeWindow *, int32_t, int32_t, int32_t);
int32_t ANativeWindow_lock(ANativeWindow *, ANativeWindow_Buffer *, ARect *);
int32_t ANativeWindow_unlockAndPost(ANativeWindow *);
void ANativeWindow_acquire(ANativeWindow *);
void ANativeWindow_release(ANativeWindow *);
ALooper *ALooper_prepare(int);
int ALooper_pollAll(int, int *, int *, void **);
void AInputQueue_attachLooper(AInputQueue *, ALooper *, int, void *, void *);
void AInputQueue_detachLooper(AInputQueue *);
int32_t AInputQueue_getEvent(AInputQueue *, AInputEvent **);
int32_t AInputQueue_preDispatchEvent(AInputQueue *, AInputEvent *);
void AInputQueue_finishEvent(AInputQueue *, AInputEvent *, int);
int32_t AInputEvent_getType(const AInputEvent *);
int32_t AMotionEvent_getAction(const AInputEvent *);
int32_t AKeyEvent_getKeyCode(const AInputEvent *);
void ANativeActivity_setWindowFlags(ANativeActivity *, uint32_t, uint32_t);
int __android_log_print(int, const char *, const char *, ...);

#define WINDOW_FORMAT_RGBA_8888 1
#define WINDOW_FORMAT_RGBX_8888 2
#define WINDOW_FORMAT_RGB_565 4
#define AINPUT_EVENT_TYPE_KEY 1
#define AINPUT_EVENT_TYPE_MOTION 2
#define AWINDOW_FLAG_KEEP_SCREEN_ON 0x80
#define AWINDOW_FLAG_FULLSCREEN 0x400
#define LOGI(...) __android_log_print(4, "TruckDash", __VA_ARGS__)

/* ---------- estado ---------- */
static pthread_mutex_t g_m;
static ANativeWindow *g_window;
static AInputQueue *g_queue;
static ALooper *g_looper;
static volatile int g_visible;
static int g_started;
static uint32_t *g_tmp;            /* buffer 8888 quando a janela e RGB565 */
static int g_tmp_n;

static void process_input(void) {
    AInputEvent *ev;
    if (!g_queue) return;
    while (AInputQueue_getEvent(g_queue, &ev) >= 0) {
        if (AInputQueue_preDispatchEvent(g_queue, ev)) continue;
        int handled = 0;
        if (AInputEvent_getType(ev) == AINPUT_EVENT_TYPE_MOTION) {
            static double down_t;
            int act = AMotionEvent_getAction(ev) & 0xff;
            if (act == 0 /* DOWN */) down_t = now_s();
            else if (act == 1 /* UP */) app_tap(now_s() - down_t > 0.7);
            handled = 1;
        }
        /* teclas (ex.: voltar) ficam com o sistema */
        AInputQueue_finishEvent(g_queue, ev, handled);
    }
}

static void render_once(void) {
    ANativeWindow_Buffer buf;
    if (ANativeWindow_lock(g_window, &buf, 0) != 0) return;
    surf_t fb;
    if (buf.format == WINDOW_FORMAT_RGB_565) {
        int n = buf.width * buf.height;
        if (n > g_tmp_n) { free(g_tmp); g_tmp = (uint32_t *)malloc((size_t)n * 4); g_tmp_n = g_tmp ? n : 0; }
        if (g_tmp) {
            fb.px = g_tmp; fb.w = buf.width; fb.h = buf.height; fb.stride = buf.width;
            app_frame(&fb);
            for (int y = 0; y < buf.height; y++) {
                uint16_t *d = (uint16_t *)buf.bits + y * buf.stride;
                const uint32_t *s = g_tmp + y * buf.width;
                for (int x = 0; x < buf.width; x++) {
                    uint32_t p = s[x];
                    d[x] = (uint16_t)(((p & 0xf8) << 8) | ((p >> 5) & 0x7e0) | ((p >> 19) & 0x1f));
                }
            }
        }
    } else {
        fb.px = (uint32_t *)buf.bits; fb.w = buf.width; fb.h = buf.height; fb.stride = buf.stride;
        app_frame(&fb);
    }
    ANativeWindow_unlockAndPost(g_window);
}

static void *render_thread(void *arg) {
    (void)arg;
    pthread_mutex_lock(&g_m);
    g_looper = ALooper_prepare(1 /* ALLOW_NON_CALLBACKS */);
    pthread_mutex_unlock(&g_m);
    ANativeWindow *configured = 0;
    for (;;) {
        double t0 = now_s();
        int fd, events; void *data;
        while (ALooper_pollAll(0, &fd, &events, &data) >= 0) {
            pthread_mutex_lock(&g_m);
            process_input();
            pthread_mutex_unlock(&g_m);
        }
        pthread_mutex_lock(&g_m);
        if (g_window && g_visible) {
            if (configured != g_window) {
                ANativeWindow_setBuffersGeometry(g_window, 0, 0, WINDOW_FORMAT_RGBX_8888);
                configured = g_window;
            }
            render_once();
        } else if (!g_window) configured = 0;
        pthread_mutex_unlock(&g_m);
        double el = now_s() - t0;
        double target = g_visible ? 1.0 / 30.0 : 0.2;
        if (el < target) usleep((unsigned long)((target - el) * 1e6));
    }
    return 0;
}

/* esconde/escurece a barra do sistema via JNI: getWindow().getDecorView().setSystemUiVisibility() */
static void hide_system_ui(ANativeActivity *a) {
    JNIEnv *env = a->env;
    void **fn = (void **)*env;
    jobject (*GetObjectClass)(JNIEnv *, jobject) = fn[31];
    jmethodID (*GetMethodID)(JNIEnv *, jobject, const char *, const char *) = fn[33];
    jobject (*CallObjectMethod)(JNIEnv *, jobject, jmethodID, ...) = fn[34];
    void (*CallVoidMethod)(JNIEnv *, jobject, jmethodID, ...) = fn[61];
    void (*DeleteLocalRef)(JNIEnv *, jobject) = fn[23];
    void (*ExceptionClear)(JNIEnv *) = fn[17];
    unsigned char (*ExceptionCheck)(JNIEnv *) = fn[228];

    jobject cls = GetObjectClass(env, a->clazz);
    jmethodID mGetWindow = GetMethodID(env, cls, "getWindow", "()Landroid/view/Window;");
    if (ExceptionCheck(env) || !mGetWindow) { ExceptionClear(env); return; }
    jobject win = CallObjectMethod(env, a->clazz, mGetWindow);
    if (!win) return;
    jobject wcls = GetObjectClass(env, win);
    jmethodID mDecor = GetMethodID(env, wcls, "getDecorView", "()Landroid/view/View;");
    if (ExceptionCheck(env) || !mDecor) { ExceptionClear(env); return; }
    jobject view = CallObjectMethod(env, win, mDecor);
    if (!view) return;
    jobject vcls = GetObjectClass(env, view);
    jmethodID mSet = GetMethodID(env, vcls, "setSystemUiVisibility", "(I)V");
    if (ExceptionCheck(env) || !mSet) { ExceptionClear(env); return; }
    int flags = 1; /* LOW_PROFILE: funciona do 4.0 em diante */
    if (a->sdkVersion >= 19) flags = 0x1000 | 0x400 | 0x200 | 0x100 | 4 | 2 | 1; /* imersivo (4.4+) */
    CallVoidMethod(env, view, mSet, flags);
    if (ExceptionCheck(env)) ExceptionClear(env);
    DeleteLocalRef(env, vcls); DeleteLocalRef(env, view); DeleteLocalRef(env, wcls);
    DeleteLocalRef(env, win); DeleteLocalRef(env, cls);
}

/* ---------- callbacks (thread principal) ---------- */
static void onResume(ANativeActivity *a) { (void)a; g_visible = 1; }
static void onPause(ANativeActivity *a) { (void)a; g_visible = 0; }
static void onFocus(ANativeActivity *a, int has) { if (has) hide_system_ui(a); }
static void onWinCreated(ANativeActivity *a, ANativeWindow *w) {
    (void)a;
    pthread_mutex_lock(&g_m);
    ANativeWindow_acquire(w);
    g_window = w;
    pthread_mutex_unlock(&g_m);
}
static void onWinDestroyed(ANativeActivity *a, ANativeWindow *w) {
    (void)a;
    pthread_mutex_lock(&g_m);
    if (g_window == w) g_window = 0;
    ANativeWindow_release(w);
    pthread_mutex_unlock(&g_m);
}
static void onQueueCreated(ANativeActivity *a, AInputQueue *q) {
    (void)a;
    pthread_mutex_lock(&g_m);
    g_queue = q;
    if (g_looper) AInputQueue_attachLooper(q, g_looper, 1, 0, 0);
    pthread_mutex_unlock(&g_m);
}
static void onQueueDestroyed(ANativeActivity *a, AInputQueue *q) {
    (void)a;
    pthread_mutex_lock(&g_m);
    if (g_looper) AInputQueue_detachLooper(q);
    if (g_queue == q) g_queue = 0;
    pthread_mutex_unlock(&g_m);
}

__attribute__((visibility("default")))
void ANativeActivity_onCreate(ANativeActivity *a, void *saved, size_t saved_size) {
    (void)saved; (void)saved_size;
    ANativeActivityCallbacks *cb = a->callbacks;
    cb->onResume = onResume;
    cb->onPause = onPause;
    cb->onWindowFocusChanged = onFocus;
    cb->onNativeWindowCreated = onWinCreated;
    cb->onNativeWindowDestroyed = onWinDestroyed;
    cb->onInputQueueCreated = onQueueCreated;
    cb->onInputQueueDestroyed = onQueueDestroyed;
    ANativeActivity_setWindowFlags(a, AWINDOW_FLAG_KEEP_SCREEN_ON | AWINDOW_FLAG_FULLSCREEN, 0);
    if (!g_started) {
        /* a thread de render e a rede vivem pelo processo inteiro */
        pthread_t th;
        g_started = 1;
        pthread_mutex_init(&g_m, 0);
        app_init(a->internalDataPath, "/sdcard");
        pthread_create(&th, 0, render_thread, 0);
        pthread_detach(th);
        LOGI("TruckDash iniciado (sdk %d)", a->sdkVersion);
        /* espera o looper existir para poder ligar a fila de toques */
        for (int i = 0; i < 100; i++) {
            pthread_mutex_lock(&g_m);
            int ok = g_looper != 0;
            pthread_mutex_unlock(&g_m);
            if (ok) break;
            usleep(5000);
        }
    }
}
