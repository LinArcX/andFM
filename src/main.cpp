#include <android/input.h>
#include <android/log.h>
#include <android/native_window.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <chrono>
#include <cstring>
#include <string>
#include <vector>

#include <dirent.h>

#include "android_native_app_glue.h"

#include "../third_party/nanovg/nanovg.h"

#define NANOVG_GLES2
#include "../third_party/nanovg/nanovg_gl.h"

#include "../third_party/oui-blendish/oui.h"
#include "../third_party/oui-blendish/blendish.h"

struct FileEntry
{
  std::string name;
  bool isDirectory = false;
};

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
  bool clicked = false;

  std::string currentPath;
  std::vector<FileEntry> entries;
};

static App g_app;

static NVGcolor rgb(
  unsigned char r,
  unsigned char g,
  unsigned char b)
{
  return nvgRGB(r, g, b);
}

static void drawFolderIcon(
  float x,
  float y,
  float size)
{
  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x,
    y + size * 0.15f,
    size,
    size * 0.80f,
    2.0f);
  nvgFillColor(g_app.vg, rgb(220, 180, 100));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x,
    y,
    size * 0.55f,
    size * 0.25f,
    1.5f);
  nvgFill(g_app.vg);
}

static void drawFileIcon(
  float x,
  float y,
  float size)
{
  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x + size * 0.10f,
    y,
    size * 0.80f,
    size,
    2.0f);
  nvgFillColor(g_app.vg, rgb(120, 130, 150));
  nvgFill(g_app.vg);
}

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
  UIcontext *,
  int,
  UIevent event)
{
  if (event == UI_BUTTON0_HOT_UP)
    g_app.clicked = !g_app.clicked;
}

struct EntryItemData
{
  int entryIndex;
};

static int createEntryItem(int entryIndex)
{
  const int item = uiItem(g_app.ui);

  uiSetSize(
    g_app.ui,
    item,
    0,
    BND_WIDGET_HEIGHT);

  uiSetEvents(
    g_app.ui,
    item,
    UI_BUTTON0_HOT_UP);

  EntryItemData *pData =
    static_cast<EntryItemData *>(
      uiAllocHandle(
        g_app.ui,
        item,
        sizeof(EntryItemData)));

  pData->entryIndex = entryIndex;

  return item;
}

static bool readDirectory(const std::string &path)
{
  DIR *pDir = opendir(path.c_str());

  if (!pDir)
  {
    __android_log_print(
      ANDROID_LOG_ERROR,
      "andFM",
      "Failed to open directory: %s",
      path.c_str());

    return false;
  }

  g_app.entries.clear();

  struct dirent *pEntry = nullptr;

  while ((pEntry = readdir(pDir)) != nullptr)
  {
    const char *pName = pEntry->d_name;

    if (strcmp(pName, ".") == 0 ||
        strcmp(pName, "..") == 0)
    {
      continue;
    }

    FileEntry entry;
    entry.name = pName;
    entry.isDirectory = (pEntry->d_type == DT_DIR);

    g_app.entries.push_back(entry);
  }

  closedir(pDir);

  return true;
}

static void buildUi()
{
  const int toolbarHeight = 36;
  const int statusHeight = 22;
  const int sidebarWidth = 140;

  uiBeginLayout(g_app.ui);

  const int root = uiItem(g_app.ui);

  uiSetSize(
    g_app.ui,
    root,
    g_app.width,
    g_app.height);

  const int column =
    uiInsert(
      g_app.ui,
      root,
      uiItem(g_app.ui));

  uiSetBox(
    g_app.ui,
    column,
    UI_COLUMN);

  uiSetLayout(
    g_app.ui,
    column,
    UI_HFILL | UI_VFILL);

  {
    const int topSpacer = uiItem(g_app.ui);

    uiSetSize(
      g_app.ui,
      topSpacer,
      0,
      toolbarHeight);

    uiSetLayout(
      g_app.ui,
      topSpacer,
      UI_HFILL);

    uiInsert(
      g_app.ui,
      column,
      topSpacer);
  }

  for (size_t i = 0; i < g_app.entries.size(); i++)
  {
    const int item =
      createEntryItem(static_cast<int>(i));

    uiSetLayout(
      g_app.ui,
      item,
      UI_HFILL);

    uiSetMargins(
      g_app.ui,
      item,
      sidebarWidth,
      0,
      0,
      0);

    uiInsert(
      g_app.ui,
      column,
      item);
  }

  {
    const int bottomSpacer = uiItem(g_app.ui);

    uiSetSize(
      g_app.ui,
      bottomSpacer,
      0,
      statusHeight);

    uiSetLayout(
      g_app.ui,
      bottomSpacer,
      UI_HFILL);

    uiInsert(
      g_app.ui,
      column,
      bottomSpacer);
  }

  uiEndLayout(g_app.ui);
}

static void drawItem(
  int item)
{
  const UIrect rect =
    uiGetRect(
      g_app.ui,
      item);

  const UIitemState state =
    uiGetState(
      g_app.ui,
      item);

  void *pHandle =
    uiGetHandle(
      g_app.ui,
      item);

  if (pHandle)
  {
    EntryItemData *pData =
      static_cast<EntryItemData *>(pHandle);

    const FileEntry &entry =
      g_app.entries[pData->entryIndex];

    if ((state & UI_HOT) || (state & UI_ACTIVE))
    {
      nvgBeginPath(g_app.vg);
      nvgRect(
        g_app.vg,
        rect.x,
        rect.y,
        rect.w,
        rect.h);
      nvgFillColor(g_app.vg, rgb(45, 60, 80));
      nvgFill(g_app.vg);
    }

    const float iconSize = 16.0f;
    const float iconX = rect.x + 10.0f;
    const float iconY = rect.y + (rect.h - iconSize) * 0.5f;

    if (entry.isDirectory)
    {
      drawFolderIcon(iconX, iconY, iconSize);
    }
    else
    {
      drawFileIcon(iconX, iconY, iconSize);
    }

    nvgFontSize(g_app.vg, 14.0f);
    nvgFontFace(g_app.vg, "default");
    nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(g_app.vg, rgb(210, 210, 210));
    nvgText(
      g_app.vg,
      iconX + iconSize + 10.0f,
      rect.y + rect.h * 0.5f,
      entry.name.c_str(),
      nullptr);

    nvgBeginPath(g_app.vg);
    nvgRect(
      g_app.vg,
      rect.x,
      rect.y + rect.h - 1.0f,
      rect.w,
      1.0f);
    nvgFillColor(g_app.vg, rgb(45, 45, 45));
    nvgFill(g_app.vg);
  }

  int child =
    uiFirstChild(
      g_app.ui,
      item);

  while (child >= 0)
  {
    drawItem(child);

    child =
      uiNextSibling(
        g_app.ui,
        child);
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
    0.118f,
    0.118f,
    0.118f,
    1.0f);

  glClear(
    GL_COLOR_BUFFER_BIT |
    GL_STENCIL_BUFFER_BIT);

  nvgBeginFrame(
    g_app.vg,
    static_cast<float>(g_app.width),
    static_cast<float>(g_app.height),
    1.0f);

  const float toolbarHeight = 36.0f;
  const float statusHeight = 22.0f;
  const float sidebarWidth = 140.0f;
  const float screenWidth = static_cast<float>(g_app.width);
  const float screenHeight = static_cast<float>(g_app.height);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    0.0f,
    screenWidth,
    toolbarHeight);
  nvgFillColor(g_app.vg, rgb(37, 37, 37));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    toolbarHeight - 1.0f,
    screenWidth,
    1.0f);
  nvgFillColor(g_app.vg, rgb(20, 20, 20));
  nvgFill(g_app.vg);

  const float sidebarHeight = screenHeight - toolbarHeight - statusHeight;

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    toolbarHeight,
    sidebarWidth,
    sidebarHeight);
  nvgFillColor(g_app.vg, rgb(33, 33, 33));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    sidebarWidth - 1.0f,
    toolbarHeight,
    1.0f,
    sidebarHeight);
  nvgFillColor(g_app.vg, rgb(20, 20, 20));
  nvgFill(g_app.vg);

  nvgFontSize(g_app.vg, 11.0f);
  nvgFontFace(g_app.vg, "default");
  nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(g_app.vg, rgb(120, 120, 120));
  nvgText(
    g_app.vg,
    14.0f,
    toolbarHeight + 14.0f,
    "PLACES",
    nullptr);

  const char *places[] =
  {
    "Home",
    "Root",
    "Documents",
    "Downloads",
    "Pictures",
    "Videos",
  };

  nvgFontSize(g_app.vg, 13.0f);
  nvgFillColor(g_app.vg, rgb(190, 190, 190));

  for (size_t i = 0; i < sizeof(places) / sizeof(places[0]); i++)
  {
    const float y = toolbarHeight + 32.0f +
      static_cast<float>(i) * 22.0f;

    nvgText(
      g_app.vg,
      24.0f,
      y,
      places[i],
      nullptr);
  }

  nvgFontSize(g_app.vg, 14.0f);
  nvgFillColor(g_app.vg, rgb(210, 210, 210));
  nvgText(
    g_app.vg,
    14.0f,
    toolbarHeight * 0.5f,
    g_app.currentPath.c_str(),
    nullptr);

  drawItem(0);

  const float statusY = screenHeight - statusHeight;

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    statusY,
    screenWidth,
    statusHeight);
  nvgFillColor(g_app.vg, rgb(37, 37, 37));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    statusY,
    screenWidth,
    1.0f);
  nvgFillColor(g_app.vg, rgb(20, 20, 20));
  nvgFill(g_app.vg);

  const std::string statusText =
    std::to_string(g_app.entries.size()) + " items";

  nvgFontSize(g_app.vg, 11.0f);
  nvgFillColor(g_app.vg, rgb(140, 140, 140));
  nvgText(
    g_app.vg,
    12.0f,
    statusY + statusHeight * 0.5f,
    statusText.c_str(),
    nullptr);

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
    (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
    AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

  const int actionType =
    action & AMOTION_EVENT_ACTION_MASK;

  const float x =
    AMotionEvent_getX(
      event,
      pointer);

  const float y =
    AMotionEvent_getY(
      event,
      pointer);

  uiSetCursor(
    g_app.ui,
    static_cast<int>(x),
    static_cast<int>(y));

  if (actionType ==
      AMOTION_EVENT_ACTION_DOWN)
  {
    uiSetButton(
      g_app.ui,
      0,
      0,
      true);

    return 1;
  }

  if (actionType ==
      AMOTION_EVENT_ACTION_UP)
  {
    uiSetButton(
      g_app.ui,
      0,
      0,
      false);

    return 1;
  }

  return 1;
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

        g_app.window = app->window;

        g_app.currentPath = "/storage/emulated/0/";

        if (!readDirectory(g_app.currentPath))
        {
          g_app.currentPath = "/";

          readDirectory(g_app.currentPath);
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
  //app_dummy();

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
        source->process(
          app,
          source);

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

    if (g_app.ui)
    {
      uiSetHandler(
        g_app.ui,
        buttonHandler);

      uiProcess(
        g_app.ui,
        static_cast<int>(
          getTimeMilliseconds()));
    }

    draw();
  }

  shutdownEgl();
}
