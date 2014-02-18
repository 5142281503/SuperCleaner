// SuperCleanerDlg.h : header file
//

#pragma once


// CSuperCleanerDlg dialog
class CSuperCleanerDlg : public CDialog
{
// Construction
public:
	CSuperCleanerDlg(CWnd* pParent = NULL);	// standard constructor

// Dialog Data
	enum { IDD = IDD_SUPERCLEANER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnBnClickedClean();
	afx_msg void OnBnClickedDefrag();
};
