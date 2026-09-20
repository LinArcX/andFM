#include <android/input.h>
#include <android/native_window.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <chrono>
#include <cstdio>
#include <cstring>

#include "android_native_app_glue.h"

#include "nanovg.h"
#include "nanovg_gl.h"

#include "oui-blendish/oui.h"
#include "oui-blendish/blendish.h"

struct App
{
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;

  NVGcontext *vg = nullptr;
  UIcontext *ui = nullptr;

  ANativeWindow *window = nullptr;

  int width = 0;
  int height = 0;

  bool running = true;
  bool checked = false;
  bool clicked = false;

  int buttonItem = -1;
};

static App g_app;

static long long getTimeMilliseconds()
{
  const auto now = std::chrono::steady_clock::now();

  return std::chrono::duration_cast<std::chrono::milliseconds>(
    now.time_since_epoch()).count();
}

static bool initEgl(ANativeWindow *window)
{
  g_app.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

  if (g_app.display == EGL_NO_DISPLAY)
    return false;

  if (!eglInitialize(g_app.display, nullptr, nullptr))
    return false;

  const EGLint configAttributes[] =
  {
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,

    EGL_RED_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_BLUE_SIZE, 8,
    EGL_ALPHA_SIZE, 8,

    EGL_NONE
  };

  EGLConfig config = nullptr;
  EGLint configCount = 0;

  if (!eglChooseConfig(
        g_app.display,
        configAttributes,
        &config,
        1,
        &configCount))
  {
    return false;
  }

  if (configCount != 1)
    return false;

  const EGLint contextAttributes[] =
  {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };

  g_app.context = eglCreateContext(
    g_app.display,
    config,
    EGL_NO_CONTEXT,
    contextAttributes);

  if (g_app.context == EGL_NO_CONTEXT)
    return false;

  g_app.surface = eglCreateWindowSurface(
    g_app.display,
    config,
    window,
    nullptr);

  if (g_app.surface == EGL_NO_SURFACE)
    return false;

  if (!eglMakeCurrent(
        g_app.display,
        g_app.surface,
        g_app.surface,
        g_app.context))
  {
    return false;
  }

  eglQuerySurface(
    g_app.display,
    g_app.surface,
    EGL_WIDTH,
    &g_app.width);

  eglQuerySurface(
    g_app.display,
    g_app.surface,
    EGL_HEIGHT,
    &g_app.height);

  g_app.vg = nvgCreateGLES2(
    NVG_ANTIALIAS | NVG_STENCIL_STROKES);

  if (!g_app.vg)
    return false;

  const int font = nvgCreateFont(
    g_app.vg,
    "default",
    "/system/fonts/Roboto-Regular.ttf");

  if (font < 0)
  {
    __android_log_print(
      ANDROID_LOG_ERROR,
      "MinimalUI",
      "Could not load Android Roboto font");

    return false;
  }

  bndSetFont(font);

  g_app.ui = uiCreateContext(
    256,
    16 * 1024);

  if (!g_app.ui)
    return false;

  uiMakeCurrent(g_app.ui);

  return true;
}

static void shutdownEgl()
{
  if (g_app.ui)
  {
    uiDestroyContext(g_app.ui);
    g_app.ui = nullptr;
  }

  if (g_app.vg)
  {
    nvgDeleteGLES2(g_app.vg);
    g_app.vg = nullptr;
  }

  if (g_app.display != EGL_NO_DISPLAY)
  {
    eglMakeCurrent(
      g_app.display,
      EGL_NO_SURFACE,
      EGL_NO_SURFACE,
      EGL_NO_CONTEXT);

    if (g_app.surface != EGL_NO_SURFACE)
    {
      eglDestroySurface(
        g_app.display,
        g_app.surface);
    }

    if (g_app.context != EGL_NO_CONTEXT)
    {
      eglDestroyContext(
        g_app.display,
        g_app.context);
    }

    eglTerminate(g_app.display);
  }

  g_app.display = EGL_NO_DISPLAY;
  g_app.surface = EGL_NO_SURFACE;
  g_app.context = EGL_NO_CONTEXT;
}

static void buttonHandler(
  int,
  UIevent event)
{
  if (event == UI_BUTTON0_HOT_UP)
    g_app.clicked = !g_app.clicked;
}

struct ButtonData
{
  const char *label;
  UIhandler handler;
};

static int createButton(
  const char *label)
{
  const int item = uiItem();

  uiSetSize(
    item,
    0,
    BND_WIDGET_HEIGHT);

  uiSetEvents(
    item,
    UI_BUTTON0_HOT_UP);

  ButtonData *data =
    static_cast<ButtonData *>(
      uiAllocHandle(
        item,
        sizeof(ButtonData)));

  data->label = label;
  data->handler = buttonHandler;

  return item;
}

static void uiHandler(
  int item,
  UIevent event)
{
  void *handle = uiGetHandle(item);

  if (!handle)
    return;

  ButtonData *data =
    static_cast<ButtonData *>(handle);

  if (data->handler)
    data->handler(item, event);
}

static void buildUi()
{
  uiBeginLayout();

  const int root = uiItem();

  uiSetSize(
    root,
    g_app.width,
    g_app.height);

  const int column =
    uiInsert(root, uiItem());

  uiSetBox(
    column,
    UI_COLUMN);

  uiSetLayout(
    column,
    UI_HFILL | UI_VFILL);

  uiSetMargins(
    column,
    24,
    24,
    24,
    24);

  {
    const int title = uiItem();

    uiSetSize(
      title,
      0,
      BND_WIDGET_HEIGHT);

    uiSetLayout(
      title,
      UI_HFILL);

    uiSetHandle(
      title,
      nullptr);

    uiInsert(column, title);
  }

  g_app.buttonItem =
    createButton("Click me");

  uiInsert(
    column,
    g_app.buttonItem);

  uiSetLayout(
    g_app.buttonItem,
    UI_HFILL);

  uiEndLayout();
}

static void drawItem(
  int item)
{
  const UIrect rect = uiGetRect(item);
  const UIitemState state = uiGetState(item);

  void *handle = uiGetHandle(item);

  if (item == g_app.buttonItem)
  {
    ButtonData *data =
      static_cast<ButtonData *>(handle);

    bndToolButton(
      g_app.vg,
      rect.x,
      rect.y,
      rect.w,
      rect.h,
      BND_CORNER_ALL,
      static_cast<BNDwidgetState>(state),
      -1,
      data ? data->label : "Click");

    if (g_app.clicked)
    {
      nvgFontSize(g_app.vg, 16.0f);
      nvgFontFace(g_app.vg, "default");
      nvgFillColor(
        g_app.vg,
        nvgRGB(220, 220, 220));

      nvgText(
        g_app.vg,
        rect.x,
        rect.y + rect.h + 28,
        "Clicked!",
        nullptr);
    }
  }

  const int child = uiFirstChild(item);

  if (child >= 0)
  {
    int current = child;

    while (current >= 0)
    {
      drawItem(current);
      current = uiNextSibling(current);
    }
  }
}

static void draw()
{
  glViewport(
    0,
    0,
    g_app.width,
    g_app.height);

  glClearColor(
    0.12f,
    0.12f,
    0.12f,
    1.0f);

  glClear(
    GL_COLOR_BUFFER_BIT |
    GL_STENCIL_BUFFER_BIT);

  nvgBeginFrame(
    g_app.vg,
    static_cast<float>(g_app.width),
    static_cast<float>(g_app.height),
    1.0f);

  nvgFontSize(
    g_app.vg,
    18.0f);

  nvgFontFace(
    g_app.vg,
    "default");

  nvgFillColor(
    g_app.vg,
    nvgRGB(220, 220, 220));

  nvgText(
    g_app.vg,
    24,
    24,
    "Minimal Android / C++17",
    nullptr);

  drawItem(0);

  nvgEndFrame(g_app.vg);

  eglSwapBuffers(
    g_app.display,
    g_app.surface);
}

static int32_t handleInput(
  struct android_app *,
  AInputEvent *event)
{
  if (AInputEvent_getType(event) !=
      AINPUT_EVENT_TYPE_MOTION)
  {
    return 0;
  }

  const int action =
    AMotionEvent_getAction(event);

  const int pointer =
    action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK;

  const int actionType =
    action & AMOTION_EVENT_ACTION_MASK;

  const float x =
    AMotionEvent_getX(event, pointer);

  const float y =
    AMotionEvent_getY(event, pointer);

  uiSetCursor(
    static_cast<int>(x),
    static_cast<int>(y));

  if (actionType ==
      AMOTION_EVENT_ACTION_DOWN)
  {
    uiSetButton(
      0,
      0,
      1);

    return 1;
  }

  if (actionType ==
      AMOTION_EVENT_ACTION_UP)
  {
    uiSetButton(
      0,
      0,
      0);

    return 1;
  }

  if (actionType ==
      AMOTION_EVENT_ACTION_MOVE)
  {
    return 1;
  }

  return 0;
}

static void handleCommand(
  struct android_app *app,
  int32_t command)
{
  switch (command)
  {
    case APP_CMD_INIT_WINDOW:
    {
      if (app->window)
      {
        if (g_app.display != EGL_NO_DISPLAY)
          shutdownEgl();

        if (!initEgl(app->window))
        {
          __android_log_print(
            ANDROID_LOG_ERROR,
            "MinimalUI",
            "EGL initialization failed");

          g_app.running = false;
          return;
        }

        buildUi();
      }

      break;
    }

    case APP_CMD_TERM_WINDOW:
    {
      shutdownEgl();
      break;
    }

    case APP_CMD_DESTROY:
    {
      g_app.running = false;
      break;
    }

    default:
      break;
  }
}

void android_main(
  struct android_app *app)
{
  app_dummy();

  app->onAppCmd = handleCommand;
  app->onInputEvent = handleInput;

  g_app.running = true;

  while (g_app.running)
  {
    int events = 0;

    struct android_poll_source *source = nullptr;

    const int timeout =
      g_app.display == EGL_NO_DISPLAY
        ? -1
        : 0;

    while (ALooper_pollOnce(
             timeout,
             nullptr,
             &events,
             reinterpret_cast<void **>(&source)) >= 0)
    {
      if (source)
        source->process(app, source);

      if (app->destroyRequested)
      {
        g_app.running = false;
        break;
      }

      if (g_app.display != EGL_NO_DISPLAY)
        break;
    }

    if (!g_app.running)
      break;

    if (g_app.display == EGL_NO_DISPLAY)
      continue;

    uiSetHandler(uiHandler);

    buildUi();

    uiProcess(
      static_cast<int>(
        getTimeMilliseconds()));

    draw();
  }

  shutdownEgl();
}
