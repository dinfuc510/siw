#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif

#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

#define TITLEBAR_HEIGHT 32
#define TITLE_POS_X 16
#define CAPTION_MENU_WIDTH 46
#define LEFT_PADDING 4

typedef enum CaptionMenu {
	CaptionButton_None,
	CaptionButton_Close,
	CaptionButton_Maximize,
	CaptionButton_Minimize
} CaptionMenu;

static HMODULE g_hmodule;

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
	SetBkColor(hdc, color);
	ExtTextOut(hdc, x, y, ETO_CLIPPED | ETO_OPAQUE, &(RECT) { x, y, x + w, y + h }, NULL, 0, NULL);
}

// https://learn.microsoft.com/en-us/windows/apps/design/style/xaml-theme-resources#the-xaml-type-ramp
// font style: (12px, normal)
// align: left(x), center(y)
int dr_caption(HDC hdc, const char *text, int length, RECT bounds, unsigned long color) {
	static int font_size = 12;
	const char *font_family = "Segoe UI";
	HFONT hfont = CreateFont(-font_size, 0, 0, 0, FW_NORMAL, 0, 0, 0,
							DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
							DEFAULT_QUALITY, DEFAULT_PITCH, font_family);
	HGDIOBJ oldfont = SelectObject(hdc, hfont);

	SIZE text_size_px;
	GetTextExtentPoint32(hdc, text, length, &text_size_px);
	int ellipsis_width_px = 0;
	if (text_size_px.cx > bounds.right - bounds.left) {
		SIZE ellipsis_size_px;
		GetTextExtentPoint32(hdc, "...", 3, &ellipsis_size_px);
		ellipsis_width_px = ellipsis_size_px.cx;
		// TODO: use binary search concept to find the length faster
		while (text_size_px.cx + ellipsis_width_px > bounds.right - bounds.left && length > 1) {
			length--;
			GetTextExtentPoint32(hdc, text, length, &text_size_px);
		}
	}
	bounds.right = bounds.left + text_size_px.cx;

	SetTextColor(hdc, color);
	ExtTextOut(hdc, bounds.left, (bounds.top + bounds.bottom)/2 - text_size_px.cy/2,
				ETO_CLIPPED | ETO_OPAQUE, &bounds, text, length, NULL);
	if (ellipsis_width_px > 0) {
		bounds.left += text_size_px.cx;
		bounds.right = bounds.left + ellipsis_width_px;
		ExtTextOut(hdc, bounds.left, (bounds.top + bounds.bottom)/2 - text_size_px.cy/2,
				ETO_CLIPPED | ETO_OPAQUE, &bounds, "...", 3, NULL);
	}

	SelectObject(hdc, oldfont);
	DeleteObject(hfont);

	return text_size_px.cx + ellipsis_width_px;
}

static void on_draw(HWND hwnd, HDC hdc) {
	const bool has_focus = !!GetFocus();
	const CaptionMenu cur_hovered_button = (CaptionMenu) GetWindowLongPtr(hwnd, GWLP_USERDATA);

	RECT rect;
	GetWindowRect(hwnd, &rect);
	const SIZE window_size = { rect.right - rect.left, rect.bottom - rect.top };

	const bool is_maximized = IsZoomed(hwnd);
	const int border_width = is_maximized ? 0 : 1;
	const unsigned long title_bar_color = has_focus ? 0 : 0x2f2f2f; //bgr 0x4f4f4f 0x2f2f2f 0xb16300
	static unsigned long border_color = 0x4f4f4f;
	static unsigned long background_color = 0x1e1e1e;				//0x0c0c0c
	const unsigned long foreground_color = has_focus ? 0xffffff : 0x7f7f7f;

	{
		dr_rect(hdc, border_width, TITLEBAR_HEIGHT, window_size.cx - border_width*2, window_size.cy - TITLEBAR_HEIGHT - border_width, background_color);
		dr_line(hdc, 0, window_size.cy - border_width, window_size.cx, window_size.cy - border_width, border_width, border_color);
		dr_line(hdc, 0, TITLEBAR_HEIGHT, 0, window_size.cy, border_width, border_color);
		dr_line(hdc, window_size.cx - border_width, TITLEBAR_HEIGHT, window_size.cx - border_width, window_size.cy, border_width, border_color);
	}
	{
		dr_rect(hdc, border_width, border_width, window_size.cx - border_width*2 - CAPTION_MENU_WIDTH*3, TITLEBAR_HEIGHT - border_width, title_bar_color);
		dr_line(hdc, 0, 0, window_size.cx, 0, border_width, border_color);
		dr_line(hdc, 0, 0, 0, TITLEBAR_HEIGHT, border_width, border_color);
		dr_line(hdc, window_size.cx - border_width, 0, window_size.cx - border_width, TITLEBAR_HEIGHT, border_width, border_color);
	}

	int left_padding = LEFT_PADDING;
	const int icon_size = GetSystemMetrics(SM_CXSMICON);
	{
		HICON icon = LoadIcon(NULL, IDI_APPLICATION);
		assert(icon != NULL && "ERROR: could not load icon");
		DrawIconEx(hdc, left_padding + icon_size/2, (TITLEBAR_HEIGHT-border_width)/2 - icon_size/2,
					icon, icon_size, icon_size, 0, NULL, DI_NORMAL | DI_COMPAT);
		DeleteObject(icon);
		left_padding += icon_size*2;
	}
	{
		const int length = GetWindowTextLength(hwnd) + 1;
		char text[MAX_PATH];
		GetWindowText(hwnd, text, length);
		left_padding += dr_caption(hdc, text, length, (RECT){ left_padding, border_width, window_size.cx - CAPTION_MENU_WIDTH*3 - border_width, TITLEBAR_HEIGHT }, foreground_color);
	}

	int right_padding = window_size.cx - border_width - CAPTION_MENU_WIDTH;
	const SIZE button_size = { CAPTION_MENU_WIDTH, TITLEBAR_HEIGHT - border_width };
	POINT button_center = { right_padding + button_size.cx/2, border_width + button_size.cy/2 };
	const int caption_icon_size = 10;
	{
		const unsigned long close_button_color = cur_hovered_button == CaptionButton_Close ? 0xffffff : foreground_color;
		dr_rect(hdc, right_padding, border_width, button_size.cx, button_size.cy, cur_hovered_button == CaptionButton_Close ? 0x2311e8 : title_bar_color);
		dr_line(hdc, button_center.x - caption_icon_size/2, button_center.y - caption_icon_size/2, button_center.x + caption_icon_size/2 + 1, button_center.y + caption_icon_size/2 + 1, 1, close_button_color);
		dr_line(hdc, button_center.x - caption_icon_size/2, button_center.y + caption_icon_size/2, button_center.x + caption_icon_size/2 + 1, button_center.y - caption_icon_size/2 - 1, 1, close_button_color);
		right_padding -= CAPTION_MENU_WIDTH;
		button_center.x -= CAPTION_MENU_WIDTH;
	}
	{
		const unsigned long maximize_button_color = cur_hovered_button == CaptionButton_Maximize ? 0xffffff : foreground_color;
		dr_rect(hdc, right_padding, border_width, button_size.cx, button_size.cy, cur_hovered_button == CaptionButton_Maximize ? 0x1a1a1a : title_bar_color);
		HPEN pen = CreatePen(PS_SOLID, border_width, maximize_button_color);
		HPEN oldpen = (HPEN) SelectObject(hdc, pen);
		SelectObject(hdc, GetStockObject(HOLLOW_BRUSH));
		if (is_maximized) {
			int offset = 2;
			Rectangle(hdc, button_center.x - caption_icon_size/2 + offset, button_center.y - caption_icon_size/2 - offset,
					button_center.x + caption_icon_size/2 + offset, button_center.y + caption_icon_size/2 - offset);
			dr_rect(hdc, button_center.x - caption_icon_size/2, button_center.y - caption_icon_size/2,
					caption_icon_size, caption_icon_size, cur_hovered_button == CaptionButton_Maximize ? 0x1a1a1a : title_bar_color);
		}
		Rectangle(hdc, button_center.x - caption_icon_size/2, button_center.y - caption_icon_size/2,
					button_center.x + caption_icon_size/2, button_center.y + caption_icon_size/2);
		SelectObject(hdc, oldpen);
		DeleteObject(pen);

		right_padding -= CAPTION_MENU_WIDTH;
		button_center.x -= CAPTION_MENU_WIDTH;
	}
	{
		const unsigned long minimize_button_color = cur_hovered_button == CaptionButton_Minimize ? 0xffffff : foreground_color;
		dr_rect(hdc, right_padding, border_width, button_size.cx, button_size.cy, cur_hovered_button == CaptionButton_Minimize ? 0x1a1a1a : title_bar_color);
		dr_line(hdc, button_center.x - caption_icon_size/2, button_center.y, button_center.x + caption_icon_size/2, button_center.y, 1, minimize_button_color);
		right_padding -= CAPTION_MENU_WIDTH;
		button_center.x -= CAPTION_MENU_WIDTH;
	}
}

bool track_mouse_move(HWND hwnd) {
	TRACKMOUSEEVENT tme = {
		.cbSize = sizeof(TRACKMOUSEEVENT),
		.dwFlags = TME_LEAVE,
		.dwHoverTime = 1,
		.hwndTrack = hwnd,
	};
	return TrackMouseEvent(&tme);
}

static LRESULT WinProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	RECT rect;
	GetWindowRect(hwnd, &rect);
	const SIZE window_size = { rect.right - rect.left, rect.bottom - rect.top };
	const CaptionMenu cur_hovered_button = (CaptionMenu) GetWindowLongPtr(hwnd, GWLP_USERDATA);
	const int border_width = IsZoomed(hwnd) ? 0 : 1;
	const int icon_size = GetSystemMetrics(SM_CXSMICON);
	// const RECT icon_rect = { LEFT_PADDING, border_width, icon_size*2, TITLEBAR_HEIGHT };
	const RECT icon_clickable_rect = { LEFT_PADDING+icon_size/2, (TITLEBAR_HEIGHT-border_width)/2 - icon_size/2,
										LEFT_PADDING+icon_size*3/2, (TITLEBAR_HEIGHT-border_width)/2 + icon_size/2 };
	const RECT title_bar_rect = { border_width, border_width, window_size.cx - border_width, TITLEBAR_HEIGHT };
	const RECT client_rect = { border_width, TITLEBAR_HEIGHT, window_size.cx - border_width, window_size.cy - border_width*2 };
	const RECT close_button_paint_rect = { window_size.cx - border_width - CAPTION_MENU_WIDTH, border_width,
											window_size.cx - border_width, TITLEBAR_HEIGHT };
	RECT maximize_button_paint_rect = close_button_paint_rect;
	OffsetRect(&maximize_button_paint_rect, -CAPTION_MENU_WIDTH, 0);
	RECT minimize_button_paint_rect = close_button_paint_rect;
	OffsetRect(&minimize_button_paint_rect, -CAPTION_MENU_WIDTH*2, 0);

	static int border_check_sensitivity = 4;
	RECT close_button_border_check = close_button_paint_rect;
	close_button_border_check.top += border_check_sensitivity;
	close_button_border_check.right -= border_check_sensitivity;
	RECT maximize_button_border_check = maximize_button_paint_rect;
	maximize_button_border_check.top += border_check_sensitivity;
	RECT minimize_button_border_check = minimize_button_paint_rect;
	minimize_button_border_check.top += border_check_sensitivity;

	static bool is_mouse_leave = true;

	switch(msg) {
		case WM_CREATE: {
			SetWindowPos(hwnd, NULL, rect.left, rect.top, window_size.cx, window_size.cy,
						SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE);
	        break;
		}
		case WM_DESTROY: {
			PostQuitMessage(0);
			return 0;
		}
		// https://github.com/grassator/win32-window-custom-titlebar/blob/main/main.c
		case WM_ACTIVATE: {
			if (LOWORD(wparam) == WA_INACTIVE) {
				if (cur_hovered_button != CaptionButton_None) {
					SetWindowLongPtr(hwnd, GWLP_USERDATA, CaptionButton_None);
				}
			}
			InvalidateRect(hwnd, &title_bar_rect, false);
			return 0;
	    }
		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);

#if !defined(DOUBLE_BUFFERING)
			on_draw(hwnd, hdc);
#else
			// https://www.codeproject.com/articles/617212/custom-controls-in-win-api-the-painting
			const int cx = ps.rcPaint.right - ps.rcPaint.left, cy = ps.rcPaint.bottom - ps.rcPaint.top;
			HDC memdc = CreateCompatibleDC(hdc);
			HBITMAP membmp = CreateCompatibleBitmap(hdc, cx, cy);
			assert(memdc != NULL && "ERROR: could not create the memory device context\n");
			assert(membmp != NULL && "ERROR: could not create the memory bitmap\n");

			HGDIOBJ oldbmp = SelectObject(memdc, membmp);
			POINT old_point;
			OffsetViewportOrgEx(memdc, -ps.rcPaint.left, -ps.rcPaint.top, &old_point);
			on_draw(hwnd, memdc);
			SetViewportOrgEx(memdc, old_point.x, old_point.y, NULL);
			BitBlt(hdc, ps.rcPaint.left, ps.rcPaint.top,
					cx, cy, memdc, 0, 0, SRCCOPY);

			SelectObject(memdc, oldbmp);
			DeleteObject(membmp);
			DeleteObject(memdc);
#endif
			EndPaint(hwnd, &ps);
			break;
		}
		case WM_NCHITTEST: {
			POINT mouse = { GET_X_LPARAM(lparam) - rect.left, GET_Y_LPARAM(lparam) - rect.top };
			const int border_width_check = border_width + border_check_sensitivity;

			if (!IsZoomed(hwnd)) {
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
				if (PtInRect(&icon_clickable_rect, mouse)) {
					return HTSYSMENU;
				}
				else if (PtInRect(&close_button_border_check, mouse)) {
					return HTCLOSE;
				}
				else if (PtInRect(&maximize_button_border_check, mouse)) {
					return HTMAXBUTTON;
				}
				else if (PtInRect(&minimize_button_paint_rect, mouse)) {
					return HTMINBUTTON;
				}
				return HTCAPTION;
			}
			else if (PtInRect(&client_rect, mouse)) {
				return HTCLIENT;
			}

			return HTNOWHERE;
		}
		case WM_NCCALCSIZE: {
			if (wparam) {
				return 0;
			}
			break;
		}
		// https://devblogs.microsoft.com/oldnewthing/20100412-00/?p=14353
		case WM_SIZE: {
			if (wparam == SIZE_MAXIMIZED) {
				MONITORINFO mi = { sizeof(mi) };
			    if (GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
					SetWindowPos(hwnd, HWND_TOP,
								mi.rcMonitor.left, mi.rcMonitor.top,
								mi.rcMonitor.right - mi.rcMonitor.left,
								mi.rcMonitor.bottom - mi.rcMonitor.top - 1,	// -1 for not overlap the taskbar (fullscreen)
								SWP_FRAMECHANGED);
				}
			}
			else if (wparam == SIZE_RESTORED) {
				SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
							SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			}
			// RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			InvalidateRect(hwnd, NULL, false);
			break;
		}
		// https://github.com/reactos/reactos/blob/48a0d8e012f77b5d7fb11d58141e1fdd5de27254/base/applications/taskmgr/taskmgr.c#L413
		case WM_SIZING: {
			RECT *drag_rect = (RECT*) lparam;
			SIZE drag_rect_size = { drag_rect->right - drag_rect->left,
									drag_rect->bottom - drag_rect->top };
			const int minimum_width = CAPTION_MENU_WIDTH*3 + border_width*2 + icon_size*2;
			const int minimum_height = TITLEBAR_HEIGHT;

			if (wparam == WMSZ_LEFT || wparam == WMSZ_TOPLEFT || wparam == WMSZ_BOTTOMLEFT) {
				if (drag_rect_size.cx < minimum_width) {
					drag_rect->left = drag_rect->right - minimum_width;
				}
			}
			if (wparam == WMSZ_RIGHT || wparam == WMSZ_TOPRIGHT || wparam == WMSZ_BOTTOMRIGHT) {
				if (drag_rect_size.cx < minimum_width) {
					drag_rect->right = drag_rect->left + minimum_width;
				}
			}
			if (wparam == WMSZ_TOP || wparam == WMSZ_TOPLEFT || wparam == WMSZ_TOPRIGHT) {
				if (drag_rect_size.cy < minimum_height) {
					drag_rect->top = drag_rect->bottom - minimum_height;
				}
			}
			if (wparam == WMSZ_BOTTOM || wparam == WMSZ_BOTTOMLEFT || wparam == WMSZ_BOTTOMRIGHT) {
				if (drag_rect_size.cy < minimum_height) {
					drag_rect->bottom = drag_rect->top + minimum_height;
				}
			}

			return true;
		}
		case WM_MOUSEMOVE: {
			if (cur_hovered_button != CaptionButton_None) {
				InvalidateRect(hwnd, &title_bar_rect, false);
				SetWindowLongPtr(hwnd, GWLP_USERDATA, CaptionButton_None);
			}
			break;
		}
		case WM_NCMOUSELEAVE: {
			if (!is_mouse_leave) {
				is_mouse_leave = true;
				if (cur_hovered_button != CaptionButton_None) {
					InvalidateRect(hwnd, &close_button_paint_rect, false);
			        InvalidateRect(hwnd, &minimize_button_paint_rect, false);
			        InvalidateRect(hwnd, &maximize_button_paint_rect, false);
					SetWindowLongPtr(hwnd, GWLP_USERDATA, CaptionButton_None);
				}
			}
			break;
		}
		case WM_NCMOUSEMOVE: {
			if (is_mouse_leave) {
				track_mouse_move(hwnd);
				is_mouse_leave = false;
			}
			POINT mouse = { GET_X_LPARAM(lparam) - rect.left, GET_Y_LPARAM(lparam) - rect.top };
			CaptionMenu new_hovered_button = CaptionButton_None;
			if (PtInRect(&close_button_border_check, mouse)) {
				new_hovered_button = CaptionButton_Close;
			}
			else if (PtInRect(&maximize_button_border_check, mouse)) {
				new_hovered_button = CaptionButton_Maximize;
			}
			else if (PtInRect(&minimize_button_border_check, mouse)) {
				new_hovered_button = CaptionButton_Minimize;
			}

			if (new_hovered_button != cur_hovered_button) {
				InvalidateRect(hwnd, &close_button_paint_rect, false);
		        InvalidateRect(hwnd, &minimize_button_paint_rect, false);
		        InvalidateRect(hwnd, &maximize_button_paint_rect, false);
		        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) new_hovered_button);
			}
			break;
		}
		case WM_NCLBUTTONDOWN: {
			POINT mouse = { GET_X_LPARAM(lparam) - rect.left, GET_Y_LPARAM(lparam) - rect.top };
			CaptionMenu clicked_button = CaptionButton_None;
			if (PtInRect(&close_button_border_check, mouse)) {
				clicked_button = CaptionButton_Close;
			}
			else if (PtInRect(&maximize_button_border_check, mouse)) {
				clicked_button = CaptionButton_Maximize;
			}
			else if (PtInRect(&minimize_button_border_check, mouse)) {
				clicked_button = CaptionButton_Minimize;
			}
			if (clicked_button != CaptionButton_None) {
		        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR) clicked_button);
				return 0;		// skip default behaviour of caption buttons
			}
			break;
		}
		case WM_NCLBUTTONUP: {
			if (cur_hovered_button == CaptionButton_Close) {
				PostMessage(hwnd, WM_CLOSE, 0, 0);
				return 0;
			}
			else if (cur_hovered_button == CaptionButton_Minimize) {
				ShowWindow(hwnd, SW_MINIMIZE);
				return 0;
			}
			else if (cur_hovered_button == CaptionButton_Maximize) {
				ShowWindow(hwnd, IsZoomed(hwnd) ? SW_RESTORE : SW_MAXIMIZE);
				return 0;
			}
			break;
		}
		case WM_NCRBUTTONDOWN: {
			if (wparam == HTCAPTION) {
				// HMENU hmenu = CreatePopupMenu();
				// if (hmenu != NULL) {
				// 	POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };

				// 	AppendMenu(hmenu, MF_STRING, 1, "Item 1");
				// 	TrackPopupMenuEx(hmenu, TPM_LEFTALIGN, mouse.x, mouse.y, hwnd, NULL);
				// 	DestroyMenu(hmenu);
				// }

				HMENU hmenu = GetSystemMenu(hwnd, false);
				if (hmenu != NULL) {
					POINT mouse = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
					EnableMenuItem(hmenu, SC_MAXIMIZE, IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);
					EnableMenuItem(hmenu, SC_RESTORE, !IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);
					EnableMenuItem(hmenu, SC_SIZE, IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);
					EnableMenuItem(hmenu, SC_MOVE, IsZoomed(hwnd) ? MF_GRAYED : MF_ENABLED);

					int cmd = TrackPopupMenuEx(hmenu, TPM_LEFTALIGN | TPM_RETURNCMD,
												mouse.x, mouse.y, hwnd, NULL);
					if (cmd != 0) {
						PostMessage(hwnd, WM_SYSCOMMAND, cmd, 0);
					}
				}
			}
			return 0;
		}
		// case WM_GETMINMAXINFO: {
		//     MINMAXINFO* mmi = (MINMAXINFO*) lparam;
		//     mmi->ptMinTrackSize.x = CAPTION_MENU_WIDTH*3 + border_width*2;
		//     mmi->ptMinTrackSize.y = TITLEBAR_HEIGHT;
		//     return 0;
		// }
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

bool RegisterWindowClass(const char *lpszClassName, WNDPROC lpfnWndProc) {
	return RegisterClass(&(WNDCLASS) {
		.lpszClassName = lpszClassName,
		.lpfnWndProc = lpfnWndProc,
		// .hInstance = GetModuleHandle(NULL),
		.hCursor = LoadCursor(NULL, IDC_ARROW),
		// .style = CS_PARENTDC// | CS_HREDRAW | CS_VREDRAW
	});
}

int main(void)
{
	g_hmodule = GetModuleHandle(NULL);
	if (g_hmodule == NULL) {
		fprintf(stderr, "ERROR: could not get module handle: %ld\n", GetLastError());
		return 1;
	}

	if (!RegisterWindowClass("Window", WinProc)) {
		fprintf(stderr, "ERROR: could not register class: %ld\n", GetLastError());
		return 1;
	}

	HWND window = CreateWindowEx(0 /*| WS_EX_TOOLWINDOW*/, "Window", "Simple Window",
		WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU,
		0, 0, 700, 500, NULL, NULL, g_hmodule, NULL);
	// EnableMenuItem(GetSystemMenu(window, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_DISABLED | MF_GRAYED);
	if (window == NULL) {
		fprintf(stderr, "ERROR: could not create window: %ld\n", GetLastError());
		UnregisterClass("Window", g_hmodule);
		return 1;
	}
	SetWindowPos(window, NULL, 200, 200, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
	ShowWindow(window, SW_SHOW);

	// https://devblogs.microsoft.com/oldnewthing/20060126-00/?p=32513
	MSG msg;
	while(GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	UnregisterClass("Window", g_hmodule);
	return 0;
}