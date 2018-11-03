#include <windows.h>
//#include "win_resource.h"
#include <stdio.h>
#include <cstdio>
#include <string>

#include "trace_marker.h"

#pragma comment(lib, "Setupapi.lib")
#pragma comment(lib, "Advapi32.lib")
#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")

#define IDC_BUTTON 0x11

#define SPIN_MS 4000

HWND hWndButton;

LRESULT CALLBACK WndProc (HWND, UINT, WPARAM, LPARAM) ;

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PSTR szCmdLine, int iCmdShow)
{
     static TCHAR szAppName[] = TEXT ("App") ;
     HWND         hwnd ;
     MSG          msg ;
     WNDCLASS     wndclass ;

     wndclass.style         = CS_HREDRAW | CS_VREDRAW ;
     wndclass.lpfnWndProc   = WndProc ;
     wndclass.cbClsExtra    = 0 ;
     wndclass.cbWndExtra    = 0 ;
     wndclass.hInstance     = hInstance ;
     wndclass.hIcon         = LoadIcon (NULL, IDI_APPLICATION) ;
     wndclass.hCursor       = LoadCursor (NULL, IDC_ARROW) ;
     wndclass.hbrBackground = (HBRUSH) GetStockObject (WHITE_BRUSH) ;
     wndclass.lpszMenuName  = NULL ;
     wndclass.lpszClassName = szAppName ;

     if (!RegisterClass (&wndclass))
     {
          MessageBox (NULL, TEXT ("This program requires Windows NT!"),
                      szAppName, MB_ICONERROR) ;
          return 0 ;
     }

     hwnd = CreateWindow (szAppName,                  // window class name
                          TEXT ("App"), // window caption
                          WS_OVERLAPPEDWINDOW,        // window style
                          CW_USEDEFAULT,              // initial x position
                          CW_USEDEFAULT,              // initial y position
                          200,              // initial x size
                          200,              // initial y size
                          NULL,                       // parent window handle
                          NULL,                       // window menu handle
                          hInstance,                  // program instance handle
                          NULL) ;                     // creation parameters

	 //CreateWindow("EDIT", 0, WS_BORDER|WS_CHILD|WS_VISIBLE, 56, 10, 50, 18, hwnd, 0, hInstance, 0);

	hWndButton = CreateWindow("BUTTON", "start spinning", WS_CHILD|WS_VISIBLE, 10, 70, 100, 25, hwnd, (HMENU)(IDC_BUTTON), hInstance, 0);
	// Creates textbox for input
#if 0
HWND hwnd_ed_1 = CreateWindow(WS_EX_CLIENTEDGE, "edit", "Line one",
	WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_LEFT,
	CW_USEDEFAULT, CW_USEDEFAULT, 200, 24,	// x, y, w, h
	hwnd, (HMENU)(101), hInstance);
	//GetWindowLong (hwnd, GWL_HINSTANCE), NULL);
#else
#if 0
HWND hwnd_ed_1 = CreateWindow("edit", "line 1", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_LEFT,
	60, 40, 200, 24,	// x, y, w, h
	hwnd, (HMENU)(101), hInstance, 0);
	//GetWindowLong (hwnd, GWL_HINSTANCE), NULL);
#endif
#endif

#if 0
// char array to hold the text from the textbox
char szInput[MAX_PATH];

// Obtains input from the textbox and puts it into the char array
GetWindowText(GetDlgItem(hwnd, 101), szInput, MAX_PATH);

#define IDC_EDIT1 0x12
 char txt[100] = {0};
 
GetDlgItemText(hwnd,IDC_EDIT1,txt,99);
#endif
     ShowWindow (hwnd, iCmdShow) ;
     UpdateWindow (hwnd) ;
     while (GetMessage (&msg, NULL, 0, 0))
     {
          TranslateMessage (&msg) ;
          DispatchMessage (&msg) ;
     }
     return msg.wParam ;
}

LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
 HDC         hdc ;
 PAINTSTRUCT ps ;
 RECT        rect ;

 switch (message)
 {
 case WM_CREATE:

      return 0 ;

 case WM_COMMAND:
      {
      switch(LOWORD(wParam))
          {
          case IDC_BUTTON:              
              /*
               IDC_BUTTON is id given to button
               This id is specified while creating window. If you are using 
               CreateWindow then it will be as follows:

           CreateWindow("BUTTON", 0, WS_CHILD|WS_VISIBLE, 70, 70, 80, 25, g_hWnd, (HMENU)IDC_BUTTON, hInst, 0);                      

       and in resource.rc file or in the beginning of code. Do "#define IDC_BUTTON                  3456"
        3456 is just a reference you can give any other no as well. Prefer using bigger number so that they can not conflict with existing ones.
              */
              //do whatever you want to do here when button is pressed
			  int i=0;
			  trace_marker_write("Begin spin loop");
			  SetWindowText(hWndButton, "spinning");
			  DWORD ms_cur, ms_beg;
			  ms_beg = GetTickCount();
			  ms_cur = ms_beg;
			  while((ms_cur - ms_beg) < SPIN_MS) { ms_cur = GetTickCount(); i++; }
			  printf("i= %d, interval ms= %d\n", i, ms_cur - ms_beg);
			  SetWindowText(hWndButton, "done");
			  std::string msg = "Spun for "+std::to_string(SPIN_MS)+" msecs";
			  trace_marker_write("Ended spin loop, "+msg);
			  //MessageBox(0, msg.c_str(), "done", MB_OK);
			  //exit(1);
          break;
          }
      }
      break;
 case WM_PAINT:
      hdc = BeginPaint (hwnd, &ps) ;



      EndPaint (hwnd, &ps) ;
      return 0 ;

 case WM_DESTROY:
      PostQuitMessage (0) ;
      return 0 ;
 }
 return DefWindowProc (hwnd, message, wParam, lParam) ;
}


#if 0
LRESULT CALLBACK WndProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     HDC         hdc ;
     PAINTSTRUCT ps ;
     RECT        rect ;

     switch (message)
     {
     case WM_CREATE:

          return 0 ;

     case WM_PAINT:
          hdc = BeginPaint (hwnd, &ps) ;



          EndPaint (hwnd, &ps) ;
          return 0 ;

     case WM_DESTROY:
          PostQuitMessage (0) ;
          return 0 ;
     }
     return DefWindowProc (hwnd, message, wParam, lParam) ;
}
#endif
