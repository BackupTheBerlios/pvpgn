 /*
 * Copyright (C) 2001  Erik Latoshek [forester] (laterk@inbox.lv)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#pragma warning(disable : 4047)

#include <shlobj.h>
#include <shlwapi.h>
#include "winmain.h"
#include "common/setup_before.h"
#include <time.h>
#include "common/eventlog.h"
#include "common/version.h"
#include "common/list.h"
#include "bnetd/server.h"
#include "bnetd/connection.h" //by greini needed for the user or connectionlist
#include "bnetd/account_wrap.h"
#include "bnetd/account.h"
#include <string.h>
#include <windows.h>
#include <winuser.h> //amadeo
#include <windowsx.h>
#include <process.h>
#include <stdio.h>
#include <richedit.h>
#include <commctrl.h>
#include "bnetd/message.h" //amadeo
#include "bnetd/resource.h"
#include "bnetd/ipban.h" //zak

#include "common/addr.h" //added for borland, should not be a prob for VS.NET


#define WM_SHELLNOTIFY          (WM_USER+1)

int extern main(int, char*[]);


void static         guiThread(void*);
void static         guiAddText(const char *, COLORREF);
void static         guiAddText_user(const char *, COLORREF);
void static         guiDEAD(char*);
void static         guiMoveWindow(HWND, RECT*);
void static         guiClearLogWindow(void);
void static         guiKillTrayIcon(void);

//by greini eventhandler return value changed
//LRESULT CALLBACK static guiWndProc(HWND, UINT, WPARAM, LPARAM);
long PASCAL guiWndProc(HWND, UINT, WPARAM, LPARAM);
void static         guiOnCommand(HWND, int, HWND, UINT);
void static         guiOnMenuSelect(HWND, HMENU, int, HMENU, UINT);
int  static         guiOnShellNotify(int, int);
BOOL static         guiOnCreate(HWND, LPCREATESTRUCT);
void static         guiOnClose(HWND);
void static         guiOnSize(HWND, UINT, int, int);
void static         guiOnPaint(HWND);
void static         guiOnCaptureChanged(HWND);
BOOL static         guiOnSetCursor(HWND, HWND, UINT, UINT);
void static         guiOnMouseMove(HWND, int, int, UINT);
void static         guiOnLButtonDown(HWND, BOOL, int, int, UINT);
void static         guiOnLButtonUp(HWND, int, int, UINT);
void extern         guiOnUpdateUserList(void); //amadeo
void static         guiOnServerConfig (void); //amadeo
void static         guiOnAbout (HWND); //amadeo
void static         guiOnUpdates (void); //amadeo
void static         guiOnAnnounce (HWND); //amadeo & honni
void static         guiOnUserStatusChange (HWND); //amadeo

BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam); //amadeo
BOOL CALLBACK AnnDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam); //amadeo& honni announcement box
BOOL CALLBACK KickDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam); //amadeo

#define MODE_HDIVIDE    1
#define MODE_VDIVIDE    1
struct gui_struc{
    HWND    hwnd;
    HMENU   hmenuTray;
    HWND    hwndConsole;
    HWND    hwndUsers; //amadeo
	HWND	hwndUserCount; //amadeo
	HWND	hwndUserEditButton;
	HWND    hwndTree;
	//HWND    hwndStatus; //by greini CreateStatusWindow doesn't exist in winxp anymore, but it should work on winnt/98 etc.  
    int     y_ratio;
    int     x_ratio;
	HANDLE  event_ready;
    BOOL    main_finished;
    int     mode;
    char    szDefaultStatus[128];
    
    RECT    rectHDivider,
            rectVDivider,
            rectConsole,
            rectUsers,
            rectConsoleEdge,
            rectUsersEdge;
	       
    WPARAM  wParam;
    LPARAM  lParam;
};

static struct gui_struc gui;
HWND    hButton; //amadeo checkbox-querys for useredit
HWND    hButton1;//amadeo checkbox-querys for useredit
HWND    hButton2;//amadeo checkbox-querys for useredit
HWND    hButton3;//amadeo checkbox-querys for useredit
HWND    hButton4;//amadeo checkbox-querys for useredit
char selected_item[255]; //amadeo: userlist->kick-box
char name1[35]; //by amadeo (for userlist)
char UserCount[80]; //anadeo , neede for UserCount disp
t_connection * conngui; //amadeo needed for kick/ipban users
t_account * accountgui; //amadeo needed for setting admin/mod/ann
const char * gtrue="true"; //needed for setting authorizations
unsigned int i_GUI;	//amadeo my favourite counter :-)
int len; //amadeo don't care :)
int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE reserved,
 LPSTR lpCmdLine, int nCmdShow){
	int result;
	 	    gui.main_finished = FALSE;
    gui.event_ready = CreateEvent(NULL, FALSE, FALSE, NULL);
	_beginthread( guiThread, 0, (void*)hInstance);
    WaitForSingleObject(gui.event_ready, INFINITE);

  //by greini new call of main cause _argc and _argv don't exist in VS
	//result = main(_argc, _argv);
	result = main(__argc ,__argv);
    
	gui.main_finished = TRUE;
    eventlog(eventlog_level_debug,"WinMain","server exited ( return : %i )", result);
    WaitForSingleObject(gui.event_ready, INFINITE);
   
    return 0;
}

  void static guiThread(void *param){
 WNDCLASSEX wc;
 
	MSG msg;

	//InitCommonControls(); //by Greini it seems as if this command is obsolete and not more needed.
    LoadLibrary("RichEd20.dll");
    wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW|CS_VREDRAW;
    wc.lpfnWndProc = (WNDPROC)guiWndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = (HINSTANCE)param;
	wc.hIcon = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_ICON1));
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = 0;
    wc.lpszMenuName = MAKEINTRESOURCE(IDR_MENU);
    wc.lpszClassName = "BnetdWndClass";
    wc.hIconSm = NULL;
    if(!RegisterClassEx( &wc )) guiDEAD("cant register WNDCLASS");
	gui.hwnd = CreateWindowEx(
     0,
     wc.lpszClassName,
     "The Player -vs- Player Gaming Network Server",
     WS_OVERLAPPEDWINDOW,
     CW_USEDEFAULT,
     CW_USEDEFAULT,
     CW_USEDEFAULT,
     CW_USEDEFAULT,
     NULL,
     NULL,
     (HINSTANCE)param,
     NULL);
    if(!gui.hwnd) guiDEAD("cant create window");

    ShowWindow(gui.hwnd, SW_SHOW);
    SetEvent(gui.event_ready);
    while( GetMessage( &msg, NULL, 0, 0 ) ){
        TranslateMessage( &msg );
        DispatchMessage( &msg );
    }

}

//by Greini Eventhandler return value is changed cause VS produced a Type Error 
//LRESULT CALLBACK static guiWndProc(
long PASCAL guiWndProc(
 HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam){

	gui.wParam = wParam;
    gui.lParam = lParam;
	switch(message){
	
        HANDLE_MSG(hwnd, WM_CREATE, guiOnCreate);
        HANDLE_MSG(hwnd, WM_COMMAND, guiOnCommand);
        HANDLE_MSG(hwnd, WM_MENUSELECT, guiOnMenuSelect);
        HANDLE_MSG(hwnd, WM_SIZE, guiOnSize);
        HANDLE_MSG(hwnd, WM_CLOSE, guiOnClose);
        HANDLE_MSG(hwnd, WM_PAINT, guiOnPaint);
        HANDLE_MSG(hwnd, WM_SETCURSOR, guiOnSetCursor);
        HANDLE_MSG(hwnd, WM_LBUTTONDOWN, guiOnLButtonDown);
        HANDLE_MSG(hwnd, WM_LBUTTONUP, guiOnLButtonUp);
        HANDLE_MSG(hwnd, WM_MOUSEMOVE, guiOnMouseMove);
        case WM_CAPTURECHANGED:
            guiOnCaptureChanged((HWND)lParam);
            return 0;
        case WM_SHELLNOTIFY:
            return guiOnShellNotify(wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}


BOOL static guiOnCreate(HWND hwnd, LPCREATESTRUCT lpCreateStruct){

    gui.hwndConsole = CreateWindowEx(
     0,
     RICHEDIT_CLASS,
     NULL,
     WS_CHILD|WS_VISIBLE|ES_READONLY|ES_MULTILINE|WS_VSCROLL|WS_HSCROLL|ES_NOHIDESEL,
     0, 0,
     0, 0,
     hwnd,
     0,
     0,
     NULL);
    if(!gui.hwndConsole) return FALSE;
	
// by amadeo: changed the userlist from riched-field to listbox
	gui.hwndUsers = CreateWindowEx(
     WS_EX_CLIENTEDGE,
     "LISTBOX",
     NULL,
     WS_CHILD|WS_VISIBLE|LBS_STANDARD|LBS_NOINTEGRALHEIGHT,
     0, 0,
     0, 0,
     hwnd,
     0,
     0,
     NULL);
    if(!gui.hwndUsers) return FALSE;

	//amadeo: temp. button for useredit until rightcklick is working....

	gui.hwndUserEditButton = CreateWindow(
		"button",
		"Edit User Status",
		WS_CHILD | WS_VISIBLE | ES_LEFT,
         0, 0,
		 0, 0,
		 hwnd,
		 (HMENU) 881,
         0,
		 NULL) ;
    if(!gui.hwndUserEditButton) return FALSE;
				
	gui.hwndUserCount = CreateWindowEx(
	 WS_EX_CLIENTEDGE,
     "edit",
     " 0 user(s) online:",
     WS_CHILD|WS_VISIBLE|ES_CENTER|ES_READONLY,
     0, 0,
     0, 0,
     hwnd,
     0,
     0,
     NULL);
    if(!gui.hwndUserCount) return FALSE;
	SendMessage(gui.hwndUserCount, WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	SendMessage(gui.hwndUsers, WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	SendMessage(gui.hwndUserEditButton, WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
     BringWindowToTop(gui.hwndUsers);
    strcpy( gui.szDefaultStatus, "Void" );
    // by greini CreateStatusWindow only creates errors under winxp
	/*gui.hwndStatus = CreateStatusWindow(  
    WS_CHILD | WS_VISIBLE,
    gui.szDefaultStatus,
    hwnd,
    0);
    if( !gui.hwndStatus ) return FALSE;*/
	
     // by amadeo commented the code out, because it's not used anymore with resource.rc
    /*gui.hmenuTray = CreatePopupMenu();
    AppendMenu(gui.hmenuTray, MF_STRING, IDM_RESTORE, "&Restore");
    AppendMenu(gui.hmenuTray, MF_SEPARATOR, 0, 0);
    AppendMenu(gui.hmenuTray, MF_STRING, IDM_EXIT, "E&xit");
    SetMenuDefaultItem(gui.hmenuTray, IDM_RESTORE, FALSE);*/
    //by amadeo made changes to adapt to new layout
    gui.y_ratio = (100<<10)/100;
    gui.x_ratio = (0<<10)/100;

    return TRUE;
}


void static guiOnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{

    if( id == IDM_EXIT )
        guiOnClose(hwnd);
    else if( id == IDM_SAVE )
        server_save_wraper();
    else if( id == IDM_RESTART )
        server_restart_wraper();
    else if( id == IDM_SHUTDOWN )
        server_quit_wraper();
    else if( id == IDM_CLEAR )
        guiClearLogWindow();
    else if( id == IDM_RESTORE )
        guiOnShellNotify(IDI_TRAY, WM_LBUTTONDBLCLK);
	//by greini catchs events from Update Userlist in the gui-Menu
	else if(id == IDM_USERLIST)
        guiOnUpdateUserList();
	//by amadeo opens the editor with the server-config-file
	else if(id == IDM_SERVERCONFIG)
		guiOnServerConfig ();
	//by amadeo displays the about-box
	else if(id == IDM_ABOUT)
	guiOnAbout (hwnd);
	//by amadeo opens www.pvpgn.org
	else if(id == ID_HELP_CHECKFORUPDATES)
		guiOnUpdates ();
	//by amadeo & honni opens announcement dialog
	else if(id == IDM_ANN)
		guiOnAnnounce (hwnd);
	//by amadeo: admin-control-panel
	else if(id == ID_USERACTIONS_KICKUSER)
		guiOnUserStatusChange(hwnd);
	else if(id == 881)
		guiOnUserStatusChange(hwnd);
}


void static guiOnMenuSelect(HWND hwnd, HMENU hmenu, int item, HMENU hmenuPopup, UINT flags){
  // char str[256];
   //by greini 
    /*if( item == 0 || flags == -1)
        SetWindowText( gui.hwndStatus, gui.szDefaultStatus );
	else{
        LoadString( GetWindowInstance(hwnd), item, str, sizeof(str) );
        if( str[0] ) SetWindowText( gui.hwndStatus, str );
    }*/
}


int static guiOnShellNotify(int uID, int uMessage){

    if(uID == IDI_TRAY){
        if(uMessage == WM_LBUTTONDBLCLK){
            if( !IsWindowVisible(gui.hwnd) )
                ShowWindow(gui.hwnd, SW_RESTORE);
            SetForegroundWindow(gui.hwnd);
        }else if(uMessage == WM_RBUTTONDOWN){
         POINT cp;
            GetCursorPos(&cp);
            SetForegroundWindow(gui.hwnd);
            TrackPopupMenu(gui.hmenuTray, TPM_LEFTALIGN|TPM_LEFTBUTTON, cp.x, cp.y, 
                0, gui.hwnd, NULL);

        }

    }
 return 0;
}


void static guiOnPaint(HWND hwnd){
 PAINTSTRUCT ps;
 HDC dc;

    dc = BeginPaint(hwnd, &ps);

    DrawEdge(dc, &gui.rectHDivider, BDR_SUNKEN, BF_MIDDLE);
    DrawEdge(dc, &gui.rectConsoleEdge, BDR_SUNKEN, BF_RECT);
    DrawEdge(dc, &gui.rectUsersEdge, BDR_SUNKEN, BF_RECT);

    EndPaint(hwnd, &ps);
  //amadeo
    UpdateWindow(gui.hwndConsole);
    UpdateWindow(gui.hwndUsers);
	UpdateWindow(gui.hwndUserCount);
	UpdateWindow(gui.hwndUserEditButton);

    //by greini
	//UpdateWindow(gui.hwndStatus);
}


void static guiOnClose(HWND hwnd){

    guiKillTrayIcon();
    if( !gui.main_finished ){
     eventlog(eventlog_level_debug,"WinMain","GUI wants server dead...");
     exit(0);
    }else{
     eventlog(eventlog_level_debug,"WinMain","GUI wants to exit...");
     SetEvent(gui.event_ready);
    }

}


void static guiOnSize(HWND hwnd, UINT state, int cx, int cy){
 int cy_console, cy_edge, cx_edge, cy_frame, cy_status;

    if( state == SIZE_MINIMIZED ){
     NOTIFYICONDATA dta;

        dta.cbSize = sizeof(NOTIFYICONDATA);
        dta.hWnd = hwnd;
        dta.uID = IDI_TRAY;
        dta.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
        dta.uCallbackMessage = WM_SHELLNOTIFY;
        dta.hIcon = LoadIcon(GetWindowInstance(hwnd), MAKEINTRESOURCE(IDI_ICON1));
        strcpy(dta.szTip, "PvPGN");
        strcat(dta.szTip, " ");
        strcat(dta.szTip, PVPGN_VERSION);

        Shell_NotifyIcon(NIM_ADD, &dta);
        ShowWindow(hwnd, SW_HIDE);
        return;
    }
    if (state == SIZE_RESTORED)
	{
        NOTIFYICONDATA dta;

        dta.hWnd = hwnd;
        dta.uID = IDI_TRAY;
	    Shell_NotifyIcon(NIM_DELETE,&dta);
	}

	//by greini
    //SendMessage( gui.hwndStatus, WM_SIZE, 0, 0);
    //GetWindowRect( gui.hwndStatus, &rs );
    //cy_status = rs.bottom - rs.top; 
	
	//by amadeo changed the whole fuckin' layout
	cy_status = 0;
    cy_edge = GetSystemMetrics(SM_CYEDGE);
    cx_edge = GetSystemMetrics(SM_CXEDGE);
    cy_frame = (cy_edge<<1) + GetSystemMetrics(SM_CYBORDER) + 1;

    cy_console = ((cy-cy_status-cy_frame-cy_edge*2)*gui.y_ratio)>>10;

    gui.rectConsoleEdge.left = 0;
    gui.rectConsoleEdge.right = cx -140;
    gui.rectConsoleEdge.top = 0;
    gui.rectConsoleEdge.bottom = cy - cy_status;

    gui.rectConsole.left = cx_edge;
    gui.rectConsole.right = cx - 140 -cx_edge;
    gui.rectConsole.top = cy_edge;
    gui.rectConsole.bottom = cy - cy_status;

    gui.rectUsersEdge.left = cx - 140;
    gui.rectUsersEdge.top = 18;
    gui.rectUsersEdge.right = cx;
    gui.rectUsersEdge.bottom = cy - cy_status - 10;

    gui.rectUsers.left = cx -138;
    gui.rectUsers.right = cx ;
    gui.rectUsers.top = 18 + cy_edge;
    gui.rectUsers.bottom = cy - cy_status -20 ;

    guiMoveWindow(gui.hwndConsole, &gui.rectConsole);
	guiMoveWindow(gui.hwndUsers, &gui.rectUsers);
	MoveWindow(gui.hwndUserCount, cx - 140, 0, 140, 18, TRUE); 
	MoveWindow(gui.hwndUserEditButton, cx - 140, cy - cy_status -20, 140, 20, TRUE);
}


BOOL static guiOnSetCursor(HWND hwnd, HWND hwndCursor, UINT codeHitTest, UINT msg){
POINT p;

    if(hwnd == hwndCursor && codeHitTest == HTCLIENT){
        GetCursorPos(&p);
        ScreenToClient(hwnd, &p);
        if(PtInRect(&gui.rectHDivider, p)) SetCursor(LoadCursor(0, IDC_SIZENS));
        return TRUE;
    } 

    return FORWARD_WM_SETCURSOR(hwnd, hwndCursor, codeHitTest, msg, DefWindowProc);
}


void static guiOnLButtonDown(HWND hwnd, BOOL fDoubleClick, int x, int y, UINT keyFlags){
 POINT p;

    p.x = x;
    p.y = y;

    if( PtInRect(&gui.rectHDivider, p) ){
        SetCapture(hwnd);
        gui.mode |= MODE_HDIVIDE;
    }
}


void static guiOnLButtonUp(HWND hwnd, int x, int y, UINT keyFlags){
    ReleaseCapture();
    gui.mode &= ~(MODE_HDIVIDE|MODE_VDIVIDE);
}


void static guiOnCaptureChanged(HWND hwndNewCapture){
    gui.mode &= ~(MODE_HDIVIDE|MODE_VDIVIDE);
}


void static guiOnMouseMove(HWND hwnd, int x, int y, UINT keyFlags){
 int offset, cy_console, cy_users;
 RECT r;
     
    if( gui.mode & MODE_HDIVIDE ){
        offset = y - gui.rectHDivider.top;
        if( !offset ) return;
        cy_console = gui.rectConsole.bottom - gui.rectConsole.top;
        cy_users = gui.rectUsers.bottom - gui.rectUsers.top;

        if( cy_console + offset <= 0)
            offset = -cy_console;
        else if( cy_users - offset <= 0)
            offset = cy_users;

        cy_console += offset;
        cy_users -= offset;
        if( cy_console + cy_users == 0 ) return;
        gui.y_ratio = (cy_console<<10) / (cy_console + cy_users);
        GetClientRect(hwnd, &r);
        guiOnSize(hwnd, 0, r.right, r.bottom);
        InvalidateRect(hwnd, NULL, FALSE);
    }

}


int extern gui_printf(const char *format, ...){
 va_list arglist;
    va_start(arglist, format);
    return gui_lvprintf(eventlog_level_error, format, arglist);
}


int extern gui_lprintf(t_eventlog_level l, const char *format, ...){
 va_list arglist;
    va_start(arglist, format);
    return gui_lvprintf(l, format, arglist);
}

int extern gui_lvprintf(t_eventlog_level l, const char *format, va_list arglist){
 char buff[4096];
 int result;
 COLORREF clr;

    result = vsprintf(buff, format, arglist);

    switch(l){
    case eventlog_level_none:
      clr = RGB(0, 0, 0);
      break;
    case eventlog_level_trace:
      clr = RGB(255, 0, 255);
      break;
    case eventlog_level_debug:
      clr = RGB(0, 0, 255);
      break;
    case eventlog_level_info:
      clr = RGB(0, 0, 0);
      break;
    case eventlog_level_warn:
      clr = RGB(255, 128, 64);
      break;
    case eventlog_level_error:
      clr = RGB(255, 0, 0);
      break;
    eventlog_level_fatal:
      clr = RGB(255, 0, 0);
      break;
    default:
      clr = RGB(0, 0, 0);
    }

    guiAddText(buff, clr);
    return result;
}


void static guiOnUpdates () //by amadeo opens the pvpgn-website
{
		
	ShellExecute(NULL, "open", "www.pvpgn.org", NULL, NULL, SW_SHOW );
}
//by amadeo&honni announcement box
void static guiOnAnnounce (HWND hwnd) //by amadeo&honni announcement box
{
	//DialogBox(hwnd, MAKEINTRESOURCE(IDD_ANN), NULL, AnnDlgProc);
	DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ANN), hwnd, (DLGPROC)AnnDlgProc);
}


//amadeo: admin-control-panel
void static guiOnUserStatusChange (HWND hwnd)
{
int  index;
index = SendMessage(gui.hwndUsers, LB_GETCURSEL, 0, 0);
SendMessage(gui.hwndUsers, LB_GETTEXT, index, (LPARAM)selected_item);
DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_KICKUSER), hwnd, (DLGPROC)KickDlgProc);
SendMessage(gui.hwndUsers, LB_SETCURSEL, -1, 0);

}
void static guiOnAbout (HWND hwnd) //by amadeo displays the about-dialog
{
  DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_ABOUT), hwnd, (DLGPROC)AboutDlgProc);
}
//by amadeo executes notepad to edit the server-config.
void static guiOnServerConfig()
{
	ShellExecute(NULL, "open", "notepad.exe", "conf\\bnetd.conf", NULL, SW_SHOW );
}
//by amadeo: new userlist-handle (based on greini's richedit-window handle)
void extern guiOnUpdateUserList()
{
	const char *name;
	t_connection * c;
	t_elem const * curr;
	t_account * acc;
	static t_list * conn_head_gui; 

	conn_head_gui = connlist();//get the Userlist
	
	if (conn_head_gui == NULL)
		return;
	SendMessage(gui.hwndUsers, LB_RESETCONTENT, 0, 0);

	for (curr=(conn_head_gui)?list_get_first_const(conn_head_gui):(NULL); curr; curr=elem_get_next_const(curr))
	{
		//the connectionstruct is nearly empty it only contains a link on an accountstruct
	    //amadeo fixes some crashes
		if (elem_get_data(curr))
		{
		c = (t_connection *)elem_get_data(curr);
		}
		if (conn_get_account(c)!= NULL)
		{
		acc = conn_get_account(c);
		
		name = (account_get_name(acc));
		SendMessage(gui.hwndUsers, LB_ADDSTRING, 0, (LPARAM)name);

		
		//sessionkey follows
		
		}
	}//amadeo updates USerCount
	sprintf(UserCount, "%d", connlist_login_get_length());
	strcat (UserCount, " user(s) online:");
	SendMessage(gui.hwndUserCount,WM_SETTEXT,0,(LPARAM)UserCount);
}
/* by amadeo: should not be needed anymore... 
//by amadeo writes userlist
void static guiAddText_user (const char *str, COLORREF clr){
	int start_lines, text_length, end_lines;
 CHARRANGE cr;
 CHARFORMAT fmt;
    start_lines = SendMessage(gui.hwndUsers, EM_GETLINECOUNT,0,0);
    text_length = SendMessage(gui.hwndUsers, WM_GETTEXTLENGTH, 0, 0);
    cr.cpMin = text_length;
    cr.cpMax = text_length;
    SendMessage(gui.hwndUsers, EM_EXSETSEL, 0, (LPARAM)&cr); 
	
    fmt.cbSize = sizeof(CHARFORMAT);
    fmt.dwMask = CFM_COLOR|CFM_FACE|CFM_SIZE|
                    CFM_BOLD|CFM_ITALIC|CFM_STRIKEOUT|CFM_UNDERLINE;
    fmt.yHeight = 160;
    fmt.dwEffects = 0;
    fmt.crTextColor = clr;
    strcpy(fmt.szFaceName,"Courier New");

    SendMessage(gui.hwndUsers, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&fmt);
    SendMessage(gui.hwndUsers, EM_REPLACESEL, FALSE, (LPARAM)str);
    end_lines = SendMessage(gui.hwndUsers, EM_GETLINECOUNT,0,0);

    SendMessage(gui.hwndUsers, EM_LINESCROLL, 0, end_lines - start_lines);
}
*/
void static guiAddText(const char *str, COLORREF clr){
 int start_lines, text_length, end_lines;
 CHARRANGE cr;
 CHARFORMAT fmt;
     
	start_lines = SendMessage(gui.hwndConsole, EM_GETLINECOUNT,0,0);
    text_length = SendMessage(gui.hwndConsole, WM_GETTEXTLENGTH, 0, 0);
    cr.cpMin = text_length;
    cr.cpMax = text_length;
    SendMessage(gui.hwndConsole, EM_EXSETSEL, 0, (LPARAM)&cr); 
	
    fmt.cbSize = sizeof(CHARFORMAT);
    fmt.dwMask = CFM_COLOR|CFM_FACE|CFM_SIZE|
                    CFM_BOLD|CFM_ITALIC|CFM_STRIKEOUT|CFM_UNDERLINE;
    fmt.yHeight = 160;
    fmt.dwEffects = 0;
    fmt.crTextColor = clr;
    strcpy(fmt.szFaceName,"Courier New");

    SendMessage(gui.hwndConsole, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&fmt);
    SendMessage(gui.hwndConsole, EM_REPLACESEL, FALSE, (LPARAM)str);
    end_lines = SendMessage(gui.hwndConsole, EM_GETLINECOUNT,0,0);

    SendMessage(gui.hwndConsole, EM_LINESCROLL, 0, end_lines - start_lines);
}



void static guiDEAD(char *message){
 char *nl;
 char errorStr[4096];
 char *msgLastError;
 
    FormatMessage( 
     FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
     NULL,
     GetLastError(),
     MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
     (LPTSTR) &msgLastError,
     0,
     NULL);

    nl = strchr(msgLastError, '\r');
    if(nl) *nl = 0;

    sprintf(errorStr, 
     "%s\n"
     "GetLastError() = '%s'\n",
     message, msgLastError);
 
    LocalFree(msgLastError);
    MessageBox(0, errorStr, "guiDEAD", MB_ICONSTOP|MB_OK);
    exit(1);
}


void static guiMoveWindow(HWND hwnd, RECT* r){
    MoveWindow(hwnd, r->left, r->top, r->right-r->left, r->bottom-r->top, TRUE);
}


void static guiClearLogWindow(void){
    SendMessage(gui.hwndConsole, WM_SETTEXT, 0, 0);
}


void static guiKillTrayIcon(void){
 NOTIFYICONDATA dta;
     
    dta.cbSize = sizeof(NOTIFYICONDATA);
    dta.hWnd = gui.hwnd;
    dta.uID = IDI_TRAY;
    dta.uFlags = 0;
    Shell_NotifyIcon(NIM_DELETE, &dta);
}
//amadeo message-handler for about-box
	BOOL CALLBACK AboutDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    switch(Message)
    {
        case WM_INITDIALOG:

        return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
                    EndDialog(hwnd, IDOK);
                break;
               
            }
        break;
        default:
            return FALSE;
    }
    return TRUE;
}

// fixed version by dedef, fixing a mem leak
BOOL CALLBACK AnnDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{
    t_message *message; //dummy for message structure
	switch(Message)
    {
        case WM_INITDIALOG:
	        return TRUE;
        case WM_COMMAND:
            switch(LOWORD(wParam))
            {
                case IDOK:
				{
					int len = GetWindowTextLength(GetDlgItem(hwnd, IDC_EDIT1));
					if(len > 0)
					{
						char* buf;
						buf = (char*)GlobalAlloc(GPTR, len + 1);
						GetDlgItemText(hwnd, IDC_EDIT1, buf, len + 1);

						//broadcasts the message
						if ((message = message_create(message_type_error,NULL,NULL,buf)))
						{
							message_send_all(message);
							message_destroy(message);
						}
						GlobalFree((HANDLE)buf);
						SetDlgItemText(hwnd, IDC_EDIT1, "");
					} else {
						MessageBox(hwnd, "You didn't enter anything!", "Warning", MB_OK);
					}
		            break;
				}
            }
			break;

        case WM_CLOSE:
			//  free the memory!
			EndDialog(hwnd, IDOK);
            break;
        default:
            return FALSE;
    }
    return TRUE;
}

//amadeo handler for Kick/Ban Users Box	
BOOL CALLBACK KickDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam)
{


	  
   	switch(Message)
    {
        case WM_INITDIALOG:
	   if (selected_item[0]!= 0)
		{
		SetDlgItemText(hwnd, IDC_EDITKICK, selected_item);
		}

        return TRUE;
		    case WM_COMMAND:
			
            switch(LOWORD(wParam))
            {
	               case IDC_KICK_EXECUTE:
					{ //closed
				  	
							BOOL messageq;
							BOOL kickq;
							char temp[60];
							char ipadr[110];
							messageq = FALSE;
							kickq = FALSE;
                            GetDlgItemText(hwnd, IDC_EDITKICK, selected_item, 32);
                            
							conngui = connlist_find_connection_by_accountname(selected_item);
							accountgui = accountlist_find_account(selected_item);

	
						    if (conngui == NULL)
								 {
							 strcat(selected_item," could not be found in Userlist!");
								MessageBox(hwnd,selected_item,"ERROR", MB_OK);
								 }
							else
							{
								


							hButton = GetDlgItem(hwnd, IDC_CHECKBAN);
							hButton1 = GetDlgItem(hwnd, IDC_CHECKKICK);
							hButton2 = GetDlgItem(hwnd, IDC_CHECKADMIN);
							hButton3 = GetDlgItem(hwnd, IDC_CHECKMOD);
							hButton4 = GetDlgItem(hwnd, IDC_CHECKANN);

							//strcpy(gtrue,"true"); 						
						
					   
								   if (SendMessage(hButton2 , BM_GETCHECK, 0, 0)==BST_CHECKED)
								   {
								   account_set_strattr(accountgui,"BNET\\auth\\admin",gtrue);
								   messageq = TRUE;
								   }
								   if (SendMessage(hButton3 , BM_GETCHECK, 0, 0)==BST_CHECKED)
								   {
								   account_set_strattr(accountgui,"BNET\\auth\\operator",gtrue);
								   messageq = TRUE;
								   }
								   if (SendMessage(hButton4 , BM_GETCHECK, 0, 0)==BST_CHECKED)
								   {
								   account_set_strattr(accountgui,"BNET\\auth\\announce",gtrue);
								   messageq = TRUE;
								   }
								   if (SendMessage(hButton , BM_GETCHECK, 0, 0)==BST_CHECKED) 
									{
							
	
									 strcpy (temp, addr_num_to_addr_str(conn_get_addr(conngui), 0)); //get the ip and cut port off
								
									 for (i_GUI=0; temp[i_GUI]!=':'; i_GUI++)
									 ipadr[i_GUI]=temp[i_GUI];
									 ipadr[i_GUI]= 0;
	
								
	
									 strcpy(temp," a "); 
									 strcat(temp,ipadr);
        
									 handle_ipban_command(NULL,temp);
									 temp[0] = 0;
									 strcpy(temp," has been added to IpBanList");
									 strcat(ipadr,temp);
									 if (messageq == TRUE)
									 {
								     strcat(ipadr," and UserStatus changed");
									 MessageBox(hwnd,ipadr,"ipBan & StatusChange", MB_OK);
									 messageq = FALSE;
									 kickq = FALSE;
									 }
									 else
									 MessageBox(hwnd,ipadr,"ipBan", MB_OK);
								}
								    if (SendMessage(hButton1 , BM_GETCHECK, 0, 0)==BST_CHECKED) 
									{
									conn_set_state(conngui,conn_state_destroy);
									kickq = TRUE;
									}		
									if ((messageq == TRUE)&&(kickq == TRUE))
									{
									strcat (selected_item,"has been kicked and Status has changed"); 								
									MessageBox(hwnd,selected_item,"UserKick & StatusChange", MB_OK);
									}
									if ((kickq == TRUE)&&(messageq == FALSE))
									{
 									strcat (selected_item," has been kicked from the server");
									MessageBox(hwnd,selected_item,"UserKick", MB_OK);
									}
									if ((kickq == FALSE)&&(messageq == TRUE))
									{
									strcat (selected_item,"'s Status has been changed");
									MessageBox(hwnd,selected_item,"StatusChange", MB_OK);
									}
							selected_item[0] = 0;
					
						}
							
	
                     break;
				
					}
		
            }
      
		break;
                case WM_CLOSE:
					EndDialog(hwnd, IDC_EDITKICK);
    				break;
        default:
            return FALSE;
    }
    return TRUE;
}
