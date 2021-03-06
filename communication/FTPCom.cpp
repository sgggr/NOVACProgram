#include "StdAfx.h"
#include "ftpcom.h"
#include "../Common/common.h"
#include "Winsock2.h"
using namespace Communication;
CFTPCom::CFTPCom(void)
{
	m_FtpConnection = NULL;
	m_InternetSession = NULL;
}

CFTPCom::~CFTPCom(void)
{
}
//Connect to FTP server
//return 0 - fail
//return 1 - success
//return 2 - ftp address parsing problem
//return 3 - can not connect to internet
//return 4 - ftp exception
int CFTPCom::Connect(LPCTSTR siteName, LPCTSTR userName, LPCTSTR password, BOOL mode)
{

	INTERNET_PORT  port = 21;
	DWORD dwServiceType = AFX_INET_SERVICE_FTP;
	CString strServer;
	CString strObject;
	CString urlAddress = _T("ftp://");
	urlAddress += siteName;	
	m_FTPSite.Format("%s", siteName);

	// If already connected, then re-connect
	if (m_FtpConnection != NULL)
		m_FtpConnection->Close();
	delete m_FtpConnection;
	m_FtpConnection = NULL;

	if (!AfxParseURL(siteName, dwServiceType, (CString)siteName, strObject, port))
	{
		// try adding the "ftp://" protocol		

		if (!AfxParseURL(urlAddress, dwServiceType, (CString)siteName, strObject, port))
		{
			m_ErrorMsg = TEXT("Can not parse  ftp address");
			ShowMessage(m_ErrorMsg);
			return 2;
		}
	}
	if(m_InternetSession == NULL)
		m_InternetSession = new CInternetSession(NULL, 1, INTERNET_OPEN_TYPE_DIRECT,NULL,
			NULL,0);//INTERNET_FLAG_NO_CACHE_WRITE );

	// Alert the user if the internet session could
	// not be started and close app
	if (!m_InternetSession)
	{
		m_ErrorMsg = TEXT("FAIL TO CONNECT INTERNET");
		ShowMessage(m_ErrorMsg);
		return 3;
	}
	
	//	int nTimeout = AfxGetApp()->GetProfileInt("Settings", "ConnectionTimeout", 30);
	int nTimeout = 30 * 60; // timeout = 30 minutes
	if (dwServiceType == INTERNET_SERVICE_FTP)// && !siteName.IsEmpty())
	{
		try
			{
				m_InternetSession->SetOption(INTERNET_OPTION_CONNECT_TIMEOUT, nTimeout * 1000);
				m_InternetSession->SetOption(INTERNET_OPTION_RECEIVE_TIMEOUT, nTimeout * 1000);
				m_InternetSession->SetOption(INTERNET_OPTION_SEND_TIMEOUT, nTimeout * 1000);
			
				m_FtpConnection = m_InternetSession->GetFtpConnection(siteName,
											userName,password,21,mode);
				m_ErrorMsg.Format("CONNECTED to FTP server: %s", siteName); 
				ShowMessage(m_ErrorMsg);
				return 1;
			}
		catch (CInternetException* pEx)
		{
			// catch errors from WinINet
			TCHAR szErr[255];
			if (pEx->GetErrorMessage(szErr, 255))
			{
				m_ErrorMsg.Format("FTP error from connecting %s: %s", m_FTPSite, szErr);
				ShowMessage(m_ErrorMsg);
			}
			else
			{
				m_ErrorMsg.Format("FTP exception");
				ShowMessage(m_ErrorMsg);
			}
			pEx->Delete();

			m_FtpConnection = NULL;
			return 4;
		}
	}
	return 1;
}
int CFTPCom::Disconnect()
{
	if (m_FtpConnection != NULL)
		m_FtpConnection->Close();
	delete m_FtpConnection;
	m_FtpConnection = NULL;
	if(m_InternetSession !=NULL)
		m_InternetSession->Close();
	delete m_InternetSession;
	m_InternetSession = NULL;
	return 1;
}


int CFTPCom::UpdateFile(LPCTSTR localFile, LPCTSTR remoteFile)
{
	int result = 0;
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to update file using FTP while not connected!");
		return 0;
	}

	// If the file exists, remove it first...
	if(FindFile((CString&)remoteFile) == TRUE)
		m_FtpConnection->Remove(remoteFile);

	// Upload the file
	result = m_FtpConnection->PutFile(localFile, remoteFile);
	return result;
}
BOOL CFTPCom::DownloadAFile(LPCTSTR remoteFile, LPCTSTR fileFullName)
{
	BOOL result = FALSE;
	CString msg;

	// Check that we're connected...
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to upload file using FTP while not connected!");
		return FALSE;
	}

	msg.Format("Trying to download %s", fileFullName);
	ShowMessage(msg);

	try
	{
		// Try to download the file
		result = m_FtpConnection->GetFile(remoteFile, fileFullName, FALSE);

		if(0 == result){ // this means something went wrong
			int ftpError = GetLastError();
			if(ftpError != 0)
			{
				msg.Format("FTP error happened when downloading %s from %s: %d", fileFullName,m_FTPSite,ftpError);
				ShowMessage(msg);
				DWORD code;
				DWORD size_needed = 0;
				InternetGetLastResponseInfo(&code,NULL,&size_needed);
				char *message = (char*)malloc(size_needed + 1);
				InternetGetLastResponseInfo(&code,message,&size_needed);
				msg.Format("Error message :%s",message);
				ShowMessage(msg);
			}
		}else{
			// SUCCESS!!
			msg.Format("Finish downloading %s", fileFullName);
			ShowMessage(msg);
			return result;
		}
	}
	catch (CInternetException* pEx)
	{
		// catch errors from WinINet
		TCHAR szErr[255];
		if (pEx->GetErrorMessage(szErr, 255))
		{
			m_ErrorMsg.Format("FTP error happened when downloading %s from %s: %s", fileFullName,m_FTPSite,szErr);
			ShowMessage(m_ErrorMsg);
		}
		else
		{
			m_ErrorMsg.Format("FTP exception");
			ShowMessage(m_ErrorMsg);
		}
		pEx->Delete();

		m_FtpConnection = NULL;
	}
	return result;
}

int CFTPCom::UploadFile(LPCTSTR localFile, LPCTSTR remoteFile)
{
	int result;

	// Check that we are connected
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to upload file using FTP while not connected!");
		return 0;
	}

	// See if we can find the file on the remote computer, if so
	//	then we can't upload it...
	if(FindFile((CString&)remoteFile) == TRUE)
		return 1;

	try{
		// Try to upload the file
		result = m_FtpConnection->PutFile(localFile,remoteFile);

	}catch(CInternetException *pEx){
		// catch errors from WinINet
		TCHAR szErr[255];
		if (pEx->GetErrorMessage(szErr, 255))
		{
			m_ErrorMsg.Format("FTP error happened when uploading %s to %s: %s", localFile,m_FTPSite,szErr);
			ShowMessage(m_ErrorMsg);
		}
		else
		{	
			m_ErrorMsg.Format("FTP exception");
			ShowMessage(m_ErrorMsg);
		}
		pEx->Delete();
	}
	return result;
}

int CFTPCom::CreateDirectory(LPCTSTR remoteDirectory)
{
	// Check that we are connected
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to create directory using FTP while not connected!");
		return 0;
	}

	int result = m_FtpConnection->CreateDirectory(remoteDirectory);
	return result;	
}

BOOL CFTPCom::SetCurDirectory(LPCTSTR curDirName)
{
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to set directory using FTP while not connected!");
		return 0; // cannot connect...
	}

	BOOL result = m_FtpConnection->SetCurrentDirectory(curDirName);
	return result;
}

int CFTPCom::FindFile(CString& fileName)
{
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to find file using FTP while not connected!");
		return 0; // cannot connect...
	}

	// use a file find object to enumerate files
	CFtpFileFind finder(m_FtpConnection);
	BOOL result = finder.FindFile(_T(fileName));
	// DLN 12/19/17: The above seems to return 1 if the file does not exists, instead of 0 that is expected.
	// A brief search of internet seemed to say that when running this on Unix/Linux system
	// the return codes are backwards.
	//if (0 != result) {
	//	result = finder.FindNextFile();
	//	CString name = finder.GetFileURL();	// Just here for debugging
	//}

	if(0 < result){
		DWORD retcode = GetLastError();
		//ShowMessage("Could not find remote file");
	}
	
	return !result;
}

// @return 0 if fail...
BOOL CFTPCom::DeleteFolder(const CString& folder)
{
	// Check
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to delete folder using FTP while not connected");
		return 0; 
	}

	// Remove the directory
	BOOL result = m_FtpConnection->RemoveDirectory(folder);
	return result;
}

BOOL CFTPCom::EnterFolder(const CString& folder)
{
	CString strDir, strFolder, msg;

	// Check...
	if(m_FtpConnection == NULL){
		ShowMessage("ERROR: Attempted to enter folder using FTP while not connected");
		return FALSE;
	}

	// Set the current directory, return 0 if fail...
	if(0 == m_FtpConnection->SetCurrentDirectory(folder))
		return FALSE;

	// Get the current directory, return 0 if fail...
	if(0 == m_FtpConnection->GetCurrentDirectory(strDir))
		return FALSE;

	// The response we want to have...
	strFolder.Format("/%s/",folder);

	// Compare if the returned string is the same as what we want...
	if(Equals(strDir, strFolder))
	{
		msg.Format("Get into folder %s", folder);
		ShowMessage(msg);
		return TRUE;
	}
	else
	{
		msg.Format("Can not get into folder %s", folder);
		ShowMessage(msg);
		return FALSE;
	}
}

BOOL CFTPCom::GotoTopDirectory()
{
	CString folder("//");	
	return EnterFolder(folder);
}

void CFTPCom::ReadResponse(CInternetFile* file)
{
	char readBuf[256];
	CString str,restStr;
	unsigned int rd = 0;

	// Check input-parameter...
	if(file == NULL){
		return;
	}

	ULONGLONG a = file->GetLength();
	do
	{
		rd = file->Read(readBuf, 256);
		if (rd > 0)
		{
			str.Format("%s",readBuf);
			restStr.Append(str);			
		}
	} while (rd > 0);
}
