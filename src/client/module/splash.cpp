#include <std_include.hpp>
#include "loader/module_loader.hpp"
#include "utils/nt.hpp"
#include "utils/hook.hpp"
#include "scheduler.hpp"
#include "game/game.hpp"

class splash final : public module
{
public:
	void post_start() override
	{
		const utils::nt::module self;
		image_ = LoadImageA(self, MAKEINTRESOURCE(IMAGE_SPLASH), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR);
	}
	
	void post_load() override
	{
		this->show();
	}

	void post_unpack() override
	{
		// Disable native splash screen
		utils::hook::nop(SELECT_VALUE(0x14043C15B, 0x140500ADB), 5);
		utils::hook::jump(SELECT_VALUE(0x14043D5A0, 0x140502000), destroy_stub);
		utils::hook::jump(SELECT_VALUE(0x14043D5E0, 0x140502040), destroy_stub);
	}

	void pre_destroy() override
	{
		this->destroy();
		
		MSG msg;
		while (this->window_ && IsWindow(this->window_))
		{
			if (PeekMessageA(&msg, nullptr, NULL, NULL, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				std::this_thread::sleep_for(1ms);
			}
		}

		this->window_ = nullptr;
	}

private:
	HWND window_{};
	HANDLE image_{};

	static void destroy_stub()
	{
		module_loader::get<splash>()->destroy();
	}

	void destroy() const
	{
		if (this->window_ && IsWindow(this->window_))
		{
			ShowWindow(this->window_, SW_HIDE);
			DestroyWindow(this->window_);
			UnregisterClassA("IW6x Splash Screen", utils::nt::module{});
		}
	}

	void show()
	{
		WNDCLASSA wnd_class;

		const utils::nt::module self;

		wnd_class.style = 0;
		wnd_class.cbClsExtra = 0;
		wnd_class.cbWndExtra = 0;
		wnd_class.lpszMenuName = nullptr;
		wnd_class.lpfnWndProc = DefWindowProcA;
		wnd_class.hInstance = self;
		wnd_class.hIcon = LoadIconA(self, LPCSTR(102));
		wnd_class.hCursor = LoadCursorA(nullptr, IDC_APPSTARTING);
		wnd_class.hbrBackground = HBRUSH(6);
		wnd_class.lpszClassName = "IW6x Splash Screen";

		if (RegisterClassA(&wnd_class))
		{
			const auto x_pixels = GetSystemMetrics(SM_CXFULLSCREEN);
			const auto y_pixels = GetSystemMetrics(SM_CYFULLSCREEN);
			
			if (image_)
			{
				this->window_ = CreateWindowExA(WS_EX_APPWINDOW, "IW6x Splash Screen", "IW6x", WS_POPUP | WS_SYSMENU, (x_pixels - 320) / 2, (y_pixels - 100) / 2, 320, 100, nullptr, nullptr, self, nullptr);

				if (this->window_)
				{
					auto* const image_window = CreateWindowExA(0, "Static", nullptr, WS_CHILD | WS_VISIBLE | 0xEu, 0, 0, 320, 100, this->window_, nullptr, self, nullptr);
					if (image_window)
					{
						RECT rect;
						SendMessageA(image_window, 0x172u, 0, LPARAM(image_));
						GetWindowRect(image_window, &rect);

						const int width = rect.right - rect.left;
						rect.left = (x_pixels - width) / 2;

						const int height = rect.bottom - rect.top;
						rect.top = (y_pixels - height) / 2;

						rect.right = rect.left + width;
						rect.bottom = rect.top + height;
						AdjustWindowRect(&rect, WS_CHILD | WS_VISIBLE | 0xEu, 0);
						SetWindowPos(this->window_, nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER);

						ShowWindow(this->window_, SW_SHOW);
						UpdateWindow(this->window_);
					}
				}
			}
		}
	}
};

REGISTER_MODULE(splash)
