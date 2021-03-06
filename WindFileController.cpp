#include "StdAfx.h"
#include "windfilecontroller.h"

// we also need the meterological data
#include "MeteorologicalData.h"

// ... and the settings
#include "Configuration/Configuration.h"

// ... and the FTP-communication handle
#include "Communication/FTPCom.h"


extern CConfigurationSetting g_settings;	// <-- the settings
extern CMeteorologicalData g_metData;			// <-- The meteorological data
extern CFormView *pView;									// <-- the main window

IMPLEMENT_DYNCREATE(CWindFileController, CWinThread)

BEGIN_MESSAGE_MAP(CWindFileController, CWinThread)
	ON_THREAD_MESSAGE(WM_QUIT,						OnQuit)
	ON_THREAD_MESSAGE(WM_TIMER,						OnTimer)
END_MESSAGE_MAP()

CWindFileController::CWindFileController(void)
{
	m_nTimerID = 0;
}

CWindFileController::~CWindFileController(void)
{
}

/** Quits the thread */
void CWindFileController::OnQuit(WPARAM wp, LPARAM lp)
{
}

void CWindFileController::OnTimer(UINT nIDEvent, LPARAM lp){
	// Try to find and read in a wind-field file, if any can be found...
	if(g_settings.windSourceSettings.enabled == 1 && g_settings.windSourceSettings.windFieldFile.GetLength() > 0){

		// If the file is a local file...
		if(IsExistingFile(g_settings.windSourceSettings.windFieldFile)){
			if(0 == g_metData.ReadWindFieldFromFile(g_settings.windSourceSettings.windFieldFile)){
				ShowMessage("Successfully re-read wind-field from file");
				pView->PostMessage(WM_NEW_WINDFIELD, NULL, NULL);
			}
			return; // SUCCESS!!
		}

		// If the file exists on an FTP-server
		if(Equals(g_settings.windSourceSettings.windFieldFile.Left(6), "ftp://")){
			DownloadFileByFTP();
		}
	}
}

BOOL CWindFileController::OnIdle(LONG lCount){

	return FALSE; // no more idle-time is needed
}

BOOL CWindFileController::InitInstance(){
	CWinThread::InitInstance();

	// Check the global settings if we are to be re-loading the wind-field file
	//	every now and then then set the timer
	if(g_settings.windSourceSettings.enabled = 1 && g_settings.windSourceSettings.windFileReloadInterval > 0 && g_settings.windSourceSettings.windFieldFile.GetLength() > 3){

		// Read the log-file now
		OnTimer(NULL, NULL);

		// Set a timer to wake up with the given interval
		int nmSeconds = g_settings.windSourceSettings.windFileReloadInterval * 60 * 1000;
		m_nTimerID = ::SetTimer(NULL, 0, nmSeconds, NULL);
	}else{
		// If we should not re-read the wind-field file then this thread has
		//	nothing to do. We can just as well quit it
		AfxEndThread(0);
	}

	return 1;
}

int CWindFileController::ExitInstance()
{
	if(0 != m_nTimerID)
	{
		::KillTimer(NULL, m_nTimerID);
	}

	return CWinThread::ExitInstance();
}

void CWindFileController::DownloadFileByFTP(){
	CString ipNumber, userName, passWord, fileName, directoryName, localFileName, msg;

	fileName.Format(g_settings.windSourceSettings.windFieldFile);
	int length		= fileName.GetLength();
	fileName = fileName.Right(length - 6); // remove the 'ftp://' - part
	int firstSlash= fileName.FindOneOf("/\\");
	length		= fileName.GetLength();

	// 0. Checks!!
	if(length <= 0)
		return;

	// 1. Parse the IP-number
	ipNumber.Format(fileName.Left(firstSlash));

	// 2. If the IP-number is the same as to the FTP-server then we already know the login and password
	if(!Equals(ipNumber, g_settings.ftpSetting.ftpAddress)){
		return; // unknown login + pwd...
	}else{
		userName.Format(g_settings.ftpSetting.userName);
		passWord.Format(g_settings.ftpSetting.password);
	}

	// 3. Setup the FTP-communciation handler
	Communication::CFTPCom* m_ftp = new Communication::CFTPCom();

	// 4. Connect!
	if(m_ftp->Connect(ipNumber, userName, passWord,TRUE) != 1){
		delete m_ftp;
		return; // fail
	}

	// 5. If necessary then enter a directory
	fileName.Format(g_settings.windSourceSettings.windFieldFile.Right(length - firstSlash - 1));
	length = fileName.GetLength();
	firstSlash = fileName.FindOneOf("/\\");
	while(-1 != firstSlash){
		directoryName.Format(fileName.Left(firstSlash));

		// Enter the directory
		if(0 == m_ftp->SetCurDirectory(directoryName)){
			m_ftp->Disconnect();
			delete m_ftp;
			return;
		}
		Sleep(500);

		// Remove the directory-name from the file-name
		fileName.Format(fileName.Right(length - firstSlash - 1));
		length = fileName.GetLength();
		firstSlash = fileName.FindOneOf("/\\");
	}

	// 6. Download the file!
	localFileName.Format("%sTemp\\WindField.txt", g_settings.outputDirectory);
	if(IsExistingFile(localFileName)){
		if(0 == DeleteFile(localFileName)){
			m_ftp->Disconnect();
			delete m_ftp;
			return;
		}
	}
	if(0 == m_ftp->DownloadAFile(fileName, localFileName)){
		m_ftp->Disconnect();
		delete m_ftp;
		return;
	}

	msg.Format("Downloaded %s", g_settings.windSourceSettings.windFieldFile);
	ShowMessage(msg);

	// 7. Disconnect!
	m_ftp->Disconnect();

	// 8. Parse the file!
	if(0 == g_metData.ReadWindFieldFromFile(localFileName)){
		ShowMessage("Successfully re-read wind-field from file");
		pView->PostMessage(WM_NEW_WINDFIELD, NULL, NULL);
	}

	// Clean up!
	delete m_ftp;
}