#ifndef _WIN32_WINNT
	#define _WIN32_WINNT 	0x0500	/* _WIN32_WINNT_WIN2K (Windows 2000) */
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef DEBUG
#include "message.c"
#endif

#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0500
	#error "ERROR: _WIN32_WINNT must be defined and at least 0x0500"
	/*
		...
		#define _WIN32_WINNT 0x0500
		#include <windows.h>
		...
	*/
#endif

#ifndef GCLP_HICONSM
	#define GCLP_HICONSM 		(-34)
#endif

#ifndef GetClassLongPtr
	#ifdef GetClassLong
		#define GetClassLongPtr	GetClassLong
	#endif
#endif

#ifndef TME_NONCLIENT
	#define TME_NONCLIENT 		0x00000010
#endif

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define TITLEBAR_HEIGHT 32
#define TITLE_POS_X 16
#define CAPTION_MENU_WIDTH 46
#define CAPTION_ICON_SIZE 10
#define LEFT_PADDING 8
#define SYSMENU_HIGHLIGHT_SIZE 4
#define SYSMENU_HIGHLIGHT_BORDER_WIDTH 1
#define BORDER_WIDTH 1

typedef struct UserData {
	LONG_PTR flags;
	RECT normal_pos;
} UserData;

#define CAPTION_BUTTON_BIT 				0
#define CAPTION_BUTTON_BIT_LENGTH 		3
typedef enum CaptionButton {
	CaptionButton_None,
	CaptionButton_Close,
	CaptionButton_Maximize,
	CaptionButton_Minimize,
	CaptionButton_Sysmenu,
} CaptionButton;

#define IS_MOUSE_LEAVE_BIT 				(CAPTION_BUTTON_BIT + CAPTION_BUTTON_BIT_LENGTH)
#define IS_MOUSE_LEAVE_BIT_LENGTH 		1
#define MAXIMIZE_SNAPPING_BIT 			(IS_MOUSE_LEAVE_BIT + IS_MOUSE_LEAVE_BIT_LENGTH)
#define MAXIMIZE_SNAPPING_BIT_LENGTH 	1
#define IS_TASKBAR_HIDDEN_BIT 			(MAXIMIZE_SNAPPING_BIT + MAXIMIZE_SNAPPING_BIT_LENGTH)
#define IS_TASKBAR_HIDDEN_BIT_LENGTH 	1

LONG_PTR get_nbits_from_ith(LONG_PTR num, unsigned char start, unsigned char count) {
	assert(start + count <= 8*sizeof(LONG_PTR));
	return (num >> start) & ((1 << count) - 1);
}

void set_nbits_from_ith(LONG_PTR *num, unsigned char start, unsigned char count, LONG_PTR value) {
	assert(start + count <= 8*sizeof(LONG_PTR));
	assert((value >> count) == 0);
	assert(num != NULL);
	*num = (*num & ~(((1 << count) - 1) << start)) | (value << start);
}

LONG_PTR get_flag(HWND hwnd, unsigned char start, unsigned char count) {
	UserData *user_data = (UserData*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (user_data == NULL) {
		return 0;
	}
	LONG_PTR prop = get_nbits_from_ith(user_data->flags, start, count);
	return prop;
}

void set_flag(HWND hwnd, unsigned char start, unsigned char count, LONG_PTR value) {
	UserData *user_data = (UserData*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (user_data == NULL) {
		return;
	}
	set_nbits_from_ith(&user_data->flags, start, count, value);
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) user_data);
}

RECT* get_normal_pos(HWND hwnd) {
	UserData *user_data = (UserData*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (user_data == NULL) {
		return NULL;
	}
	return &user_data->normal_pos;
}

void set_normal_pos(HWND hwnd, RECT *pos) {
	UserData *user_data = (UserData*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (user_data == NULL) {
		return;
	}
	user_data->normal_pos.left = pos->left;
	user_data->normal_pos.top = pos->top;
	user_data->normal_pos.right = pos->right;
	user_data->normal_pos.bottom = pos->bottom;
	SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) user_data);
}

/* NOTE: Both bg and fg are in rgb */
unsigned long blend_color(unsigned long bg, unsigned long fg, unsigned char alpha) {
	int blue = ((bg & 0xff) * (255 - alpha) + (fg & 0xff) * alpha) / 255;
	bg >>= 8; fg >>= 8;
	int green = ((bg & 0xff) * (255 - alpha) + (fg & 0xff) * alpha) / 255;
	bg >>= 8; fg >>= 8;
	int red = ((bg & 0xff) * (255 - alpha) + (fg & 0xff) * alpha) / 255;
	return (blue << 16) | (green << 8) | red;						/* the winapi use bgr */
}


void dr_line(HDC hdc, int x1, int y1, int x2, int y2, int border_width, unsigned long color) {
	if (border_width == 0) {
		return;
	}
	HPEN pen = CreatePen(PS_SOLID, border_width, color);
	HPEN oldpen = (HPEN) SelectObject(hdc, pen);
	POINT old_point;
	MoveToEx(hdc, x1, y1, &old_point);
	LineTo(hdc, x2, y2);
	MoveToEx(hdc, old_point.x, old_point.y, NULL);
	SelectObject(hdc, oldpen);
	DeleteObject(pen);
}

void dr_rect(HDC hdc, int x, int y, int w, int h, unsigned long color) {
	unsigned long old_color = SetBkColor(hdc, color);
	ExtTextOut(hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &(RECT) { x, y, x + w, y + h }, NULL, 0, NULL);
	SetBkColor(hdc, old_color);
}

void dr_rect_line(HDC hdc, int x, int y, int w, int h, int border_width, unsigned long color) {
	if (border_width == 0) {
		return;
	}
	HPEN pen = CreatePen(PS_SOLID, border_width, color);
	HPEN oldpen = (HPEN) SelectObject(hdc, pen);
	POINT old_point;
	MoveToEx(hdc, x, y, &old_point);
	LineTo(hdc, x + w - border_width, y);
	LineTo(hdc, x + w - border_width, y + h - border_width);
	LineTo(hdc, x, y + h - border_width);
	LineTo(hdc, x, y);
	MoveToEx(hdc, old_point.x, old_point.y, NULL);
	SelectObject(hdc, oldpen);
	DeleteObject(pen);
}

/* https://learn.microsoft.com/en-us/windows/apps/design/style/xaml-theme-resources#the-xaml-type-ramp
   font style: (12px, normal)
   align: left(x), center(y) */
int dr_caption(HDC hdc, const char *text, int length, RECT bounds, unsigned long color) {
	static int font_size = 12;
	const char *font_family = "Segoe UI";
	HFONT hfont = CreateFont(-font_size, 0, 0, 0, FW_NORMAL, 0, 0, 0,
							DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY, DEFAULT_PITCH, font_family);
	if (hfont == NULL) {
		HFONT def_font = GetStockObject(DEFAULT_GUI_FONT);
		LOGFONT lf;
		GetObject(def_font, sizeof(LOGFONT), &lf);
		lf.lfHeight = -font_size;
		hfont = CreateFontIndirect(&lf);
	}
	assert(hfont != NULL && "ERROR: could not create font");
	HGDIOBJ oldfont = SelectObject(hdc, hfont);

	SIZE text_size_px;
	GetTextExtentPoint32(hdc, text, length, &text_size_px);
	int ellipsis_width_px = 0;
	if (text_size_px.cx > bounds.right - bounds.left) {
		SIZE ellipsis_size_px;
		GetTextExtentPoint32(hdc, "...", 3, &ellipsis_size_px);
		ellipsis_width_px = ellipsis_size_px.cx;
		/* TODO: use binary search concept to find the length faster */
		while (text_size_px.cx + ellipsis_width_px > bounds.right - bounds.left && length > 1) {
			length--;
			GetTextExtentPoint32(hdc, text, length, &text_size_px);
		}
	}
	bounds.right = bounds.left + text_size_px.cx;

	SetTextColor(hdc, color);
	int old_mode = SetBkMode(hdc, TRANSPARENT);
	ExtTextOut(hdc, bounds.left, (bounds.top + bounds.bottom)/2 - text_size_px.cy/2,
				ETO_CLIPPED | ETO_OPAQUE, NULL, text, length, NULL);
	if (ellipsis_width_px > 0) {
		bounds.left += text_size_px.cx;
		bounds.right = bounds.left + ellipsis_width_px;
		ExtTextOut(hdc, bounds.left, (bounds.top + bounds.bottom)/2 - text_size_px.cy/2,
				ETO_CLIPPED | ETO_OPAQUE, NULL, "...", 3, NULL);
	}
	SetBkMode(hdc, old_mode);

	SelectObject(hdc, oldfont);
	DeleteObject(hfont);

	return text_size_px.cx + ellipsis_width_px;
}

bool is_taskbar_hidden(HWND hwnd) {
	MONITORINFO mi;
	mi.cbSize = sizeof(MONITORINFO);
	if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
		return EqualRect(&mi.rcWork, &mi.rcMonitor);
	}
	return false;
}

bool set_maximize_window(HWND hwnd) {
	MONITORINFO mi;
	mi.cbSize = sizeof(MONITORINFO);
	if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi)) {
		/* to not overlap the autohide taskbar (fullscreen)
		   this will affect to the snapping behaviour on Windows 10
		   The solution is handle WM_WINDOWPOSCHANGING (see below) */
		if (get_flag(hwnd, IS_TASKBAR_HIDDEN_BIT, IS_TASKBAR_HIDDEN_BIT_LENGTH)) {
			APPBARDATA abd;
			abd.cbSize = sizeof(APPBARDATA);
			abd.uEdge = ABE_BOTTOM;
			if ((HWND) SHAppBarMessage(ABM_GETAUTOHIDEBAR, &abd) != NULL) {
				mi.rcWork.bottom -= 1;
			}
			abd.uEdge = ABE_RIGHT;
			if ((HWND) SHAppBarMessage(ABM_GETAUTOHIDEBAR, &abd) != NULL) {
				mi.rcWork.right -= 1;
			}
			abd.uEdge = ABE_TOP;
			if ((HWND) SHAppBarMessage(ABM_GETAUTOHIDEBAR, &abd) != NULL) {
				mi.rcWork.top += 1;
			}
			abd.uEdge = ABE_LEFT;
			if ((HWND) SHAppBarMessage(ABM_GETAUTOHIDEBAR, &abd) != NULL) {
				mi.rcWork.left += 1;
			}
		}
		return SetWindowPos(hwnd, NULL,
						mi.rcWork.left, mi.rcWork.top,
						mi.rcWork.right - mi.rcWork.left,
						mi.rcWork.bottom - mi.rcWork.top,
						SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS);
	}

	return false;
}

/* https://devblogs.microsoft.com/oldnewthing/20110520-00/?p=10613 */
static void on_draw(HWND hwnd, HDC hdc) {
	bool has_focus = !!GetFocus();
	CaptionButton cur_hovered_button = (CaptionButton) get_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH);
	bool is_maximized = IsZoomed(hwnd);

	RECT rect;
	GetWindowRect(hwnd, &rect);
	SIZE window_size = { rect.right - rect.left, rect.bottom - rect.top };

	int border_width = is_maximized ? 0 : BORDER_WIDTH;
	unsigned long title_bar_color = has_focus ? 0 : 0x2f2f2f; /* bgr 0x4f4f4f 0x2f2f2f 0xb16300 */
	static unsigned long border_color = 0x4f4f4f;
	static unsigned long background_color = 0x1e1e1e;				/* 0x0c0c0c */
	unsigned long foreground_color = has_focus ? 0xffffff : 0x7f7f7f;

	{
		dr_rect(hdc, border_width, TITLEBAR_HEIGHT, window_size.cx - border_width*2, window_size.cy - TITLEBAR_HEIGHT - border_width, background_color);
		dr_line(hdc, 0, window_size.cy - border_width/2 - (border_width&1), window_size.cx, window_size.cy - border_width/2-(border_width&1), border_width, border_color);
		dr_line(hdc, 0, TITLEBAR_HEIGHT, 0, window_size.cy, border_width*2, border_color);
		dr_line(hdc, window_size.cx - border_width/2-(border_width&1), TITLEBAR_HEIGHT, window_size.cx - border_width/2-(border_width&1), window_size.cy, border_width, border_color);
	}
	{
		dr_rect(hdc, border_width, border_width, window_size.cx - border_width*2 - CAPTION_MENU_WIDTH*3, TITLEBAR_HEIGHT - border_width, title_bar_color);
		dr_line(hdc, 0, 0, window_size.cx, 0, border_width*2, border_color);
		dr_line(hdc, 0, 0, 0, TITLEBAR_HEIGHT, border_width*2, border_color);
		dr_line(hdc, window_size.cx - border_width/2-(border_width&1), 0, window_size.cx - border_width/2-(border_width&1), TITLEBAR_HEIGHT, border_width, border_color);
	}

	int left_padding = (LEFT_PADDING > (border_width*2 + SYSMENU_HIGHLIGHT_SIZE) ? LEFT_PADDING : border_width*2 + SYSMENU_HIGHLIGHT_SIZE);
	{
		SIZE sysmenu_size = { GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) };
		unsigned long sysmenu_color = cur_hovered_button == CaptionButton_Sysmenu ?
											blend_color(title_bar_color, foreground_color, 20) : title_bar_color;
		HICON sysmenu_icon = NULL;
#ifdef GetClassLongPtr
		if (sysmenu_icon == NULL) {
			sysmenu_icon = (HICON) GetClassLongPtr(hwnd, GCLP_HICONSM);
		}
#endif
		if (sysmenu_icon == NULL) {
			sysmenu_icon = LoadIcon(NULL, IDI_APPLICATION);
		}
		assert(sysmenu_icon != NULL && "ERROR: could not load sysmenu icon");
		/* https://devblogs.microsoft.com/oldnewthing/20101020-00/?p=12493 */
		dr_rect(hdc, left_padding-SYSMENU_HIGHLIGHT_SIZE, border_width + (TITLEBAR_HEIGHT-border_width)/2 - (sysmenu_size.cy + SYSMENU_HIGHLIGHT_SIZE*2)/2,
				sysmenu_size.cx + SYSMENU_HIGHLIGHT_SIZE*2, sysmenu_size.cy + SYSMENU_HIGHLIGHT_SIZE*2, sysmenu_color);
		HBRUSH hbr = CreateSolidBrush(sysmenu_color); 	/* GetSysColorBrush(COLOR_MENU) */
		DrawIconEx(hdc, left_padding, border_width + (TITLEBAR_HEIGHT-border_width)/2 - sysmenu_size.cy/2, sysmenu_icon,
				sysmenu_size.cx, sysmenu_size.cy, 0, hbr, DI_NORMAL | DI_COMPAT);
		DeleteObject(hbr);
		if (cur_hovered_button == CaptionButton_Sysmenu) {
			dr_rect_line(hdc, left_padding-SYSMENU_HIGHLIGHT_SIZE, border_width + (TITLEBAR_HEIGHT-border_width)/2 - (sysmenu_size.cy + SYSMENU_HIGHLIGHT_SIZE*2)/2,
					sysmenu_size.cx + SYSMENU_HIGHLIGHT_SIZE*2, sysmenu_size.cy + SYSMENU_HIGHLIGHT_SIZE*2, SYSMENU_HIGHLIGHT_BORDER_WIDTH, blend_color(sysmenu_color, foreground_color, 20));
		}
		left_padding += sysmenu_size.cx + SYSMENU_HIGHLIGHT_SIZE + /* padding */ 1;
	}
	{
		int length = GetWindowTextLength(hwnd);
		char text[MAX_PATH];
		GetWindowText(hwnd, text, length + 1);
		left_padding += dr_caption(hdc, text, length, (RECT) { left_padding, border_width, window_size.cx - CAPTION_MENU_WIDTH*3 - border_width, TITLEBAR_HEIGHT }, foreground_color);
	}

	SIZE button_size = { CAPTION_MENU_WIDTH, TITLEBAR_HEIGHT - border_width };
	int right_padding = window_size.cx - border_width - button_size.cx;
	POINT button_center = { right_padding + button_size.cx/2, border_width + button_size.cy/2 };
	int caption_icon_size = CAPTION_ICON_SIZE;
	{
		unsigned long close_button_color = cur_hovered_button == CaptionButton_Close ? 0xffffff : foreground_color;
		dr_rect(hdc, right_padding, border_width, button_size.cx, button_size.cy, cur_hovered_button == CaptionButton_Close ? 0x2311e8 : title_bar_color);
		dr_line(hdc, button_center.x - caption_icon_size/2, button_center.y - caption_icon_size/2, button_center.x + caption_icon_size/2 + 1, button_center.y + caption_icon_size/2 + 1, 1, close_button_color);
		dr_line(hdc, button_center.x - caption_icon_size/2, button_center.y + caption_icon_size/2, button_center.x + caption_icon_size/2 + 1, button_center.y - caption_icon_size/2 - 1, 1, close_button_color);
		right_padding -= button_size.cx;
		button_center.x -= button_size.cx;
	}
	{
		unsigned long maximize_button_color = cur_hovered_button == CaptionButton_Maximize ? 0xffffff : foreground_color;
		dr_rect(hdc, right_padding, border_width, button_size.cx, button_size.cy, cur_hovered_button == CaptionButton_Maximize ? 0x1a1a1a : title_bar_color);
		if (is_maximized) {
			int offset = 2;
			dr_rect_line(hdc, button_center.x - caption_icon_size/2 + offset, button_center.y - caption_icon_size/2 - offset,
						caption_icon_size, caption_icon_size, 1, maximize_button_color);
			dr_rect(hdc, button_center.x - caption_icon_size/2, button_center.y - caption_icon_size/2,
					caption_icon_size, caption_icon_size, cur_hovered_button == CaptionButton_Maximize ? 0x1a1a1a : title_bar_color);
		}
		dr_rect_line(hdc, button_center.x - caption_icon_size/2, button_center.y - caption_icon_size/2,
						caption_icon_size, caption_icon_size, 1, maximize_button_color);

		right_padding -= button_size.cx;
		button_center.x -= button_size.cx;
	}
	{
		unsigned long minimize_button_color = cur_hovered_button == CaptionButton_Minimize ? 0xffffff : foreground_color;
		dr_rect(hdc, right_padding, border_width, button_size.cx, button_size.cy, cur_hovered_button == CaptionButton_Minimize ? 0x1a1a1a : title_bar_color);
		dr_line(hdc, button_center.x - caption_icon_size/2, button_center.y, button_center.x + caption_icon_size/2, button_center.y, 1, minimize_button_color);
		right_padding -= button_size.cx;
		button_center.x -= button_size.cx;
	}
}

static bool register_window_class(const char *class, WNDPROC proc) {
	return RegisterClassEx(&(WNDCLASSEX) {
		.cbSize = sizeof(WNDCLASSEX),
		.lpszClassName = class,
		.lpfnWndProc = proc,
		.hIcon = LoadIcon(NULL, IDI_APPLICATION),
		.style = CS_OWNDC
	});
}

/* https://github.com/cwabbott0/wine-hangover/blob/1736d902f713e30e3d46552ca7c790c0836c44d9/dlls/comctl32/button.c#L674 */
bool track_mouse_leave(HWND hwnd) {
	return TrackMouseEvent(&(TRACKMOUSEEVENT) {
		.cbSize = sizeof(TRACKMOUSEEVENT),
		.dwFlags = TME_NONCLIENT | TME_LEAVE,
		.hwndTrack = hwnd,
	});
}

static LRESULT win_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	RECT rect;
	GetWindowRect(hwnd, &rect);
	bool is_mouse_leave = (bool) get_flag(hwnd, IS_MOUSE_LEAVE_BIT, IS_MOUSE_LEAVE_BIT_LENGTH);
	bool is_maximized = IsZoomed(hwnd);
	SIZE window_size = { rect.right - rect.left, rect.bottom - rect.top };
	CaptionButton cur_hovered_button = (CaptionButton) get_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH);
	int border_width = is_maximized ? 0 : BORDER_WIDTH;
	SIZE sysmenu_size = { GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON) };
	int left_padding = (LEFT_PADDING > (border_width*2 + SYSMENU_HIGHLIGHT_SIZE) ? LEFT_PADDING : border_width*2 + SYSMENU_HIGHLIGHT_SIZE);
	RECT sysmenu_paint_rect = {
		.left = left_padding-SYSMENU_HIGHLIGHT_SIZE,
		.top = border_width + (TITLEBAR_HEIGHT-border_width)/2 - (sysmenu_size.cy + SYSMENU_HIGHLIGHT_SIZE*2)/2,
	};
	sysmenu_paint_rect.right = sysmenu_paint_rect.left + sysmenu_size.cx + SYSMENU_HIGHLIGHT_SIZE*2;
	sysmenu_paint_rect.bottom = sysmenu_paint_rect.top + sysmenu_size.cy + SYSMENU_HIGHLIGHT_SIZE*2;
	RECT client_rect = { border_width, TITLEBAR_HEIGHT, window_size.cx - border_width, window_size.cy - border_width };
	RECT title_bar_rect = { border_width, border_width, window_size.cx - border_width, TITLEBAR_HEIGHT };
	RECT close_button_paint_rect = { window_size.cx - border_width - CAPTION_MENU_WIDTH, border_width,
											window_size.cx - border_width, TITLEBAR_HEIGHT };
	RECT maximize_button_paint_rect = close_button_paint_rect;
	OffsetRect(&maximize_button_paint_rect, -CAPTION_MENU_WIDTH, 0);
	RECT minimize_button_paint_rect = close_button_paint_rect;
	OffsetRect(&minimize_button_paint_rect, -CAPTION_MENU_WIDTH*2, 0);

#ifdef DEBUG
	if (print_message) {
		if (msg < messages_len && messages[msg] != NULL) {
			printf("%s\n", messages[msg]);
		}
		else {
			printf("0x%04x\n", msg);
		}
	}
	if (msg == WM_CHAR && wparam == 'p') {
		print_message = !print_message;
	}
#endif

	switch(msg) {
		case WM_CREATE: {
			RECT dummy_rect = { 0, 0, 0, 0 };
			if (register_window_class("DWindow", DefWindowProc)) {
				HWND dummy = CreateWindow("DWindow", "Dummy Window", WS_OVERLAPPED,
				CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
				NULL, NULL, NULL, NULL);
				if (dummy != NULL) {
					GetWindowRect(dummy, &dummy_rect);
					DestroyWindow(dummy);
				}
				/* UnregisterClass("DWindow", g_hmodule); */
			}

			if (rect.left <= 0) {
				rect.left = dummy_rect.left;
			}
			/* https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowa
			   rect.top might be ignore */
			if (rect.top <= 0) {
				rect.top = dummy_rect.top;
			}
			if (window_size.cx < CAPTION_MENU_WIDTH*3 + border_width*2 + GetSystemMetrics(SM_CXSMICON) + LEFT_PADDING) {
				window_size.cx = dummy_rect.right - dummy_rect.left;
			}
			if (window_size.cy < TITLEBAR_HEIGHT) {
				window_size.cy = dummy_rect.bottom - dummy_rect.top;
			}

			SetWindowPos(hwnd, NULL, rect.left, rect.top, window_size.cx, window_size.cy,
						SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOREDRAW | SWP_NOCOPYBITS);
			/* trigger the program create system menu */
			(void) GetSystemMenu(hwnd, false);
			UserData *user_data = (UserData*) calloc(1, sizeof(UserData));
			assert(user_data != NULL);
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) user_data);
			set_flag(hwnd, IS_MOUSE_LEAVE_BIT, IS_MOUSE_LEAVE_BIT_LENGTH, true);
			set_flag(hwnd, IS_TASKBAR_HIDDEN_BIT, IS_TASKBAR_HIDDEN_BIT_LENGTH, is_taskbar_hidden(hwnd));
			set_normal_pos(hwnd, &rect);
			break;
		}
		case WM_DESTROY: {
			UserData *user_data = (UserData*) GetWindowLongPtr(hwnd, GWLP_USERDATA);
			free(user_data);
			/* TODO: save window's position and size when close by hold ctrl then click X button */
			/* https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-writeprivateprofilestringa */
			PostQuitMessage(0);
			break;
		}
		/* https://github.com/grassator/win32-window-custom-titlebar/blob/main/main.c */
		case WM_ACTIVATE: {
			if (LOWORD(wparam) == WA_INACTIVE) {
				if (cur_hovered_button != CaptionButton_None) {
					set_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH, CaptionButton_None);
				}
			}
			InvalidateRect(hwnd, &title_bar_rect, false);
			return 0;
		}
		case WM_NCACTIVATE: {
			/* redraw take too long when hold inactive titlebar */
			lparam = -1;
			return true;
		}
		case WM_NCPAINT: {
			return true;
		}
		case WM_PAINT: {
			PAINTSTRUCT ps;
			BeginPaint(hwnd, &ps);
#ifndef DOUBLE_BUFFERING
			on_draw(hwnd, ps.hdc);
#else
			/* https://www.codeproject.com/articles/617212/custom-controls-in-win-api-the-painting */
			int cx = ps.rcPaint.right - ps.rcPaint.left, cy = ps.rcPaint.bottom - ps.rcPaint.top;
			HDC memdc = CreateCompatibleDC(ps.hdc);
			HBITMAP membmp = CreateCompatibleBitmap(ps.hdc, cx, cy);
			assert(memdc != NULL && "ERROR: could not create the memory device context");
			assert(membmp != NULL && "ERROR: could not create the memory bitmap");

			HGDIOBJ oldbmp = SelectObject(memdc, membmp);
			POINT old_point;
			OffsetViewportOrgEx(memdc, -ps.rcPaint.left, -ps.rcPaint.top, &old_point);
			on_draw(hwnd, memdc);
			SetViewportOrgEx(memdc, old_point.x, old_point.y, NULL);
			BitBlt(ps.hdc, ps.rcPaint.left, ps.rcPaint.top,
					cx, cy, memdc, 0, 0, SRCCOPY);

			SelectObject(memdc, oldbmp);
			DeleteObject(membmp);
			DeleteObject(memdc);
#endif
			EndPaint(hwnd, &ps);
			return 0;
		}
		case WM_NCHITTEST: {
			int border_check_sensitivity = is_maximized ? 0 : 4;
			POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
			MapWindowPoints(NULL, hwnd, &mouse, 1 /*number of points*/);
			int border_width_check = border_width + border_check_sensitivity;

			if (!is_maximized) {
				if (mouse.x < border_width_check && mouse.y < border_width_check) {
					return HTTOPLEFT;
				}
				else if (mouse.x >= window_size.cx - border_width_check && mouse.y < border_width_check) {
					return HTTOPRIGHT;
				}
				else if (mouse.x < border_width_check && mouse.y >= window_size.cy - border_width_check) {
					return HTBOTTOMLEFT;
				}
				else if (mouse.x >= window_size.cx - border_width_check && mouse.y >= window_size.cy - border_width_check) {
					return HTBOTTOMRIGHT;
				}
				else if (mouse.y < border_width_check) {
					return HTTOP;
				}
				else if (mouse.y >= window_size.cy - border_width_check) {
					return HTBOTTOM;
				}
				else if (mouse.x < border_width_check) {
					return HTLEFT;
				}
				else if (mouse.x >= window_size.cx - border_width_check) {
					return HTRIGHT;
				}
			}
			if (mouse.y < TITLEBAR_HEIGHT) {
				RECT close_button_border_check = close_button_paint_rect;
				close_button_border_check.top += border_check_sensitivity;
				close_button_border_check.right -= border_check_sensitivity;
				RECT maximize_button_border_check = maximize_button_paint_rect;
				maximize_button_border_check.top += border_check_sensitivity;
				RECT minimize_button_border_check = minimize_button_paint_rect;
				minimize_button_border_check.top += border_check_sensitivity;
				if (PtInRect(&sysmenu_paint_rect, mouse)) {
					return HTSYSMENU;
				}
				else if (PtInRect(&close_button_border_check, mouse)) {
					return HTCLOSE;
				}
				else if (PtInRect(&maximize_button_border_check, mouse)) {
					return HTMAXBUTTON;
				}
				else if (PtInRect(&minimize_button_border_check, mouse)) {
					return HTMINBUTTON;
				}
				if (!!GetFocus()) {
					return HTCAPTION;							/* when not focus and hold titlebar (no move) */
				}												/* there will be a delay in repaint titlebar */
				return HTCLIENT;								/* so we must return HTCLIENT */
			}
			else if (PtInRect(&client_rect, mouse)) {
				return HTCLIENT;
			}

			return HTNOWHERE;
		}
		/* https://stackoverflow.com/questions/53000291/how-to-smooth-ugly-jitter-flicker-jumping-when-resizing-windows-especially-drag */
		/* https://github.com/Thomas-Mielke-Software/ECTImport/blob/ff4ba7b31a4a220c801029a10bc5305a7c9fca71/ResizableLayout.cpp#L858 */
		case WM_NCCALCSIZE: {
			if (wparam == true) {
				NCCALCSIZE_PARAMS *params = (NCCALCSIZE_PARAMS*) lparam;
				if (!is_maximized || get_flag(hwnd, IS_TASKBAR_HIDDEN_BIT, IS_TASKBAR_HIDDEN_BIT_LENGTH)) {
					params->rgrc[0].bottom += border_width;
				}
				return WVR_VALIDRECTS;			/* make the resize smoothly */
			}
			return 0;							/* disable default behaviour
												   when right click menu shown,
												   an old style caption button appear */
		}
		case WM_GETMINMAXINFO: {
			MINMAXINFO *mmi = (MINMAXINFO*) lparam;
			mmi->ptMinTrackSize.x = CAPTION_MENU_WIDTH*3 + border_width*2 + sysmenu_paint_rect.right;
			mmi->ptMinTrackSize.y = TITLEBAR_HEIGHT + border_width*2;
			break;
		}
		case WM_MOUSEMOVE: {
			if (GetCapture()) {
				PostMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, lparam);
				/* force redraw */
				RedrawWindow(hwnd, &title_bar_rect, NULL, RDW_INVALIDATE | RDW_NOERASE | RDW_NOFRAME | RDW_UPDATENOW);
				ReleaseCapture();
			}
			if (cur_hovered_button != CaptionButton_None) {
				InvalidateRect(hwnd, &title_bar_rect, false);
				set_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH, CaptionButton_None);
			}
			break;
		}
		case WM_NCMOUSELEAVE: {
			if (!is_mouse_leave) {
				set_flag(hwnd, IS_MOUSE_LEAVE_BIT, IS_MOUSE_LEAVE_BIT_LENGTH, true);
				if (cur_hovered_button != CaptionButton_None) {
					InvalidateRect(hwnd, &close_button_paint_rect, false);
					InvalidateRect(hwnd, &minimize_button_paint_rect, false);
					InvalidateRect(hwnd, &maximize_button_paint_rect, false);
					InvalidateRect(hwnd, &sysmenu_paint_rect, false);
					set_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH, CaptionButton_None);
				}
			}
			break;
		}
		case WM_NCMOUSEMOVE: {
			if (is_mouse_leave) {
				track_mouse_leave(hwnd);
				set_flag(hwnd, IS_MOUSE_LEAVE_BIT, IS_MOUSE_LEAVE_BIT_LENGTH, false);
			}
			int hit_test = wparam;
			CaptionButton new_hovered_button = CaptionButton_None;
			if (hit_test == HTCLOSE) {
				new_hovered_button = CaptionButton_Close;
			}
			else if (hit_test == HTMAXBUTTON) {
				new_hovered_button = CaptionButton_Maximize;
			}
			else if (hit_test == HTMINBUTTON) {
				new_hovered_button = CaptionButton_Minimize;
			}
			else if (hit_test == HTSYSMENU) {
				new_hovered_button = CaptionButton_Sysmenu;
			}

			if (new_hovered_button != cur_hovered_button) {
				InvalidateRect(hwnd, &close_button_paint_rect, false);
				InvalidateRect(hwnd, &minimize_button_paint_rect, false);
				InvalidateRect(hwnd, &maximize_button_paint_rect, false);
				InvalidateRect(hwnd, &sysmenu_paint_rect, false);
				set_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH, new_hovered_button);
			}
			break;
		}
		case WM_NCLBUTTONDOWN: {
			int hit_test = wparam;
			CaptionButton clicked_button = CaptionButton_None;
			if (hit_test == HTCLOSE) {
				clicked_button = CaptionButton_Close;
			}
			else if (hit_test == HTMAXBUTTON) {
				clicked_button = CaptionButton_Maximize;
			}
			else if (hit_test == HTMINBUTTON) {
				clicked_button = CaptionButton_Minimize;
			}
			else if (hit_test == HTSYSMENU) {
				clicked_button = CaptionButton_Sysmenu;
			}
			if (clicked_button != CaptionButton_None) {
				set_flag(hwnd, CAPTION_BUTTON_BIT, CAPTION_BUTTON_BIT_LENGTH, clicked_button);
				if (clicked_button != CaptionButton_Sysmenu) {
					return 0;		/* skip default behaviour of caption buttons except sysmenu */
				}
			}
			break;
		}
		/* https://github.com/microsoft/terminal/blob/3486111722296f287158e0340789c607642c1067/src/cascadia/TerminalApp/TitlebarControl.cpp#L97 */
		case WM_NCLBUTTONUP: {
			int hit_test = wparam;
			if (hit_test == HTCLOSE) {
				PostMessage(hwnd, WM_SYSCOMMAND, SC_CLOSE, 0);
				return 0;
			}
			else if (hit_test == HTMINBUTTON) {
				PostMessage(hwnd, WM_SYSCOMMAND, SC_MINIMIZE | HTMINBUTTON, 0);
				return 0;
			}
			else if (hit_test == HTMAXBUTTON) {
				PostMessage(hwnd, WM_SYSCOMMAND, (is_maximized ? SC_RESTORE : SC_MAXIMIZE) | HTMAXBUTTON, MAKELPARAM(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
				return 0;
			}
			break;
		}
		case WM_NCRBUTTONUP: {
			if (wparam == HTCAPTION) {
				HMENU hmenu = GetSystemMenu(hwnd, false);
				if (hmenu != NULL) {
					POINT menu_pos = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
					int cmd = TrackPopupMenuEx(hmenu,
							TPM_RIGHTBUTTON | TPM_RETURNCMD | (GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0 ? TPM_LEFTALIGN : TPM_RIGHTALIGN),
							menu_pos.x, menu_pos.y, hwnd, NULL);
					if (cmd != 0) {
						PostMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
					}
				}
			}
			return 0;
		}
		case WM_LBUTTONDOWN: {
			if (GET_Y_LPARAM(lparam) <= TITLEBAR_HEIGHT) {
				SetCapture(hwnd);
			}
			break;
		}
		case WM_LBUTTONUP: {
			ReleaseCapture();
			break;
		}
		case WM_SYSCOMMAND: {
			int request = (wparam & 0xFFF0);
			bool display_menu = false;
			if (request == SC_KEYMENU) {
				char key_stroke = (char) lparam;
				if (key_stroke == ' ') {
					display_menu = true;
				}
			}
			else if (request == SC_MOUSEMENU) {
				display_menu = true;
			}

			if (display_menu) {
				HMENU hmenu = GetSystemMenu(hwnd, false);
				if (hmenu != NULL) {
					int cmd = TrackPopupMenuEx(hmenu,
							TPM_RIGHTBUTTON | TPM_RETURNCMD | (GetSystemMetrics(SM_MENUDROPALIGNMENT) == 0 ? TPM_LEFTALIGN : TPM_RIGHTALIGN),
							rect.left, rect.top+TITLEBAR_HEIGHT, hwnd, NULL);
					if (cmd != 0) {
						PostMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
					}
				}
				return 0;
			}
			break;
		}
		case WM_INITMENUPOPUP: {
			bool is_system_menu = HIWORD(lparam);
			HMENU hmenu = (HMENU) wparam;
			if (is_system_menu || GetSystemMenu(hwnd, false) == hmenu) {
				EnableMenuItem(hmenu, SC_MAXIMIZE, is_maximized ? MF_GRAYED : MF_ENABLED);
				EnableMenuItem(hmenu, SC_RESTORE, !is_maximized ? MF_GRAYED : MF_ENABLED);
				EnableMenuItem(hmenu, SC_SIZE, is_maximized ? MF_GRAYED : MF_ENABLED);
				EnableMenuItem(hmenu, SC_MOVE, is_maximized ? MF_GRAYED : MF_ENABLED);
				return 0;
			}
			break;
		}
		case WM_SETCURSOR: {
			int hit_test = LOWORD(lparam);
			if (hit_test == HTSYSMENU) {
				SetCursor(LoadCursor(NULL, IDC_HAND));
				return true;
			}
			SetCursor(LoadCursor(NULL, IDC_ARROW));
			break;
		}
		case WM_WINDOWPOSCHANGING: {
			WINDOWPOS* wpos = (WINDOWPOS*) lparam;
			if (wpos->flags == 0x308020) {
				set_flag(hwnd, MAXIMIZE_SNAPPING_BIT, MAXIMIZE_SNAPPING_BIT_LENGTH, true);
			}
			else if (wpos->flags == 0x300204) {
				if (get_flag(hwnd, MAXIMIZE_SNAPPING_BIT, MAXIMIZE_SNAPPING_BIT_LENGTH)) {
					if (get_flag(hwnd, IS_TASKBAR_HIDDEN_BIT, IS_TASKBAR_HIDDEN_BIT_LENGTH)) {
						wpos->cy *= 2;
					}
					int system_border_width = GetSystemMetrics(SM_CYFRAME);
					wpos->cy += 2*system_border_width;
					wpos->y -= system_border_width;
					set_flag(hwnd, MAXIMIZE_SNAPPING_BIT, MAXIMIZE_SNAPPING_BIT_LENGTH, false);
				}
			}
			break;
		}
		/* https://github.com/atauzki/notepad2/blob/5984878391ebbd649eaf566a0982a1a26be5deea/src/Notepad2.c#L1117 */
		/* https://github.com/eval1749/evita/blob/4e9dd86009af091e213a208270ef9bd429fbb698/evita/ui/widget.cc#L807 */
		case WM_WINDOWPOSCHANGED: {
			WINDOWPOS* wpos = (WINDOWPOS*) lparam;
			/* wpos->flags |= SWP_NOCOPYBITS;					cause unnecessary redraw when moving */
			if (!(wpos->flags & SWP_NOSIZE)) {
				/* https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353 */
				if (is_maximized) {
					set_maximize_window(hwnd);
				}
				InvalidateRect(hwnd, NULL, true);
				return 0;
			}
			if ((wpos->flags & SWP_NOSIZE) && !(wpos->flags & SWP_NOMOVE) && (wpos->flags & SWP_NOZORDER)) {
				SetCursor(LoadCursor(NULL, IDC_SIZEALL));
			}
			break;
		}
		case WM_EXITSIZEMOVE: {
			if (!IsIconic(hwnd)) {
				set_normal_pos(hwnd, &rect);
			}
			break;
		}
		case WM_SETTINGCHANGE: {
			if (wparam == SPI_SETWORKAREA) {
				WINDOWPLACEMENT wp = { .length = sizeof(WINDOWPLACEMENT) };
				if (GetWindowPlacement(hwnd, &wp)) {
					RECT *normal_pos = get_normal_pos(hwnd);
					if (normal_pos != NULL) {
						wp.rcNormalPosition = *normal_pos;
						SetWindowPlacement(hwnd, &wp);
					}
				}
				set_flag(hwnd, IS_TASKBAR_HIDDEN_BIT, IS_TASKBAR_HIDDEN_BIT_LENGTH, is_taskbar_hidden(hwnd));
				if (is_maximized) {
					set_maximize_window(hwnd);
				}
			}
			break;
		}
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

int main(void)
{
	HMODULE g_hmodule = GetModuleHandle(NULL);
	if (g_hmodule == NULL) {
		fprintf(stderr, "ERROR: could not get module handle: %ld\n", GetLastError());
		return 1;
	}

	if (!register_window_class("SWindow", (WNDPROC) win_proc)) {
		fprintf(stderr, "ERROR: could not register class: %ld\n", GetLastError());
		return 1;
	}

	HWND window = CreateWindowEx(0 /*| WS_EX_TOOLWINDOW*/, "SWindow", "Simple Window",
		WS_POPUP | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 700, 500, NULL, NULL, g_hmodule, NULL);
	if (window == NULL) {
		fprintf(stderr, "ERROR: could not create window: %ld\n", GetLastError());
		UnregisterClass("SWindow", g_hmodule);
		return 1;
	}

	/* https://devblogs.microsoft.com/oldnewthing/20060126-00/?p=32513 */
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	/* UnregisterClass("SWindow", g_hmodule); */
	return 0;
}

/*
TODO:
	dpi
	layered window
	make this become library
*/
