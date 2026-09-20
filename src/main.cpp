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
#include <unistd.h>
#include <sys/stat.h>

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

  std::string currentPath;
  std::vector<FileEntry> entries;
  std::vector<int> filteredIndices;
  std::string pendingNavigate;
  float scrollOffset = 0.0f;

  bool searchActive = false;
  std::string searchQuery;

  struct BreadcrumbItem
  {
    std::string name;
    std::string path;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
  };

  std::vector<BreadcrumbItem> breadcrumbs;
};

static App g_app;

static const float kTopInset = 150.0f;
static const float kToolbarHeight = 108.0f;
static const float kSidebarWidth = 420.0f;
static const float kBottomNavHeight = 210.0f;
static const float kStatusBarHeight = 66.0f;
static const float kRowHeight = 168.0f;
static const float kSidebarItemHeight = 132.0f;

static const float kScrollButtonSize = 108.0f;
static const float kScrollButtonMargin = 24.0f;

static const float kSearchButtonSize = 72.0f;
static const float kSearchButtonMargin = 30.0f;

static const float kKeyboardKeyHeight = 108.0f;
static const float kKeyboardKeyGap = 10.0f;
static const float kKeyboardPadding = 12.0f;
static const float kKeyboardBottomInset = 150.0f;

static const char *g_keyboardRows[] =
{
  "1234567890",
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm._-",
};

static const int g_keyboardRowCount =
  static_cast<int>(sizeof(g_keyboardRows) / sizeof(g_keyboardRows[0]));

struct SidebarPlace
{
  const char *name;
  const char *path;
};

static const SidebarPlace g_places[] =
{
  {"Home", "/storage/emulated/0/"},
  {"Root", "/"},
  {"Pictures", "/storage/emulated/0/Pictures/"},
  {"DCIM", "/storage/emulated/0/DCIM/"},
  {"Download", "/storage/emulated/0/Download/"},
  {"Android", "/storage/emulated/0/Android/"},
  {"Movies", "/storage/emulated/0/Movies/"},
  {"Notifications", "/storage/emulated/0/Notifications/"},
  {"Music", "/storage/emulated/0/Music/"},
};

static const int g_placeCount =
  static_cast<int>(sizeof(g_places) / sizeof(g_places[0]));

static NVGcolor rgb(
  unsigned char r,
  unsigned char g,
  unsigned char b)
{
  return nvgRGB(r, g, b);
}

static float getKeyboardTop()
{
  const float keyboardHeight =
    kKeyboardPadding * 2.0f +
    static_cast<float>(g_keyboardRowCount) *
      (kKeyboardKeyHeight + kKeyboardKeyGap) +
    kKeyboardKeyHeight;

  return static_cast<float>(g_app.height) -
    keyboardHeight -
    kKeyboardBottomInset;
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

static void drawHomeIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.5f, y);
  nvgLineTo(g_app.vg, x + size, y + size * 0.5f);
  nvgLineTo(g_app.vg, x + size * 0.85f, y + size * 0.5f);
  nvgLineTo(g_app.vg, x + size * 0.85f, y + size);
  nvgLineTo(g_app.vg, x + size * 0.15f, y + size);
  nvgLineTo(g_app.vg, x + size * 0.15f, y + size * 0.5f);
  nvgLineTo(g_app.vg, x, y + size * 0.5f);
  nvgClosePath(g_app.vg);
  nvgFillColor(g_app.vg, rgb(190, 200, 220));
  nvgFill(g_app.vg);
}

static void drawRootIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x + size * 0.10f,
    y + size * 0.10f,
    size * 0.80f,
    size * 0.80f,
    2.0f);
  nvgFillColor(g_app.vg, rgb(160, 170, 190));
  nvgFill(g_app.vg);
}

static void drawPictureIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgRoundedRect(g_app.vg, x, y, size, size, 2.0f);
  nvgFillColor(g_app.vg, rgb(70, 90, 120));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.10f, y + size * 0.85f);
  nvgLineTo(g_app.vg, x + size * 0.40f, y + size * 0.35f);
  nvgLineTo(g_app.vg, x + size * 0.60f, y + size * 0.60f);
  nvgLineTo(g_app.vg, x + size * 0.75f, y + size * 0.45f);
  nvgLineTo(g_app.vg, x + size * 0.90f, y + size * 0.85f);
  nvgClosePath(g_app.vg);
  nvgFillColor(g_app.vg, rgb(130, 180, 110));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.72f, y + size * 0.25f, size * 0.09f);
  nvgFillColor(g_app.vg, rgb(240, 200, 100));
  nvgFill(g_app.vg);
}

static void drawDcimIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x + size * 0.05f,
    y + size * 0.25f,
    size * 0.90f,
    size * 0.60f,
    2.0f);
  nvgFillColor(g_app.vg, rgb(140, 150, 170));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x + size * 0.35f,
    y + size * 0.15f,
    size * 0.30f,
    size * 0.15f,
    1.0f);
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.5f, y + size * 0.55f, size * 0.18f);
  nvgFillColor(g_app.vg, rgb(60, 70, 90));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.5f, y + size * 0.55f, size * 0.09f);
  nvgFillColor(g_app.vg, rgb(160, 180, 210));
  nvgFill(g_app.vg);
}

static void drawDownloadIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.5f, y + size * 0.10f);
  nvgLineTo(g_app.vg, x + size * 0.5f, y + size * 0.60f);
  nvgStrokeColor(g_app.vg, rgb(190, 200, 220));
  nvgStrokeWidth(g_app.vg, 2.0f);
  nvgLineCap(g_app.vg, NVG_ROUND);
  nvgStroke(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.30f, y + size * 0.42f);
  nvgLineTo(g_app.vg, x + size * 0.5f, y + size * 0.62f);
  nvgLineTo(g_app.vg, x + size * 0.70f, y + size * 0.42f);
  nvgStroke(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.15f, y + size * 0.75f);
  nvgLineTo(g_app.vg, x + size * 0.15f, y + size * 0.90f);
  nvgLineTo(g_app.vg, x + size * 0.85f, y + size * 0.90f);
  nvgLineTo(g_app.vg, x + size * 0.85f, y + size * 0.75f);
  nvgStroke(g_app.vg);
}

static void drawAndroidIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgRoundedRect(
    g_app.vg,
    x + size * 0.15f,
    y + size * 0.35f,
    size * 0.70f,
    size * 0.45f,
    size * 0.22f);
  nvgFillColor(g_app.vg, rgb(130, 200, 120));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.30f, y + size * 0.35f);
  nvgLineTo(g_app.vg, x + size * 0.22f, y + size * 0.15f);
  nvgMoveTo(g_app.vg, x + size * 0.70f, y + size * 0.35f);
  nvgLineTo(g_app.vg, x + size * 0.78f, y + size * 0.15f);
  nvgStrokeColor(g_app.vg, rgb(130, 200, 120));
  nvgStrokeWidth(g_app.vg, 1.5f);
  nvgStroke(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.35f, y + size * 0.52f, size * 0.05f);
  nvgFillColor(g_app.vg, rgb(30, 30, 30));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.65f, y + size * 0.52f, size * 0.05f);
  nvgFill(g_app.vg);
}

static void drawMovieIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    x + size * 0.10f,
    y + size * 0.15f,
    size * 0.80f,
    size * 0.70f);
  nvgFillColor(g_app.vg, rgb(120, 130, 150));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(g_app.vg, x + size * 0.18f, y + size * 0.20f, size * 0.12f, size * 0.10f);
  nvgRect(g_app.vg, x + size * 0.44f, y + size * 0.20f, size * 0.12f, size * 0.10f);
  nvgRect(g_app.vg, x + size * 0.70f, y + size * 0.20f, size * 0.12f, size * 0.10f);
  nvgRect(g_app.vg, x + size * 0.18f, y + size * 0.70f, size * 0.12f, size * 0.10f);
  nvgRect(g_app.vg, x + size * 0.44f, y + size * 0.70f, size * 0.12f, size * 0.10f);
  nvgRect(g_app.vg, x + size * 0.70f, y + size * 0.70f, size * 0.12f, size * 0.10f);
  nvgFillColor(g_app.vg, rgb(40, 40, 50));
  nvgFill(g_app.vg);
}

static void drawBellIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.20f, y + size * 0.65f);
  nvgQuadTo(
    g_app.vg,
    x + size * 0.20f,
    y + size * 0.20f,
    x + size * 0.50f,
    y + size * 0.20f);
  nvgQuadTo(
    g_app.vg,
    x + size * 0.80f,
    y + size * 0.20f,
    x + size * 0.80f,
    y + size * 0.65f);
  nvgLineTo(g_app.vg, x + size * 0.90f, y + size * 0.75f);
  nvgLineTo(g_app.vg, x + size * 0.10f, y + size * 0.75f);
  nvgClosePath(g_app.vg);
  nvgFillColor(g_app.vg, rgb(220, 190, 100));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.50f, y + size * 0.82f, size * 0.07f);
  nvgFill(g_app.vg);
}

static void drawMusicIcon(float x, float y, float size)
{
  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.30f, y + size * 0.78f, size * 0.12f);
  nvgFillColor(g_app.vg, rgb(190, 150, 220));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgCircle(g_app.vg, x + size * 0.72f, y + size * 0.68f, size * 0.12f);
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.42f, y + size * 0.78f);
  nvgLineTo(g_app.vg, x + size * 0.42f, y + size * 0.20f);
  nvgLineTo(g_app.vg, x + size * 0.84f, y + size * 0.10f);
  nvgLineTo(g_app.vg, x + size * 0.84f, y + size * 0.68f);
  nvgStrokeColor(g_app.vg, rgb(190, 150, 220));
  nvgStrokeWidth(g_app.vg, 2.0f);
  nvgStroke(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgMoveTo(g_app.vg, x + size * 0.42f, y + size * 0.20f);
  nvgLineTo(g_app.vg, x + size * 0.84f, y + size * 0.10f);
  nvgStroke(g_app.vg);
}

static void drawPlaceIcon(int index, float x, float y, float size)
{
  switch (index)
  {
    case 0:
      drawHomeIcon(x, y, size);
      break;
    case 1:
      drawRootIcon(x, y, size);
      break;
    case 2:
      drawPictureIcon(x, y, size);
      break;
    case 3:
      drawDcimIcon(x, y, size);
      break;
    case 4:
      drawDownloadIcon(x, y, size);
      break;
    case 5:
      drawAndroidIcon(x, y, size);
      break;
    case 6:
      drawMovieIcon(x, y, size);
      break;
    case 7:
      drawBellIcon(x, y, size);
      break;
    case 8:
      drawMusicIcon(x, y, size);
      break;
    default:
      drawFileIcon(x, y, size);
      break;
  }
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
    kRowHeight);

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

static void rebuildFilter()
{
  g_app.filteredIndices.clear();
  g_app.scrollOffset = 0.0f;

  if (g_app.searchQuery.empty())
  {
    for (size_t i = 0; i < g_app.entries.size(); i++)
      g_app.filteredIndices.push_back(static_cast<int>(i));

    return;
  }

  std::string needle = g_app.searchQuery;

  for (size_t i = 0; i < needle.size(); i++)
  {
    const char c = needle[i];

    if (c >= 'A' && c <= 'Z')
      needle[i] = static_cast<char>(c - 'A' + 'a');
  }

  for (size_t i = 0; i < g_app.entries.size(); i++)
  {
    std::string haystack = g_app.entries[i].name;

    for (size_t j = 0; j < haystack.size(); j++)
    {
      const char c = haystack[j];

      if (c >= 'A' && c <= 'Z')
        haystack[j] = static_cast<char>(c - 'A' + 'a');
    }

    if (haystack.find(needle) != std::string::npos)
      g_app.filteredIndices.push_back(static_cast<int>(i));
  }
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

    if (strncmp(pName, "vendor_", 7) == 0 ||
        strncmp(pName, "plat_", 5) == 0 ||
        strncmp(pName, "init", 4) == 0)
    {
      continue;
    }

    std::string childPath = path;

    if (childPath.empty() || childPath.back() != '/')
      childPath += '/';

    childPath += pName;

    struct stat st;

    if (stat(childPath.c_str(), &st) != 0)
      continue;

    const bool isDir = S_ISDIR(st.st_mode);

    if (isDir)
    {
      DIR *pChild = opendir(childPath.c_str());

      if (!pChild)
        continue;

      closedir(pChild);
    }

    FileEntry entry;
    entry.name = pName;
    entry.isDirectory = isDir;

    g_app.entries.push_back(entry);
  }

  closedir(pDir);

  rebuildFilter();

  return true;
}

static void buildUi()
{
  const int toolbarHeight = static_cast<int>(kToolbarHeight);
  const int statusHeight = static_cast<int>(kStatusBarHeight);
  const int sidebarWidth = static_cast<int>(kSidebarWidth);
  const int topInset = static_cast<int>(kTopInset);
  const int bottomNav = static_cast<int>(kBottomNavHeight);

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
    UI_HFILL | UI_TOP);

  uiSetMargins(
    g_app.ui,
    column,
    0,
    topInset,
    0,
    0);

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

  for (size_t i = 0; i < g_app.filteredIndices.size(); i++)
  {
    const int item =
      createEntryItem(g_app.filteredIndices[i]);

    uiSetLayout(
      g_app.ui,
      item,
      UI_HFILL);

    const int topMargin =
      (i == 0) ? -static_cast<int>(g_app.scrollOffset) : 0;

    uiSetMargins(
      g_app.ui,
      item,
      sidebarWidth,
      topMargin,
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
      statusHeight + bottomNav);

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

static bool isOnSearchButton(
  float x,
  float y)
{
  const float btnX =
    static_cast<float>(g_app.width) - kSearchButtonSize - kSearchButtonMargin;

  const float btnY =
    kTopInset + kToolbarHeight * 0.5f - kSearchButtonSize * 0.5f;

  return x >= btnX &&
         x <= btnX + kSearchButtonSize &&
         y >= btnY &&
         y <= btnY + kSearchButtonSize;
}

static void handleKeyboardTap(
  float x,
  float y)
{
  const float screenWidth = static_cast<float>(g_app.width);
  const float keyboardTop = getKeyboardTop();

  const float totalGaps = 9.0f * kKeyboardKeyGap;
  const float keyWidth =
    (screenWidth - kKeyboardPadding * 2.0f - totalGaps) / 10.0f;

  float rowY = keyboardTop + kKeyboardPadding;

  for (int r = 0; r < g_keyboardRowCount; r++)
  {
    const char *pRow = g_keyboardRows[r];
    const int n = static_cast<int>(strlen(pRow));

    const float rowWidth =
      static_cast<float>(n) * keyWidth +
      static_cast<float>(n - 1) * kKeyboardKeyGap;

    const float rowX = (screenWidth - rowWidth) * 0.5f;

    if (y >= rowY && y <= rowY + kKeyboardKeyHeight)
    {
      for (int c = 0; c < n; c++)
      {
        const float keyX =
          rowX + static_cast<float>(c) * (keyWidth + kKeyboardKeyGap);

        if (x >= keyX && x <= keyX + keyWidth)
        {
          g_app.searchQuery += pRow[c];
          rebuildFilter();
          buildUi();
          return;
        }
      }

      return;
    }

    rowY += kKeyboardKeyHeight + kKeyboardKeyGap;
  }

  const float actionWidth =
    (screenWidth - kKeyboardPadding * 2.0f - kKeyboardKeyGap * 2.0f) / 3.0f;

  float actionX = kKeyboardPadding;

  if (y >= rowY && y <= rowY + kKeyboardKeyHeight)
  {
    if (x >= actionX && x <= actionX + actionWidth)
    {
      g_app.searchActive = false;
      g_app.searchQuery.clear();
      rebuildFilter();
      buildUi();
      return;
    }

    actionX += actionWidth + kKeyboardKeyGap;

    if (x >= actionX && x <= actionX + actionWidth)
    {
      g_app.searchQuery += ' ';
      rebuildFilter();
      buildUi();
      return;
    }

    actionX += actionWidth + kKeyboardKeyGap;

    if (x >= actionX && x <= actionX + actionWidth)
    {
      if (!g_app.searchQuery.empty())
      {
        g_app.searchQuery.pop_back();
        rebuildFilter();
        buildUi();
      }

      return;
    }
  }
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

    const float iconSize = 72.0f;
    const float iconX = rect.x + 30.0f;
    const float iconY = rect.y + (rect.h - iconSize) * 0.5f;

    if (entry.isDirectory)
    {
      drawFolderIcon(iconX, iconY, iconSize);
    }
    else
    {
      drawFileIcon(iconX, iconY, iconSize);
    }

    nvgFontSize(g_app.vg, 42.0f);
    nvgFontFace(g_app.vg, "default");
    nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(g_app.vg, rgb(210, 210, 210));
    nvgText(
      g_app.vg,
      iconX + iconSize + 30.0f,
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

  const float toolbarHeight = 108.0f;
  const float statusHeight = 66.0f;
  const float sidebarWidth = 420.0f;
  const float screenWidth = static_cast<float>(g_app.width);
  const float screenHeight = static_cast<float>(g_app.height);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    0.0f,
    screenWidth,
    kTopInset + toolbarHeight);
  nvgFillColor(g_app.vg, rgb(37, 37, 37));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    kTopInset + toolbarHeight - 1.0f,
    screenWidth,
    1.0f);
  nvgFillColor(g_app.vg, rgb(20, 20, 20));
  nvgFill(g_app.vg);

  const float listTop = kTopInset + toolbarHeight;
  const float listBottom =
    screenHeight - statusHeight - kBottomNavHeight;
  const float sidebarHeight = listBottom - listTop;

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    listTop,
    sidebarWidth,
    sidebarHeight);
  nvgFillColor(g_app.vg, rgb(33, 33, 33));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    sidebarWidth - 1.0f,
    listTop,
    1.0f,
    sidebarHeight);
  nvgFillColor(g_app.vg, rgb(20, 20, 20));
  nvgFill(g_app.vg);

  nvgFontSize(g_app.vg, 33.0f);
  nvgFontFace(g_app.vg, "default");
  nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(g_app.vg, rgb(120, 120, 120));
  nvgText(
    g_app.vg,
    42.0f,
    listTop + 42.0f,
    "PLACES",
    nullptr);

  nvgFontSize(g_app.vg, 39.0f);
  nvgFontFace(g_app.vg, "default");
  nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  for (int i = 0; i < g_placeCount; i++)
  {
    const float y = listTop + 96.0f +
      static_cast<float>(i) * kSidebarItemHeight;

    const float iconSize = 54.0f;

    drawPlaceIcon(
      i,
      42.0f,
      y - iconSize * 0.5f,
      iconSize);

    nvgFillColor(g_app.vg, rgb(190, 190, 190));
    nvgText(
      g_app.vg,
      126.0f,
      y,
      g_places[i].name,
      nullptr);
  }

  const float toolbarCenterY = kTopInset + toolbarHeight * 0.5f;
  {
    g_app.breadcrumbs.clear();
    float bx = 30.0f;
    const float maxX =
      screenWidth - kSearchButtonSize - kSearchButtonMargin - 30.0f;
    const float itemHeight = 72.0f;
    const float itemY = toolbarCenterY - itemHeight * 0.5f;

    std::string breadcrumbPath = g_app.currentPath;

    if (breadcrumbPath.empty())
      breadcrumbPath = "/";

    std::vector<std::string> segments;
    segments.push_back("/");

    std::string currentSegment;

    for (size_t i = 0; i < breadcrumbPath.size(); i++)
    {
      const char c = breadcrumbPath[i];

      if (c == '/')
      {
        if (!currentSegment.empty())
        {
          segments.push_back(currentSegment);
          currentSegment.clear();
        }
      }
      else
      {
        currentSegment += c;
      }
    }

    if (!currentSegment.empty())
      segments.push_back(currentSegment);

    nvgFontSize(g_app.vg, 36.0f);
    nvgFontFace(g_app.vg, "default");
    nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    std::string currentPathAccum = "";

    for (size_t i = 0; i < segments.size(); i++)
    {
      const std::string &seg = segments[i];
      const bool isLast = (i + 1 == segments.size());

      if (i == 0)
      {
        currentPathAccum = "/";
      }
      else
      {
        if (!currentPathAccum.empty() && currentPathAccum.back() != '/')
          currentPathAccum += '/';
        currentPathAccum += seg;
      }

      float segBounds[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

      nvgTextBounds(
        g_app.vg,
        0.0f,
        0.0f,
        seg.c_str(),
        nullptr,
        segBounds);

      const float textW = segBounds[2] - segBounds[0];

      if (bx + textW > maxX)
        break;

      if (isLast)
      {
        nvgFillColor(g_app.vg, rgb(240, 240, 240));
      }
      else
      {
        nvgFillColor(g_app.vg, rgb(160, 160, 160));
      }

      nvgText(
        g_app.vg,
        bx,
        itemY + itemHeight * 0.5f,
        seg.c_str(),
        nullptr);

      g_app.breadcrumbs.push_back({seg, currentPathAccum, bx, itemY, textW, itemHeight});

      bx += textW + 12.0f;

      if (!isLast)
      {
        nvgFillColor(g_app.vg, rgb(100, 100, 100));
        nvgText(
          g_app.vg,
          bx,
          itemY + itemHeight * 0.5f,
          "/",
          nullptr);

        bx += 24.0f;
      }
    }
  }

  {
    const float btnX =
      screenWidth - kSearchButtonSize - kSearchButtonMargin;

    const float btnY =
      toolbarCenterY - kSearchButtonSize * 0.5f;

    nvgBeginPath(g_app.vg);
    nvgRoundedRect(
      g_app.vg,
      btnX,
      btnY,
      kSearchButtonSize,
      kSearchButtonSize,
      6.0f);
    nvgFillColor(
      g_app.vg,
      g_app.searchActive
        ? rgb(70, 110, 160)
        : rgb(55, 55, 55));
    nvgFill(g_app.vg);

    nvgBeginPath(g_app.vg);
    nvgCircle(
      g_app.vg,
      btnX + kSearchButtonSize * 0.42f,
      btnY + kSearchButtonSize * 0.42f,
      kSearchButtonSize * 0.20f);
    nvgStrokeColor(g_app.vg, rgb(220, 220, 220));
    nvgStrokeWidth(g_app.vg, 4.0f);
    nvgStroke(g_app.vg);

    nvgBeginPath(g_app.vg);
    nvgMoveTo(
      g_app.vg,
      btnX + kSearchButtonSize * 0.56f,
      btnY + kSearchButtonSize * 0.56f);
    nvgLineTo(
      g_app.vg,
      btnX + kSearchButtonSize * 0.76f,
      btnY + kSearchButtonSize * 0.76f);
    nvgStroke(g_app.vg);
  }

  nvgSave(g_app.vg);
  nvgScissor(
    g_app.vg,
    sidebarWidth,
    listTop,
    screenWidth - sidebarWidth,
    sidebarHeight);
  drawItem(0);
  nvgRestore(g_app.vg);

  const float bottomNavY =
    screenHeight - statusHeight - kBottomNavHeight;

  const float statusY = screenHeight - statusHeight;

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    bottomNavY,
    screenWidth,
    kBottomNavHeight);
  nvgFillColor(g_app.vg, rgb(37, 37, 37));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    bottomNavY,
    screenWidth,
    1.0f);
  nvgFillColor(g_app.vg, rgb(20, 20, 20));
  nvgFill(g_app.vg);

  nvgBeginPath(g_app.vg);
  nvgRect(
    g_app.vg,
    0.0f,
    statusY,
    screenWidth,
    statusHeight);
  nvgFillColor(g_app.vg, rgb(28, 28, 28));
  nvgFill(g_app.vg);

  std::string statusText;

  if (g_app.searchActive)
  {
    statusText =
      std::to_string(g_app.filteredIndices.size()) +
      " / " +
      std::to_string(g_app.entries.size()) +
      " items";
  }
  else
  {
    statusText =
      std::to_string(g_app.entries.size()) + " items";
  }

  nvgFontSize(g_app.vg, 33.0f);
  nvgFillColor(g_app.vg, rgb(140, 140, 140));
  nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(
    g_app.vg,
    36.0f,
    statusY + statusHeight * 0.5f,
    statusText.c_str(),
    nullptr);

  const float contentHeight =
    static_cast<float>(g_app.entries.size()) *
    static_cast<float>(kRowHeight);

  if (contentHeight > sidebarHeight)
  {
    const float sbx =
      screenWidth - kScrollButtonSize - kScrollButtonMargin;

    const float sbyUp =
      listBottom - kScrollButtonSize * 2.0f - 36.0f;

    const float sbyDown =
      listBottom - kScrollButtonSize - 18.0f;

    nvgBeginPath(g_app.vg);
    nvgRoundedRect(
      g_app.vg,
      sbx,
      sbyUp,
      kScrollButtonSize,
      kScrollButtonSize,
      6.0f);
    nvgFillColor(g_app.vg, rgb(55, 55, 55));
    nvgFill(g_app.vg);

    nvgBeginPath(g_app.vg);
    nvgRoundedRect(
      g_app.vg,
      sbx,
      sbyDown,
      kScrollButtonSize,
      kScrollButtonSize,
      6.0f);
    nvgFill(g_app.vg);

    nvgFontSize(g_app.vg, 54.0f);
    nvgFillColor(g_app.vg, rgb(220, 220, 220));
    nvgTextAlign(g_app.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(
      g_app.vg,
      sbx + kScrollButtonSize * 0.5f,
      sbyUp + kScrollButtonSize * 0.5f,
      "^",
      nullptr);
    nvgText(
      g_app.vg,
      sbx + kScrollButtonSize * 0.5f,
      sbyDown + kScrollButtonSize * 0.5f,
      "v",
      nullptr);
    nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  }

  if (g_app.searchActive)
  {
    const float searchBarRight =
      screenWidth - kSearchButtonSize - kSearchButtonMargin;

    nvgBeginPath(g_app.vg);
    nvgRect(
      g_app.vg,
      0.0f,
      kTopInset,
      searchBarRight,
      toolbarHeight);
    nvgFillColor(g_app.vg, rgb(48, 48, 48));
    nvgFill(g_app.vg);

    nvgBeginPath(g_app.vg);
    nvgRect(
      g_app.vg,
      0.0f,
      kTopInset + toolbarHeight - 2.0f,
      searchBarRight,
      2.0f);
    nvgFillColor(g_app.vg, rgb(70, 110, 160));
    nvgFill(g_app.vg);

    nvgFontSize(g_app.vg, 40.0f);
    nvgFontFace(g_app.vg, "default");
    nvgTextAlign(g_app.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    std::string displayQuery = g_app.searchQuery;

    if (displayQuery.empty())
      displayQuery = "Search...";

    nvgFillColor(
      g_app.vg,
      g_app.searchQuery.empty()
        ? rgb(120, 120, 120)
        : rgb(230, 230, 230));

    nvgText(
      g_app.vg,
      36.0f,
      kTopInset + toolbarHeight * 0.5f,
      displayQuery.c_str(),
      nullptr);

    const float keyboardTop = getKeyboardTop();
    const float keyboardHeight = screenHeight - keyboardTop;

    nvgBeginPath(g_app.vg);
    nvgRect(
      g_app.vg,
      0.0f,
      keyboardTop,
      screenWidth,
      keyboardHeight);
    nvgFillColor(g_app.vg, rgb(28, 28, 28));
    nvgFill(g_app.vg);

    const float totalGaps = 9.0f * kKeyboardKeyGap;
    const float keyWidth =
      (screenWidth - kKeyboardPadding * 2.0f - totalGaps) / 10.0f;

    float rowY = keyboardTop + kKeyboardPadding;

    for (int r = 0; r < g_keyboardRowCount; r++)
    {
      const char *pRow = g_keyboardRows[r];
      const int n = static_cast<int>(strlen(pRow));

      const float rowWidth =
        static_cast<float>(n) * keyWidth +
        static_cast<float>(n - 1) * kKeyboardKeyGap;

      float keyX = (screenWidth - rowWidth) * 0.5f;

      for (int c = 0; c < n; c++)
      {
        nvgBeginPath(g_app.vg);
        nvgRoundedRect(
          g_app.vg,
          keyX,
          rowY,
          keyWidth,
          kKeyboardKeyHeight,
          8.0f);
        nvgFillColor(g_app.vg, rgb(55, 55, 55));
        nvgFill(g_app.vg);

        char label[2] = { pRow[c], '\0' };

        nvgFontSize(g_app.vg, 48.0f);
        nvgFontFace(g_app.vg, "default");
        nvgTextAlign(g_app.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(g_app.vg, rgb(230, 230, 230));
        nvgText(
          g_app.vg,
          keyX + keyWidth * 0.5f,
          rowY + kKeyboardKeyHeight * 0.5f,
          label,
          nullptr);

        keyX += keyWidth + kKeyboardKeyGap;
      }

      rowY += kKeyboardKeyHeight + kKeyboardKeyGap;
    }

    {
      const float actionWidth =
        (screenWidth - kKeyboardPadding * 2.0f - kKeyboardKeyGap * 2.0f) / 3.0f;

      float actionX = kKeyboardPadding;

      nvgBeginPath(g_app.vg);
      nvgRoundedRect(
        g_app.vg,
        actionX,
        rowY,
        actionWidth,
        kKeyboardKeyHeight,
        8.0f);
      nvgFillColor(g_app.vg, rgb(90, 55, 55));
      nvgFill(g_app.vg);

      nvgFontSize(g_app.vg, 40.0f);
      nvgFontFace(g_app.vg, "default");
      nvgTextAlign(g_app.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(g_app.vg, rgb(230, 230, 230));
      nvgText(
        g_app.vg,
        actionX + actionWidth * 0.5f,
        rowY + kKeyboardKeyHeight * 0.5f,
        "Close",
        nullptr);

      actionX += actionWidth + kKeyboardKeyGap;

      nvgBeginPath(g_app.vg);
      nvgRoundedRect(
        g_app.vg,
        actionX,
        rowY,
        actionWidth,
        kKeyboardKeyHeight,
        8.0f);
      nvgFillColor(g_app.vg, rgb(55, 55, 55));
      nvgFill(g_app.vg);

      nvgText(
        g_app.vg,
        actionX + actionWidth * 0.5f,
        rowY + kKeyboardKeyHeight * 0.5f,
        "Space",
        nullptr);

      actionX += actionWidth + kKeyboardKeyGap;

      nvgBeginPath(g_app.vg);
      nvgRoundedRect(
        g_app.vg,
        actionX,
        rowY,
        actionWidth,
        kKeyboardKeyHeight,
        8.0f);
      nvgFillColor(g_app.vg, rgb(55, 55, 55));
      nvgFill(g_app.vg);

      nvgText(
        g_app.vg,
        actionX + actionWidth * 0.5f,
        rowY + kKeyboardKeyHeight * 0.5f,
        "Del",
        nullptr);
    }
  }

  nvgEndFrame(g_app.vg);

  eglSwapBuffers(
    g_app.display,
    g_app.surface);
}

static bool isOnSidebar(
  float x,
  float y)
{
  const float listTop = kTopInset + kToolbarHeight;
  const float itemHeight = kSidebarItemHeight;
  const float startY = listTop + 32.0f;

  if (x < 0.0f || x > kSidebarWidth)
    return false;

  if (y < startY - itemHeight * 0.5f)
    return false;

  if (y > startY + static_cast<float>(g_placeCount) * itemHeight)
    return false;

  return true;
}

static void handleSidebarClick(float y)
{
  const float listTop = kTopInset + kToolbarHeight;
  const float itemHeight = kSidebarItemHeight;
  const float startY = listTop + 32.0f;

  const int idx = static_cast<int>(
    (y - startY) / itemHeight + 0.5f);

  if (idx < 0 || idx >= g_placeCount)
    return;

  g_app.pendingNavigate = g_places[idx].path;
}

static void buttonHandler(
  UIcontext *,
  int item,
  UIevent event)
{
  if (event != UI_BUTTON0_HOT_UP)
    return;

  void *pHandle = uiGetHandle(g_app.ui, item);

  if (!pHandle)
    return;

  EntryItemData *pData = static_cast<EntryItemData *>(pHandle);

  const FileEntry &entry = g_app.entries[pData->entryIndex];

  if (!entry.isDirectory)
    return;

  std::string newPath = g_app.currentPath;

  if (newPath.empty() || newPath.back() != '/')
    newPath += '/';

  newPath += entry.name;
  newPath += '/';

  g_app.pendingNavigate = newPath;
}

static bool isOnScrollUp(float x, float y)
{
  const float bx =
    static_cast<float>(g_app.width) - kScrollButtonSize - kScrollButtonMargin;

  const float listBottom =
    static_cast<float>(g_app.height) - kStatusBarHeight - kBottomNavHeight;

  const float by = listBottom - kScrollButtonSize * 2.0f - 36.0f;

  return x >= bx &&
         x <= bx + kScrollButtonSize &&
         y >= by &&
         y <= by + kScrollButtonSize;
}

static bool isOnScrollDown(float x, float y)
{
  const float bx =
    static_cast<float>(g_app.width) - kScrollButtonSize - kScrollButtonMargin;

  const float listBottom =
    static_cast<float>(g_app.height) - kStatusBarHeight - kBottomNavHeight;

  const float by = listBottom - kScrollButtonSize - 18.0f;

  return x >= bx &&
         x <= bx + kScrollButtonSize &&
         y >= by &&
         y <= by + kScrollButtonSize;
}

static bool isInListArea(float x, float y)
{
  const float listTop = kTopInset + kToolbarHeight;
  const float listBottom =
    static_cast<float>(g_app.height) - kStatusBarHeight - kBottomNavHeight;

  return x >= kSidebarWidth &&
         y >= listTop &&
         y <= listBottom;
}

static void handleScroll(int direction)
{
  const float contentHeight =
    static_cast<float>(g_app.entries.size()) *
    static_cast<float>(kRowHeight);

  const float visibleHeight =
    static_cast<float>(g_app.height) -
    kTopInset - kToolbarHeight - kStatusBarHeight - kBottomNavHeight;

  float maxScroll = contentHeight - visibleHeight;

  if (maxScroll < 0.0f)
    maxScroll = 0.0f;

  const float step = static_cast<float>(kRowHeight) * 3.0f;

  float newOffset =
    g_app.scrollOffset + static_cast<float>(direction) * step;

  if (newOffset < 0.0f)
    newOffset = 0.0f;

  if (newOffset > maxScroll)
    newOffset = maxScroll;

  if (newOffset != g_app.scrollOffset)
  {
    g_app.scrollOffset = newOffset;
    buildUi();
  }
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
    if (g_app.searchActive && y >= getKeyboardTop())
      return 1;

    if (isOnSearchButton(x, y))
      return 1;

    if (!isInListArea(x, y))
      return 1;

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
    if (isOnSearchButton(x, y))
    {
      g_app.searchActive = !g_app.searchActive;

      if (!g_app.searchActive)
      {
        g_app.searchQuery.clear();
        rebuildFilter();
        buildUi();
      }

      return 1;
    }

    if (g_app.searchActive && y >= getKeyboardTop())
    {
      handleKeyboardTap(x, y);
      return 1;
    }

    if (isOnSidebar(x, y))
    {
      handleSidebarClick(y);
      return 1;
    }

    for (const auto &crumb : g_app.breadcrumbs)
    {
      if (x >= crumb.x && x <= crumb.x + crumb.w &&
          y >= crumb.y && y <= crumb.y + crumb.h)
      {
        g_app.pendingNavigate = crumb.path;
        return 1;
      }
    }

    if (isOnScrollUp(x, y))
    {
      handleScroll(-1);
      return 1;
    }

    if (isOnScrollDown(x, y))
    {
      handleScroll(1);
      return 1;
    }

    if (!isInListArea(x, y))
      return 1;

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

      if (!g_app.pendingNavigate.empty())
      {
        g_app.searchActive = false;
        g_app.searchQuery.clear();

        if (readDirectory(g_app.pendingNavigate))
        {
          g_app.currentPath = g_app.pendingNavigate;
          g_app.scrollOffset = 0.0f;

          buildUi();
        }

        g_app.pendingNavigate.clear();
      }
    }

    draw();
  }

  shutdownEgl();
}
