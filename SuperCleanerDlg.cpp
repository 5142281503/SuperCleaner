#include "stdafx.h"
#include "SuperCleaner.h"
#include "SuperCleanerDlg.h"
#include <string>
#include <shlwapi.h> // StrCpyNW, PathAddBackslashW, StrNCatW
#include <Wininet.h> // LPINTERNET_CACHE_ENTRY_INFO, FindFirstUrlCacheEntry, COOKIE_CACHE_ENTRY



///////////////////// helpers

std::wstring
GetSystemDir()
{
	const int siz2 = MAX_PATH;
	wchar_t buf2[siz2 + 2] = { 0 };
	GetSystemDirectoryW(buf2, siz2); // %SystemRoot%\system32
	return buf2;
}

std::wstring GetExeDir()
{
	wchar_t strFullPath[MAX_PATH+2] = { 0 } ;
	::GetModuleFileName( NULL, strFullPath, _MAX_PATH );

	wchar_t strDrive[MAX_PATH+2] = {0};
	wchar_t strDir[MAX_PATH+2] = {0};
	_wsplitpath_s( strFullPath, strDrive, MAX_PATH, strDir, MAX_PATH, NULL, 0, NULL, 0 );

	const std::wstring result = std::wstring( strDrive ) + strDir;

	return result;
}


enum RuntimeOS
	{
		OS_Unknown = 0,
		OS_Win311_Win32s,
		OS_WinCE,
		OS_Win9x_Unknown,
		OS_Win95a,
		OS_Win95b,
		OS_Win95c,
		OS_Win98First,
		OS_Win98SE,
		OS_WinME,
		OS_WinNTx_Unknown,
		OS_WinNT350,
		OS_WinNT351,
		OS_WinNT4,
		OS_Win2000,
		OS_WinXP,
		OS_WinVista,
		OS_Win7,
		OS_FutureWin
	};

RuntimeOS 
WhichOS()
{
	OSVERSIONINFO osvi = { sizeof( osvi ), 0  };

	RuntimeOS eTheOS = OS_Unknown;

	if( GetVersionEx( &osvi ) )
	{
		switch( osvi.dwPlatformId )
		{
		case VER_PLATFORM_WIN32s:
			eTheOS = OS_Win311_Win32s;
			break;

		case VER_PLATFORM_WIN32_WINDOWS:
			// Win9x
			eTheOS = OS_Win9x_Unknown;

			if( osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0 )
			{
				eTheOS = OS_Win95a;

				if( osvi.szCSDVersion[1] == 'B' )
					eTheOS = OS_Win95b;
				else if( osvi.szCSDVersion[1] == 'C' )
					eTheOS = OS_Win95c;
			}
			// Win98?
			else if( osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 10 )
			{
				if( LOWORD( osvi.dwBuildNumber ) == 1998 )
					eTheOS = OS_Win98First;
				else if( LOWORD( osvi.dwBuildNumber ) == 2222 )
					eTheOS = OS_Win98SE;
			}
			// WinME?
			else if( osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 90 )
			{
				eTheOS = OS_WinME;
			}

			break;

		case VER_PLATFORM_WIN32_NT:
			eTheOS = OS_WinNTx_Unknown;

			if( osvi.dwMajorVersion == 3 )
			{
				if( osvi.dwMinorVersion == 51 )
					eTheOS = OS_WinNT351;
				else if( osvi.dwMinorVersion == 50 )
					eTheOS = OS_WinNT350;
			}
			else if( osvi.dwMajorVersion == 4 )
			{
				eTheOS = OS_WinNT4;
			}
			else if( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 0 )
			{
				eTheOS = OS_Win2000;
			}
			else if( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1 )
			{
				eTheOS = OS_WinXP;
			}
			else if( osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 2 )
			{
				// 64bit XP and Server 2003 SP1
				eTheOS = OS_WinXP;
			}
			else if( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 0 )
			{
				eTheOS = OS_WinVista;
			}
			else if( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion == 1 )
			{
				eTheOS = OS_Win7;
			}

			// NOTE : If you add a check for a new Windows version here, don't forget to
			// modify the code below (checking for future version of Windows).

			break;

		case 3: // VER_PLATFORM_WIN32_CE
			eTheOS = OS_WinCE;
			break;
		}

		if( osvi.dwMajorVersion == 6 && osvi.dwMinorVersion > 1
			|| osvi.dwMajorVersion > 6 )
		{
			eTheOS = OS_FutureWin;
		}
	}

	return eTheOS;
}

bool IsRunningOn64BitWindows()
{
#if defined(_WIN64)
	return true;
#else
	BOOL bRunningAsWow64Process = FALSE;

	if( !IsWow64Process( GetCurrentProcess(), &bRunningAsWow64Process ) )
	{
		DWORD dwError = GetLastError();
	}

	return bRunningAsWow64Process ? true : false;

#endif
}

bool RunningOn64BitWindows()
{
	static bool bRunningOn64Bits = IsRunningOn64BitWindows();
	return bRunningOn64Bits;
}


#define SAFEDELETEARRAY(x)											{ delete[] (x); (x) = NULL; }

// Use in_sExtension to restrict the files, or specify "L"\\*" for all the files in the folder
// return value is in bytes
unsigned __int64 GetFolderSize( const std::wstring& in_sDirName, bool in_bRecurse, const std::wstring& in_sExtension)
{
	unsigned __int64 retVal = 0;

	WIN32_FIND_DATAW findData = { 0 };

	// start working for files
	HANDLE hFileSearch = ::FindFirstFileW((in_sDirName + in_sExtension ).c_str(), &findData );

	if( hFileSearch != INVALID_HANDLE_VALUE )
	{
		do
		{
			std::wstring strFileName( findData.cFileName );
			
			if( strFileName == L"." || strFileName == L".." )
				continue;

			if( findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				if (in_bRecurse)
				{
					const std::wstring currentDir( in_sDirName + L"\\" + strFileName );

					retVal += GetFolderSize( currentDir, in_bRecurse, in_sExtension );
				}
			}
			else
			{
				ULARGE_INTEGER space = { findData.nFileSizeLow, findData.nFileSizeHigh };
				retVal += space.QuadPart;
			}

		} while( ::FindNextFileW( hFileSearch, &findData ) );
	}

	::FindClose( hFileSearch );

	return retVal;
}

// 'in_path' may or may not terminate with backslash '\'.
// OS may display progress UI, or 'skip' options when a file is in use, when 'in_bShowUI' is set to true.
// Subfolders removed     if 'in_bNoRecursion' is set to false.
// Subfolders left intact if 'in_bNoRecursion' is set to true.
// read-only files are removed.
// If you specify false for 'in_bShowUI', some files may be skipped so this will return false in that case, meaning not all files were removed.
bool EmptyDir(const wchar_t* in_path, const bool in_bSend2RecycleBin, const bool in_bNoRecursion, const bool in_bShowUI)
{
	wchar_t Path[ MAX_PATH+2 ] = { 0 };

	StrCpyNW(Path, in_path, MAX_PATH);
	
	PathAddBackslashW(Path);

	StrNCatW(Path, L"*.*\0", MAX_PATH);

	FILEOP_FLAGS dwFlags = FOF_NOCONFIRMATION;

	if(in_bSend2RecycleBin)
		dwFlags |= FOF_ALLOWUNDO;

	if(in_bNoRecursion)
		dwFlags |= FOF_FILESONLY;

	if(!in_bShowUI)
		dwFlags |= FOF_NO_UI;

	SHFILEOPSTRUCTW  shfs = { ::GetDesktopWindow(), FO_DELETE, Path, NULL, dwFlags, FALSE, 0, 0};

	return 0 == SHFileOperationW(&shfs);
}

bool GetFolderPath( int in_folderCSIDL, std::wstring& out_folderPath )
{
	bool retVal = false;

	wchar_t buffer[MAX_PATH+1] = { 0 };

	if(	SUCCEEDED(	SHGetFolderPath(	NULL, 
										in_folderCSIDL, 
										NULL, 
										SHGFP_TYPE_CURRENT, 
										buffer )))
	{
		out_folderPath = buffer;
		retVal = true;
	}

	return retVal;
}




///////////////////// IE

// async, done is separate thread
bool DeleteIETemporaryFiles()
{
	return (int)ShellExecute(::GetDesktopWindow(), L"open", L"RunDll32.exe", L"InetCpl.cpl,ClearMyTracksByProcess 8", 0, SW_SHOWNORMAL) > 32;
}

// Removes cookies from CSIDL_COOKIES and the protected-mode 'low' folder 
// async, done is separate thread
bool DeleteIECookies()
{
	return (int)ShellExecute(::GetDesktopWindow(), L"open", L"RunDll32.exe", L"InetCpl.cpl,ClearMyTracksByProcess 2", 0, SW_SHOWNORMAL) > 32;
}

// async, done is separate thread
bool DeleteIEHistory()
{
	return (int)ShellExecute(::GetDesktopWindow(), L"open", L"RunDll32.exe", L"InetCpl.cpl,ClearMyTracksByProcess 1", 0, SW_SHOWNORMAL) > 32;
}

// async, done is separate thread
bool DeleteIEFormData()
{
	return (int)ShellExecute(::GetDesktopWindow(), L"open", L"RunDll32.exe", L"InetCpl.cpl,ClearMyTracksByProcess 16", 0, SW_SHOWNORMAL) > 32;
}

// async, done is separate thread
bool DeleteIEPasswords()
{
	return (int)ShellExecute(::GetDesktopWindow(), L"open", L"RunDll32.exe", L"InetCpl.cpl,ClearMyTracksByProcess 32", 0, SW_SHOWNORMAL) > 32;
}

///////////////////// Win

void DeleteWinSearchHistory()
{
	// not implemented
}

bool DeleteWinRunHistory()
{
	if( ERROR_SUCCESS != SHDeleteKey(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\explorer\\RunMRU"))
	{
		//AfxMessageBox(L"Failed to delete recently run history", MB_ICONERROR);
		return false;
	}
	return true;
}

// Deletes all files in the folder, but not subfolders
// The 'Recent Items' folder in Vista shows these, it will be empty after this call.
bool DeleteWinRecentDocuments(const bool in_bSend2RecycleBin)
{
	std::wstring recentDocsPath;

	if(GetFolderPath( CSIDL_RECENT, recentDocsPath )) // Eg: "C:\Users\mario\AppData\Roaming\Microsoft\Windows\Recent", not subfolders
	{
		return EmptyDir(recentDocsPath.c_str(), in_bSend2RecycleBin, true /*probably empty, but safer not to remove sub folders just in case, no recursion*/, true);
	}
	return false;
}

// Empties all the bins on all the drives
bool EmptyAllWinRecycleBins()
{
	if(S_OK != ::SHEmptyRecycleBin( ::GetDesktopWindow(), NULL/*all bins on all drives*/, SHERB_NOCONFIRMATION ) )
	{
		//AfxMessageBox(L"empty recycle bin", MB_ICONERROR);
		return false;
	}
	return true;
}

// Deletes all files and all folders in the folder.
// Recommended option is to use 'true' for 'in_bAllowOsToShowUI',
// Since OS will display progress, and file sizes!
//
// Otherwise we would need to iterate manually and call DeleteFile() for every file, failures are silent.
// Not to mention having to remove the Read-Only flag if present.
bool DeleteWinTempFiles(const bool in_bAllowOsToShowUI, const bool in_bSend2RecycleBin)
{
	wchar_t buffer[MAX_PATH+2] = { 0 };

	if( GetTempPath( MAX_PATH, buffer ) ) // Eg: "C:\Users\mario\AppData\Local\Temp", and sub-folders
	{
		return EmptyDir(buffer, in_bSend2RecycleBin, false /*recurse into directories*/, in_bAllowOsToShowUI);		
	}
	return false;
}

bool DeleteWinPrefetchFiles(const bool in_bAllowOsToShowUI, const bool in_bSend2RecycleBin)
{
	wchar_t buffer[MAX_PATH+2] = { 0 };

	if(GetWindowsDirectory(buffer, MAX_PATH)) // Eg: "C:\Windows\Prefetch", not subfolders
	{
		PathAppendW(buffer, L"Prefetch");

		return EmptyDir(buffer, in_bSend2RecycleBin, true /*safer to avoid potential sub-folders, no recursion*/, in_bAllowOsToShowUI);
	}

	return false;
}


///////////////////// These are separate because they are slow. Don't use them unless necessary!


///////////////////// IE Size APIs

// Cumulative file sizes, including those files in sub folder(s) of the folder.
unsigned __int64 GetIETemporaryFileSizeOnDisk(const int in_Type)
{
	unsigned __int64 usedSpace(0);
	
    bool bDone = false;
    LPINTERNET_CACHE_ENTRY_INFO lpCacheEntry = NULL;  
 
    DWORD  dwEntrySize = 4096; // start buffer size    
    HANDLE hCacheDir = NULL;    
    DWORD  dwError = ERROR_INSUFFICIENT_BUFFER;
	BOOL bSuccess = FALSE;
    
    do 
    {                               
        switch (dwError)
        {
            // need a bigger buffer
            case ERROR_INSUFFICIENT_BUFFER: 
                SAFEDELETEARRAY( lpCacheEntry );
                lpCacheEntry = (LPINTERNET_CACHE_ENTRY_INFO) new char[dwEntrySize];
                lpCacheEntry->dwStructSize = dwEntrySize;
                bSuccess = FALSE;
                
				if (hCacheDir == NULL)                
				{
                    bSuccess = (hCacheDir = FindFirstUrlCacheEntry(NULL, lpCacheEntry,&dwEntrySize)) != NULL;
				}
                else
                    bSuccess = FindNextUrlCacheEntry(hCacheDir, lpCacheEntry, &dwEntrySize);

                if (bSuccess)
                    dwError = ERROR_SUCCESS;    
                else
                {
                    dwError = GetLastError();
                }
                break;

             // we are done
            case ERROR_NO_MORE_ITEMS:
                bDone = true;                
                break;

             // we have got an entry
            case ERROR_SUCCESS:                       
                        
                if (lpCacheEntry->CacheEntryType & in_Type)
				{
					ULARGE_INTEGER space = { lpCacheEntry->dwSizeLow, lpCacheEntry->dwSizeHigh };
					usedSpace += space.QuadPart;	                
				}
                    
                // get ready for next entry
                if (FindNextUrlCacheEntry(hCacheDir, lpCacheEntry, &dwEntrySize))
				{
                    dwError = ERROR_SUCCESS;          
				}
				else
                {
                    dwError = GetLastError();
                }                    
                break;

            // unknown error
            default:
                bDone = true;                
                break;
        }

        if (bDone)
        {   
            SAFEDELETEARRAY( lpCacheEntry );
            if (hCacheDir)
                FindCloseUrlCache(hCacheDir);         
                                  
        }
    } while (!bDone);
	
	return usedSpace;
}

// Includes IE protected mode cookie files
// return value is in bytes
int GetIECookiesSizeOnDisk()
{	
	std::wstring strCookiesPath;

	if(GetFolderPath( CSIDL_COOKIES, strCookiesPath )) 
	{
		strCookiesPath += L"\\low";	// Eg: "C:\Users\mario\AppData\Roaming\Microsoft\Windows\Cookies\low"
	}

	const unsigned __int64 nRet( GetFolderSize(strCookiesPath, false, L"\\*.txt") ); // no recursing, we specify an extension to skip non-cookie files such as index.dat

	const unsigned __int64 nCookies( GetIETemporaryFileSizeOnDisk(COOKIE_CACHE_ENTRY) ); // Eg: "C:\Users\mario\AppData\Roaming\Microsoft\Windows\Cookies\mario@c.msn[1].txt"
	
	return nRet + nCookies;
}

///////////////////// Win Size APIs

// Cumulative file sizes
// return value is in bytes
unsigned __int64 GetWinRecentDocumentsSizeOnDisk()
{
	std::wstring recentDocsPath;
	unsigned __int64 nRet(0);

	if(GetFolderPath( CSIDL_RECENT, recentDocsPath )) // Eg: "C:\Users\mario\AppData\Roaming\Microsoft\Windows\Recent"
	{
		nRet = GetFolderSize(recentDocsPath.c_str(), false, L"\\*"); // not recursing since DeleteWinRecentDocuments() does not
	}

	return nRet;
}


#define MAX_NUMBER_DRIVES 26

// return value is in bytes
unsigned __int64 GetAllWinRecycleBinSizeOnDisk()
{
	SHQUERYRBINFO rbInfo = { sizeof(SHQUERYRBINFO), 0 };
	unsigned __int64 recycleBinSize = 0;
	
	for( int i = 3; i <= MAX_NUMBER_DRIVES; ++i) // Start at two because there is no Recycle bin on Drive A: and B: ( floppy drive )
	{	
		wchar_t driveBuff[10] = { 0 };
		swprintf_s(driveBuff, _countof( driveBuff ), L"%c:\\", i + L'A' - 1);		

		if( DRIVE_FIXED == GetDriveTypeW( driveBuff ) )
		{			
			::SHQueryRecycleBin( driveBuff, &rbInfo );
			recycleBinSize += rbInfo.i64Size;
		
			// reset 'rbInfo' after each call to SHQueryRecycleBin()
			rbInfo.cbSize = sizeof SHQUERYRBINFO;
			rbInfo.i64NumItems = rbInfo.i64Size = 0;
		}
	}


	return recycleBinSize;
}

// The deletion would certainly be less, since some of the files may be locked and in use by Windows and various apps; and un-eraseable.
// return value is in bytes
unsigned __int64 GetWinTempFilesSizeOnDisk()
{
	wchar_t buffer[MAX_PATH+2] = { 0 };
	unsigned __int64 nRet(0);

	if( GetTempPath( MAX_PATH, buffer ) ) // Eg: "C:\Users\mario\AppData\Local\Temp"
	{
		nRet = GetFolderSize(buffer, true, L"\\*"); // recursing since DeleteWinTempFiles() does so
	}

	return nRet;
}

// return value is in bytes
unsigned __int64 GetWinPrefetchFilesSizeOnDisk()
{
	wchar_t buffer[MAX_PATH+2] = { 0 };
	unsigned __int64 nRet(0);

	if(GetWindowsDirectory(buffer, MAX_PATH)) // Eg: "C:\Windows\Prefetch"
	{
		PathAppendW(buffer, L"Prefetch");
		nRet = GetFolderSize(buffer, false, L"\\*"); // not recursing since DeleteWinPrefetchFiles() does not
	}

	return nRet;
}



class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()


// CSuperCleanerDlg dialog




CSuperCleanerDlg::CSuperCleanerDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CSuperCleanerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CSuperCleanerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSuperCleanerDlg, CDialog)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	//}}AFX_MSG_MAP
	ON_BN_CLICKED(IDOK, &CSuperCleanerDlg::OnBnClickedClean)
	ON_BN_CLICKED(IDC_BUTTON1, &CSuperCleanerDlg::OnBnClickedDefrag)
END_MESSAGE_MAP()


// CSuperCleanerDlg message handlers

BOOL CSuperCleanerDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// greying out DeleteWinSearchHistory
	GetDlgItem( IDC_CHECK6 )->EnableWindow(FALSE);

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CSuperCleanerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CSuperCleanerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CSuperCleanerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CSuperCleanerDlg::OnBnClickedClean()
{
	CWaitCursor c;

	// IE

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK1 ))->GetCheck())
	{		
		wchar_t buf[255] = { 0 };
		_snwprintf(buf, 254, L"Removing [%d] bytes of temporary IE files", GetIETemporaryFileSizeOnDisk(NORMAL_CACHE_ENTRY));
		AfxMessageBox(buf, MB_ICONINFORMATION);
		VERIFY(DeleteIETemporaryFiles());
	}

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK2 ))->GetCheck())
	{
		wchar_t buf[255] = { 0 };
		_snwprintf(buf, 254, L"Removing [%d] bytes of IE cookie files", GetIECookiesSizeOnDisk());
		AfxMessageBox(buf, MB_ICONINFORMATION);		
		VERIFY(DeleteIECookies());
	}

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK3 ))->GetCheck())
		VERIFY(DeleteIEHistory());

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK4 ))->GetCheck())
		VERIFY(DeleteIEFormData());

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK5 ))->GetCheck())
		VERIFY(DeleteIEPasswords());

	// Win

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK6 ))->GetCheck())
		DeleteWinSearchHistory();

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK7 ))->GetCheck())
		VERIFY(DeleteWinRunHistory());

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK8 ))->GetCheck())
	{
		wchar_t buf[255] = { 0 };
		_snwprintf(buf, 254, L"Removing [%d] bytes of recent documents", GetWinRecentDocumentsSizeOnDisk());
		AfxMessageBox(buf, MB_ICONINFORMATION);						
		DeleteWinRecentDocuments(true /*send to recycle bin*/);
	}

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK9 ))->GetCheck())
	{
		wchar_t buf[255] = { 0 };
		_snwprintf(buf, 254, L"Removing [%d] bytes of recycle bin data", GetAllWinRecycleBinSizeOnDisk());
		AfxMessageBox(buf, MB_ICONINFORMATION);				
		EmptyAllWinRecycleBins();
	}

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK10 ))->GetCheck())
	{
		wchar_t buf[255] = { 0 };
		_snwprintf(buf, 254, L"Removing [%d] bytes of Windows temporary files", GetWinTempFilesSizeOnDisk());
		AfxMessageBox(buf, MB_ICONINFORMATION);			
		DeleteWinTempFiles(true/*allow OS to show UI*/, true/*send to recycle bin*/);
	}

	if(BST_CHECKED == ((CButton*)GetDlgItem( IDC_CHECK11 ))->GetCheck())
	{
		wchar_t buf[255] = { 0 };
		_snwprintf(buf, 254, L"Removing [%d] bytes of Windows prefetch files", GetWinPrefetchFilesSizeOnDisk());
		AfxMessageBox(buf, MB_ICONINFORMATION);					
		DeleteWinPrefetchFiles(true/*allow OS to show UI*/, true/*send to recycle bin*/);
	}
}



void CSuperCleanerDlg::OnBnClickedDefrag()
{
	#define rp64bitApptoRun L"64DfrgLauncherR.exe"

	bool bRet(false);

	if(OS_WinXP == WhichOS())
	{
		bRet = (int)ShellExecute(::GetDesktopWindow(), L"open",  L"mmc.exe", (GetSystemDir() + L"dfrg.msc").c_str(), 0, SW_SHOWNORMAL) > 32;
	}
	else if(OS_WinVista == WhichOS())
	{
		bRet = (int)ShellExecute(::GetDesktopWindow(), L"runas", L"dfrgui.exe", L"", 0, SW_SHOWNORMAL) > 32;
	}
	else if(OS_Win7 == WhichOS())
	{
		if(RunningOn64BitWindows()) // Exceptional case on Win7 64 Bits. No explanation for this behavior.
		{
			const std::wstring app(GetExeDir() + rp64bitApptoRun);
			bRet = (int)ShellExecute(::GetDesktopWindow(), L"open", app.c_str(), L"", 0, SW_HIDE) > 32;
		}
		else
		{
			bRet = (int)ShellExecute(::GetDesktopWindow(), L"runas", L"dfrgui.exe", L"", 0, SW_SHOWNORMAL) > 32;
		}		
	}
}
