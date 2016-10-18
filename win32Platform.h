// win32 platform dependent WSI handling and event loop

#ifndef COMMON_WIN32_WSI_H
#define COMMON_WIN32_WSI_H

#include <functional>

#include <Windows.h>
#include <vulkan/vulkan.h>

#include "CompilerMessages.h"


TODO( "Easier to use, but might prevent platform co-existence. Could be namespaced. Make all of this a class?" )
#define PLATFORM_SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
struct PlatformWindow{ HINSTANCE hInstance; ATOM wndClass; HWND hWnd; };

int messageLoop( bool& quit );
LRESULT CALLBACK wndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam );

bool presentationSupport( VkPhysicalDevice device, uint32_t queueFamilyIndex );

PlatformWindow initWindow( int canvasWidth, int canvasHeight );
void killWindow( PlatformWindow window );

VkSurfaceKHR initSurface( VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueFamily, PlatformWindow window );
// killSurface() is not platform dependent

void setSizeEventHandler( std::function<void(void)> newSizeEventHandler );
void setPaintEventHandler( std::function<void(void)> newPaintEventHandler );

void showWindow( PlatformWindow window );

// In case of non console subsystem just relay to main()
int main();
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow ){
	UNREFERENCED_PARAMETER( hInstance );
	UNREFERENCED_PARAMETER( hPrevInstance );
	UNREFERENCED_PARAMETER( pCmdLine );
	UNREFERENCED_PARAMETER( nCmdShow );

	return main();
}

// Implementation
//////////////////////////////////

void nullHandler(){}

std::function<void(void)> sizeEventHandler = nullHandler;

void setSizeEventHandler( std::function<void(void)> newSizeEventHandler ){
	if( !newSizeEventHandler ) sizeEventHandler = nullHandler;
	sizeEventHandler = newSizeEventHandler;
}

std::function<void(void)> paintEventHandler = nullHandler;

void setPaintEventHandler( std::function<void(void)> newPaintEventHandler ){
	if( !newPaintEventHandler ) paintEventHandler = nullHandler;
	paintEventHandler = newPaintEventHandler;
}

void showWindow( PlatformWindow window ){
	ShowWindow( window.hWnd, SW_SHOW );
	SetForegroundWindow( window.hWnd );
}

int messageLoop(){
	MSG msg;
	BOOL ret = GetMessageW( &msg, NULL, 0, 0 );

	for( ; ret; ret = GetMessageW( &msg, NULL, 0, 0 ) ){
			TranslateMessage( &msg );
			DispatchMessageW( &msg ); //dispatch to wndProc; ignore return from wndProc
	}

	if( ret == -1 ) throw string( "Trouble getting message: " ) + to_string( GetLastError() );

	return static_cast<int>( msg.wParam );
}

LRESULT CALLBACK wndProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ){

	switch( uMsg ){
	case WM_CLOSE:
		PostQuitMessage( 0 );
		return 0;

	TODO( "Probably needs to be implemented to allow seamless resizing." )
	case WM_ERASEBKGND:
		//cout << "erase\n";
		return DefWindowProc( hWnd, uMsg, wParam, lParam );

	case WM_NCCALCSIZE:
		//cout << "nccalcsize " << wParam << " " << lParam << endl;
		return DefWindowProc( hWnd, uMsg, wParam, lParam );

	case WM_SIZE:
		//cout << "size " << wParam << " " << LOWORD( lParam ) << "x" << HIWORD( lParam ) << endl;
		sizeEventHandler();
		return 0;

	case WM_PAINT: // sent after WM_SIZE -- react to this immediately to resize seamlessly
		//ValidateRect( hWnd, NULL ); // never validate so window always gets redrawn
		//cout << "paint\n";
		paintEventHandler();
		return 0;

	case WM_KEYDOWN:
		switch( wParam ){
		case VK_ESCAPE:
			PostQuitMessage( 0 );
			return 0;
		}
		return 0;

	default:
		//cout << "unknown " << to_string( uMsg ) << endl;
		return DefWindowProc( hWnd, uMsg, wParam, lParam );
	}
}


bool presentationSupport( VkPhysicalDevice device, uint32_t queueFamilyIndex ){
	return vkGetPhysicalDeviceWin32PresentationSupportKHR( device, queueFamilyIndex ) == VK_TRUE;
}

ATOM classAtom = 0;
uint64_t classAtomRefCount = 0;

ATOM initWindowClass(){
	HINSTANCE hInstance = GetModuleHandleW( NULL );

	if( classAtom == 0 ){
		WNDCLASSEXW windowClass = {
			sizeof( WNDCLASSEXW ), // cbSize
			CS_OWNDC /*| CS_HREDRAW | CS_VREDRAW*/, // style -- some window behavior
			wndProc, // lpfnWndProc -- set event handler
			0, // cbClsExtra -- set 0 extra bytes after class
			0, // cbWndExtra -- set 0 extra bytes after class instance
			hInstance, // hInstance
			LoadIconW( NULL, IDI_APPLICATION ), // hIcon -- application icon
			LoadCursorW( NULL, IDC_ARROW ), // hCursor -- cursor inside
			NULL, //(HBRUSH)( COLOR_WINDOW + 1 ), // hbrBackground
			NULL, // lpszMenuName -- menu class name
			TEXT("vkwc"), // lpszClassName -- window class name/identificator
			LoadIconW( NULL, IDI_APPLICATION ) // hIconSm
		};

		// register window class
		classAtom = RegisterClassExW( &windowClass );
		if( !classAtom ){
			throw string( "Trouble registering window class: " ) + to_string( GetLastError() );
		}
	}

	++classAtomRefCount;
	return classAtom;
}

PlatformWindow initWindow( int canvasWidth, int canvasHeight ){
	HINSTANCE hInstance = GetModuleHandleW( NULL );

	ATOM wndClassAtom = initWindowClass();

	// adjust size of window to contain given size canvas
	DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;
	RECT windowRect = { 0, 0, canvasWidth, canvasHeight };
	if(  !AdjustWindowRectEx( &windowRect, style, FALSE, exStyle )  ){
		throw string( "Trouble adjusting window size: " ) + to_string( GetLastError() );
	}

	// create window instance
	HWND hWnd =  CreateWindowExW(
		exStyle,
		MAKEINTATOM(wndClassAtom),
		TEXT("Hello Vulkan Triangle"),
		style,
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
		NULL,
		NULL,
		hInstance,
		NULL
	);
	if( !hWnd  ){
		throw string( "Trouble creating window instance: " ) + to_string( GetLastError() );
	}

	return { hInstance, classAtom, hWnd };
}

void killWindow( PlatformWindow window ){
	if(  !DestroyWindow( window.hWnd )  ) throw string( "Trouble destroying window instance: " ) + to_string( GetLastError() );

	--classAtomRefCount;
	if( classAtomRefCount == 0 ){
		if(  !UnregisterClassW( MAKEINTATOM(window.wndClass), window.hInstance )  ){
			throw string( "Trouble unregistering window class: " ) + to_string( GetLastError() );
		}
	}
}

VkSurfaceKHR initSurface( VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t queueFamily, PlatformWindow window ){
	VkWin32SurfaceCreateInfoKHR surfaceInfo{
		VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		nullptr, // pNext for extensions use
		0, // flags - reserved for future use
		window.hInstance,
		window.hWnd
	};


	VkSurfaceKHR surface;
	VkResult errorCode = vkCreateWin32SurfaceKHR( instance, &surfaceInfo, nullptr, &surface ); RESULT_HANDLER( errorCode, "vkCreateWin32SurfaceKHR" );

	// validate WSI support
	VkBool32 supported;
	errorCode = vkGetPhysicalDeviceSurfaceSupportKHR( physicalDevice, queueFamily, surface, &supported ); RESULT_HANDLER( errorCode, "vkGetPhysicalDeviceSurfaceSupportKHR" );
	if( !supported ) throw "Selected queue family of physical device can't present to the surface!";

	return surface;
}


#endif //COMMON_WIN32_WSI_H