#include <string.h>
#include <stdlib.h>

typedef struct chainsaw_Window
{
	void *handle;
	void *device;
	void *context;
	int swap_interval;
	void (*OnWindowMove)(struct chainsaw_Window *);
	void (*OnWindowResize)(struct chainsaw_Window *);
	void (*OnWindowClose)(struct chainsaw_Window *);
	void *user_data;
} chainsaw_Window_t;

#ifdef CHAINSAW_IMPLEMENTATION
void chainsaw_OnWindowMove(chainsaw_Window_t *window, void (*callback)(chainsaw_Window_t *))
{
	window->OnWindowMove = callback;
}

void chainsaw_OnWindowResize(chainsaw_Window_t *window, void (*callback)(chainsaw_Window_t *))
{
	window->OnWindowResize = callback;
}

void chainsaw_OnWindowClose(chainsaw_Window_t *window, void (*callback)(chainsaw_Window_t *))
{
	window->OnWindowClose = callback;
}

#ifdef _WIN32
#include <Windows.h>

static const char *_chainsaw_WindowClassName = "chainsaw_h_WindowClass";

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126

static LRESULT CALLBACK _chainsaw_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	chainsaw_Window_t *window = (chainsaw_Window_t *)GetWindowLongPtrA(hwnd, 0);

	if (window)
	{
		switch (msg)
		{
		case WM_MOVE:
			if (window->OnWindowMove)
				window->OnWindowMove(window);
			return 0;
		case WM_SIZE:
		case WM_SIZING:
			if (window->OnWindowResize)
				window->OnWindowResize(window);
			return 0;
		case WM_CLOSE:
			if (window->OnWindowClose)
				window->OnWindowClose(window);
			return 0;
		}
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

chainsaw_Window_t *chainsaw_WindowCreate(const char *title, const unsigned int width, const unsigned int height, const unsigned int flags)
{
	static int window_class_registered = 0;

	const HINSTANCE instance = GetModuleHandleA(NULL);

	if (!window_class_registered)
	{
		WNDCLASSEXA wnd_class;
		memset(&wnd_class, 0, sizeof(wnd_class));

		wnd_class.cbSize = sizeof(wnd_class);
		wnd_class.style = CS_DBLCLKS | CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
		wnd_class.lpfnWndProc = _chainsaw_WndProc;
		wnd_class.cbWndExtra = sizeof(chainsaw_Window_t *);
		wnd_class.hInstance = instance;
		wnd_class.lpszClassName = _chainsaw_WindowClassName;

		if (!RegisterClassExA(&wnd_class))
		{
			return NULL;
		}
		window_class_registered = 1;
	}

	DWORD style = WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_THICKFRAME | WS_OVERLAPPED | WS_SYSMENU;
	DWORD ex_style = WS_EX_ACCEPTFILES;
	RECT rect = {0, 0, (LONG)width, (LONG)height};

	AdjustWindowRectEx(&rect, style, false, ex_style);

	HWND window_handle = CreateWindowExA(ex_style, _chainsaw_WindowClassName, title, style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, instance, NULL);
	if (!window_handle)
		return NULL;

	chainsaw_Window_t *window = (chainsaw_Window_t *)malloc(sizeof(chainsaw_Window_t));
	if (!window)
	{
		CloseWindow(window_handle);
		return NULL;
	}

	SetWindowLongPtrA(window_handle, 0, (LONG_PTR)window);

	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd, 0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cRedBits = 8;
	pfd.cGreenBits = 8;
	pfd.cBlueBits = 8;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;
	pfd.iLayerType = PFD_MAIN_PLANE;

	HDC device_context = GetDC(window_handle);

	int choosen_pixel_format = ChoosePixelFormat(device_context, &pfd);
	SetPixelFormat(device_context, choosen_pixel_format, &pfd);

	HGLRC gl_context = wglCreateContext(device_context);
	;
	if (!gl_context)
	{
		CloseWindow(window_handle);
		free(window);
		return NULL;
	}

	wglMakeCurrent(device_context, gl_context);

	HGLRC(*wglCreateContextAttribsARB)
	(HDC, HGLRC, const int *) = (HGLRC(*)(HDC, HGLRC, const int *))wglGetProcAddress("wglCreateContextAttribsARB");
	if (wglCreateContextAttribsARB)
	{
		const int gl_attribs[] = {
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 1,
			WGL_CONTEXT_FLAGS_ARB, 0,
			WGL_CONTEXT_PROFILE_MASK_ARB, 1, 0};

		HGLRC modern_gl_context = wglCreateContextAttribsARB(device_context, NULL, gl_attribs);
		if (modern_gl_context)
		{
			wglMakeCurrent(device_context, NULL);
			wglDeleteContext(gl_context);
			gl_context = modern_gl_context;
		}
	}

	wglMakeCurrent(device_context, gl_context);

	memset(window, 0, sizeof(chainsaw_Window_t));
	window->handle = (void *)window_handle;
	window->device = (void *)device_context;

	return window;
}

void chainsaw_WindowShow(chainsaw_Window_t *window)
{
	ShowWindow((HWND)window->handle, SW_SHOW);
}

void chainsaw_WindowHide(chainsaw_Window_t *window)
{
	ShowWindow((HWND)window->handle, SW_HIDE);
}

void chainsaw_WindowDequeueEvents(chainsaw_Window_t *window)
{
	MSG msg;
	while (PeekMessageA(&msg, (HWND)window->handle, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

void chainsaw_WindowSwapBuffers(chainsaw_Window_t *window, int interval)
{
	if (interval != window->swap_interval)
	{
		BOOL(*wglSwapIntervalEXT)
		(int) = (BOOL(*)(int))wglGetProcAddress("wglSwapIntervalEXT");
		if (wglSwapIntervalEXT)
		{
			wglSwapIntervalEXT(interval);
		}
		window->swap_interval = interval;
	}
	SwapBuffers((HDC)window->device);
}

void chainsaw_WindowMove(chainsaw_Window_t *window, unsigned int x, unsigned int y)
{
	RECT rect;
	GetWindowRect((HWND)window->handle, &rect);
	MoveWindow((HWND)window->handle, x, y, rect.right - rect.left, rect.bottom - rect.top, TRUE);
}

void chainsaw_WindowResize(chainsaw_Window_t *window, unsigned int width, unsigned int height)
{
	RECT rect;
	GetWindowRect((HWND)window->handle, &rect);

	DWORD style = GetWindowLongA((HWND)window->handle, GWL_STYLE);
	DWORD ex_style = GetWindowLongA((HWND)window->handle, GWL_EXSTYLE);
	RECT new_rect = {0, 0, (LONG)width, (LONG)height};

	AdjustWindowRectEx(&new_rect, style, false, ex_style);

	MoveWindow((HWND)window->handle, rect.left, rect.top, new_rect.right - new_rect.left, new_rect.bottom - new_rect.top, TRUE);
}

void chainsaw_WindowClose(chainsaw_Window_t *window)
{
	CloseWindow((HWND)window->handle);
	free(window);
}

unsigned int chainsaw_WindowGetWidth(const chainsaw_Window_t *window)
{
	RECT rect;
	GetWindowRect((HWND)window->handle, &rect);

	DWORD style = GetWindowLongA((HWND)window->handle, GWL_STYLE);
	DWORD ex_style = GetWindowLongA((HWND)window->handle, GWL_EXSTYLE);

	AdjustWindowRectEx(&rect, style, false, ex_style);

	return rect.right - rect.left;
}

unsigned int chainsaw_WindowGetHeight(const chainsaw_Window_t *window)
{
	RECT rect;
	GetWindowRect((HWND)window->handle, &rect);

	DWORD style = GetWindowLongA((HWND)window->handle, GWL_STYLE);
	DWORD ex_style = GetWindowLongA((HWND)window->handle, GWL_EXSTYLE);

	AdjustWindowRectEx(&rect, style, false, ex_style);

	return rect.bottom - rect.top;
}

int chainsaw_WindowGetX(const chainsaw_Window_t *window)
{
	RECT rect;
	GetWindowRect((HWND)window->handle, &rect);

	DWORD style = GetWindowLongA((HWND)window->handle, GWL_STYLE);
	DWORD ex_style = GetWindowLongA((HWND)window->handle, GWL_EXSTYLE);

	AdjustWindowRectEx(&rect, style, false, ex_style);

	return rect.left;
}

int chainsaw_WindowGetY(const chainsaw_Window_t *window)
{
	RECT rect;
	GetWindowRect((HWND)window->handle, &rect);

	DWORD style = GetWindowLongA((HWND)window->handle, GWL_STYLE);
	DWORD ex_style = GetWindowLongA((HWND)window->handle, GWL_EXSTYLE);

	AdjustWindowRectEx(&rect, style, false, ex_style);

	return rect.top;
}

unsigned long long chainsaw_Now()
{
	LARGE_INTEGER value;
	QueryPerformanceCounter(&value);
	return (unsigned long long)value.QuadPart;
}
#else
// fallback to GLX
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <time.h>

chainsaw_Window_t *chainsaw_WindowCreate(const char *title, const unsigned int width, const unsigned int height, const unsigned int flags)
{
	Display *display = XOpenDisplay(NULL);
	if (!display)
		return NULL;

	Window root = DefaultRootWindow(display);

	const int attribs[] = {
		GLX_RGBA, 1,
		GLX_DOUBLEBUFFER, 1,
		GLX_RED_SIZE, 8,
		GLX_GREEN_SIZE, 8,
		GLX_BLUE_SIZE, 8,
		GLX_ALPHA_SIZE, 8,
		GLX_DEPTH_SIZE, 24,
		GLX_STENCIL_SIZE, 8,
		0};

	XVisualInfo *vi = glXChooseVisual(display, DefaultScreen(display), (int *)attribs);
	printf("%d\n", vi->bits_per_rgb);

	Colormap color_map = XCreateColormap(display, root, vi->visual, AllocNone);

	XSetWindowAttributes xattr;
	memset(&xattr, 0, sizeof(XSetWindowAttributes));
	xattr.colormap = color_map;
	xattr.event_mask = ResizeRedirectMask;

	Window window_handle = XCreateWindow(display, root, 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &xattr);

	XMapWindow(display, window_handle);

	XStoreName(display, window_handle, title);

	GLXContext context = glXCreateContext(display, vi, NULL, GL_TRUE);
	glXMakeCurrent(display, window_handle, context);

	chainsaw_Window_t *window = (chainsaw_Window_t *)malloc(sizeof(chainsaw_Window_t));
	if (!window)
	{
		return NULL;
	}

	window->handle = (void*)window_handle;
	window->device = display;

}

void chainsaw_WindowHide(chainsaw_Window_t *window)
{
}

void chainsaw_WindowDequeueEvents(chainsaw_Window_t *window)
{
}

void chainsaw_WindowSwapBuffers(chainsaw_Window_t *window, int interval)
{
	glXSwapBuffers((Display*)window->device, (GLXDrawable)window->handle);
}

void chainsaw_WindowMove(chainsaw_Window_t *window, unsigned int x, unsigned int y)
{
}

void chainsaw_WindowShow(chainsaw_Window_t *window)
{
}

unsigned int chainsaw_WindowGetWidth(const chainsaw_Window_t *window)
{
	return 0;
}

unsigned int chainsaw_WindowGetHeight(const chainsaw_Window_t *window)
{
	return 0;
}

void chainsaw_WindowClose(chainsaw_Window_t *window)
{
	free(window);
}

unsigned long long chainsaw_Now()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	unsigned long long now = ts.tv_sec * 1000000000;
	now += (unsigned long long)ts.tv_nsec;
	return now;
}

void chainsaw_WindowResize(chainsaw_Window_t *window, unsigned int width, unsigned int height)
{
}

#endif
#endif
