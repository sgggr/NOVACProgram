#include "StdAfx.h"
#include "evaluationlogfilehandler.h"
#include "../Common/SpectrometerModel.h"
#include "../Common/Version.h"

// Include synchronization classes
#include <afxmt.h>

// Global variables;
extern CCriticalSection g_evalLogCritSect; // synchronization access to evaluation-log files


using namespace FileHandler;

CEvaluationLogFileHandler::CEvaluationLogFileHandler(void)
{
	// Defining which column contains which information
	m_col.position    = 1;
	m_col.position2   = -1; // azimuth does not exist in the typical eval-log files 
	m_col.starttime   = 2;
	m_col.stoptime    = 3;
	m_col.name        = -1; // the name does not exist in the original version
	m_col.delta       = 4;
	m_col.chiSquare   = -1; // Chi-square does not exist in the original version
	m_col.expTime     = 5;
	m_col.nSpec       = 6;
	m_col.intensity   = -1;   // intensity does not exist in the original version
	m_col.fitIntensity= -1;    // fit intensity only exists in the reevaluation logs
	m_col.peakSaturation = -1; // peak and fit saturation does not exist in the original version
	m_col.fitSaturation = -1;  // peak and fit saturation does not exist in the original version
	m_col.offset      = -1;    // offset does not exist in the original version
	m_col.nSpecies    = 0;
	for(int i = 0; i < MAX_N_REFERENCES; ++i){
		m_col.column[i]      = 7;
		m_col.columnError[i] = 8;
		m_col.shift[i]       = 9;
		m_col.shiftError[i]  = 10;
		m_col.squeeze[i]     = 11;
		m_col.squeezeError[i]= 12;
	}

	m_instrumentType = INSTR_GOTHENBURG;
	m_scanNum = 0;
	m_scan.SetSize(ORIGINAL_ARRAY_LENGTH);
	m_windField.SetSize(ORIGINAL_ARRAY_LENGTH);
}

CEvaluationLogFileHandler::~CEvaluationLogFileHandler(void)
{
	m_scan.RemoveAll();
	m_scan.SetSize(0);
	m_windField.RemoveAll();
	m_windField.SetSize(0);
}


void CEvaluationLogFileHandler::ParseScanHeader(const char szLine[8192]){
	// reset some old information
	ResetColumns();

	char str[8192];
	if(szLine[0] == '#')
		strncpy(str, szLine+1, 8191*sizeof(char));
	else
		strncpy(str, szLine, 8192*sizeof(char));

	char* szToken = (char*)(LPCSTR)str;
	int curCol = -1;
	char elevation[]    = _T("elevation");
	char scanAngle[]    = _T("scanangle");
	char obsAngle[]		= _T("observationangle");
	char azimuth[]		= _T("azimuth");
	char column[]       = _T("column");
	char columnError[]  = _T("columnerror");
	char intensity[]    = _T("intensity");	// peak intensity
	char fitIntensity[] = _T("intens(fitregion)"); // fit-region intensity
	char fitIntensity2[]= _T("fitintensity"); // fit-region intensity
	char peakSat[]		= _T("specsaturation");	// maximum saturation ratio of the whole spectrum
	char fitSat[]		= _T("fitsaturation");	// maximum saturation ratio in the fit region
	char delta[]        = _T("delta");
	char chiSquare[]    = _T("chisquare");
	char shift[]        = _T("shift");
	char shiftError[]   = _T("shifterror");
	char squeeze[]      = _T("squeeze");
	char squeezeError[] = _T("squeezeerror");
	char exposureTime[] = _T("exposuretime");
	char numSpec[]      = _T("numSpec");
	char offset[]       = _T("offset");
	char starttime[]    = _T("starttime");
	char stoptime[]		= _T("stoptime");
	char nameStr[]		= _T("name");

	while(szToken = strtok(szToken, "\t")){
		++curCol;

		// The scan-angle (previously known as elevation)
		if(0 == strnicmp(szToken, elevation, strlen(elevation))){
			m_col.position = curCol;
			szToken = NULL;
			continue;
		}

		// The scan-angle (previously known as elevation)
		if(0 == strnicmp(szToken, scanAngle, strlen(scanAngle))){
			m_col.position = curCol;
			szToken = NULL;
			continue;
		}

		// The observation-angle (the scan-angle for the heidelberg instrument)
		if(0 == strnicmp(szToken, obsAngle, strlen(obsAngle))){
			m_col.position = curCol;
			szToken = NULL;
			continue;
		}

		// The azimuth-angle (defined for the heidelberg instrument)
		if(0 == strnicmp(szToken, azimuth, strlen(azimuth))){
			m_col.position2 = curCol;
			szToken = NULL;
			continue;
		}

		// The exposure time
		if(0 == strnicmp(szToken, exposureTime, strlen(exposureTime))){
			m_col.expTime = curCol;
			szToken = NULL;
			continue;
		}

		// The start time
		if(0 == strnicmp(szToken, starttime, strlen(starttime))){
			m_col.starttime = curCol;
			szToken = NULL;
			continue;
		}

		// The stop time
		if(0 == strnicmp(szToken, stoptime, strlen(stoptime))){
			m_col.stoptime = curCol;
			szToken = NULL;
			continue;
		}

		// The name of the spectrum
		if(0 == strnicmp(szToken, nameStr, strlen(nameStr))){
			m_col.name	= curCol;
			szToken = NULL;
			continue;
		}

		// The number of co-added spectra
		if(0 == strnicmp(szToken, numSpec, strlen(numSpec))){
			m_col.nSpec = curCol;
			szToken = NULL;
			continue;
		}

		// The offset
		if(0 == strnicmp(szToken, offset, strlen(offset))){
			m_col.offset = curCol;
			szToken = NULL;
			continue;
		}

		// The column error (must be looked for before 'column')
		if(0 == strnicmp(szToken, columnError, strlen(columnError))){
			m_col.columnError[m_evResult.m_speciesNum-1] = curCol;
			szToken = NULL;
			continue;
		}

		// The column
		if(0 == strnicmp(szToken, column, strlen(column))){
			CString str;
			m_col.column[m_evResult.m_speciesNum] = curCol;
			char *pt = szToken + strlen(column) + 1;
			szToken[strlen(szToken) - 1] = 0;
			str.Format("%s", pt);
			m_evResult.InsertSpecie(str);
			++m_col.nSpecies;
			szToken = NULL;
			continue;
		}

		// The shift error (must be checked before 'shift')
		if(0 == strnicmp(szToken, shiftError, strlen(shiftError))){
			m_col.shiftError[m_evResult.m_speciesNum-1] = curCol;
			szToken = NULL;
			continue;
		}

		// The shift
		if(0 == strnicmp(szToken, shift, strlen(shift))){
			m_col.shift[m_evResult.m_speciesNum-1] = curCol;
			szToken = NULL;
			continue;
		}

		// The squeeze error (must be checked before 'squeeze')
		if(0 == strnicmp(szToken, squeezeError, strlen(squeezeError))){
			m_col.squeezeError[m_evResult.m_speciesNum-1] = curCol;
			szToken = NULL;
			continue;
		}

		// The squeeze
		if(0 == strnicmp(szToken, squeeze, strlen(squeeze))){
			m_col.squeeze[m_evResult.m_speciesNum-1] = curCol;
			szToken = NULL;
			continue;
		}

		// The spectrum peak-intensity
		if(0 == strnicmp(szToken, intensity, strlen(intensity))){
			m_col.intensity = curCol;
			szToken = NULL;
			continue;
		}

		// The spectrum fit-intensity
		if(0 == strnicmp(szToken, fitIntensity, strlen(fitIntensity)) || 
		   0 == strnicmp(szToken, fitIntensity2, strlen(fitIntensity2))){

			m_col.fitIntensity = curCol;
			szToken = NULL;
			continue;
		}

		// The spectrum maximum saturation ratio of the whole spectrum
		if(0 == strnicmp(szToken, peakSat, strlen(peakSat))){
			m_col.peakSaturation = curCol;
			szToken = NULL;
			continue;
		}

		// The spectrum maximum saturation ratio in the fit region
		if(0 == strnicmp(szToken, fitSat, strlen(fitSat))){
			m_col.fitSaturation = curCol;
			szToken = NULL;
			continue;
		}

		// The delta of the fit
		if(0 == strnicmp(szToken, delta, strlen(delta))){
			m_col.delta = curCol;
			szToken = NULL;
			continue;
		}

		// The chi-square of the fit
		if(0 == strnicmp(szToken, chiSquare, strlen(chiSquare))){
			m_col.chiSquare = curCol;
			szToken = NULL;
			continue;
		}

		szToken = NULL;
	}

	m_specieNum = m_evResult.m_speciesNum;
	for(int k = 0; k < m_specieNum; ++k)
		m_specie[k].Format("%s", m_evResult.m_ref[k].m_specieName);

	return;
}

RETURN_CODE CEvaluationLogFileHandler::ReadEvaluationLog(){
	char  expTimeStr[]        = _T("exposuretime");         // this string only exists in the header line.
	char  scanInformation[]   = _T("<scaninformation>");    // this string only exists in the scan-information section before the scan-data
	char  fluxInformation[]   = _T("<fluxinfo>");           // this string only exists in the flux-information section before the scan-data
	char  spectralData[]      = _T("<spectraldata>");
	char  endofSpectralData[] = _T("</spectraldata>");
	CString str;
	char szLine[8192];
	int measNr = 0;
	double fValue;
	m_scanNum = -1;
	bool fReadingScan = false;
	double flux = 0.0;

	// If no evaluation log selected, quit
	if(strlen(m_evaluationLog) <= 1)
		return FAIL;

	// First count the number of scans in the file.
	//	This to speed up the initialization of the arrays
	CArray<CDateTime*, CDateTime*> allStartTimes;
	CArray<unsigned int, unsigned int&> sortOrder;
	long nScans = GetScanStartTimes(allStartTimes);
	if(nScans > 0){
		m_scan.SetSize(nScans);
		m_windField.SetSize(nScans + 1);
	}else{
		MessageBox(NULL, "No scans found in file", "No scans", MB_OK);
		return FAIL;
	}
	SortScanStartTimes(allStartTimes, sortOrder);
	
	// Open the evaluation log
	CSingleLock singleLock(&g_evalLogCritSect);
	singleLock.Lock();
	if(singleLock.IsLocked()){
		FILE *f = fopen(m_evaluationLog, "r");
		if(NULL == f){
			singleLock.Unlock();
			return FAIL;
		}

		// Reset the column- and spectrum info
		ResetColumns();
		ResetScanInformation();

		// Read the file, one line at a time
		while(fgets(szLine, 8192, f)){

			// ignore empty lines
			if(strlen(szLine) < 2){
				if(fReadingScan){
					fReadingScan = false;
					// Reset the column- and spectrum-information
					ResetColumns();
					ResetScanInformation();
				}
				continue;
			}

			// convert the string to all lower-case letters
			for(unsigned int it = 0; it < strlen(szLine); ++it){
				szLine[it] = tolower(szLine[it]);
			}

			// find the next scan-information section
			if(NULL != strstr(szLine, scanInformation)){
				ResetScanInformation();
				ParseScanInformation(m_specInfo, flux, f);
				continue;
			}

			// find the next flux-information section
			if(NULL != strstr(szLine, fluxInformation)){
				ParseFluxInformation(m_windField[sortOrder[m_scanNum+1]], flux, f);
				continue;
			}

			if(NULL != strstr(szLine, spectralData)){
				fReadingScan = true;
				continue;
			}else if(NULL != strstr(szLine, endofSpectralData)){
				fReadingScan = false;
				continue;
			}

			// find the next start of a scan 
			if(NULL != strstr(szLine, expTimeStr)){

				// check so that there was some information in the last scan read
				//	if not the re-use the memory space
				if((measNr > 0) || (measNr == 0 && m_scanNum < 0)){

					// The current measurement position inside the scan
					measNr = 0;

					// before we start the next scan, calculate some information about
					// the old one

					// 1. If the sky and dark were specified, remove them from the measurement
					if(m_scanNum >= 0 && fabs(m_scan[sortOrder[m_scanNum]].GetScanAngle(1) - 180.0) < 1){
						m_scan[sortOrder[m_scanNum]].RemoveResult(0); // remove sky
						m_scan[sortOrder[m_scanNum]].RemoveResult(0); // remove dark
					}

					// 2. Calculate the offset
					if(m_scanNum >= 0){
						m_scan[sortOrder[m_scanNum]].CalculateOffset(m_evResult.m_ref[0].m_specieName);
					}

					// start the next scan.
					++m_scanNum;
					if(m_scanNum >= m_scan.GetSize()){
						m_scan.SetSize(m_scanNum + 1);
						m_windField.SetSize(m_scanNum + 2);
					}
				}

			// This line is the header line which says what each column represents.
			//  Read it and parse it to find out how to interpret the rest of the 
			//  file. 
			ParseScanHeader(szLine);

			// start parsing the lines
			fReadingScan = true;

			// read the next line, which is the first line in the scan
			continue; 
			}

			// ignore comment lines
			if(szLine[0] == '#')
			continue;

			// if we're not reading a scan, let's read the next line
			if(!fReadingScan)
			continue;

			// Split the scan information up into tokens and parse them. 
			char* szToken = (char*)(LPCSTR)szLine;
			int curCol = -1;
			while(szToken = strtok(szToken, " \t")){
			++curCol;

			// First check the starttime
			if(curCol == m_col.starttime){
				int fValue1, fValue2, fValue3;
				if(strstr(szToken, ":"))
					sscanf(szToken, "%d:%d:%d", &fValue1, &fValue2, &fValue3);
				else
					sscanf(szToken, "%d.%d.%d", &fValue1, &fValue2, &fValue3);
				m_specInfo.m_startTime.hr   = fValue1;
				m_specInfo.m_startTime.m    = fValue2;
				m_specInfo.m_startTime.sec  = fValue3;
				szToken = NULL;
				continue;
			}

			// Then check the stoptime
				if(curCol == m_col.stoptime){
				int fValue1, fValue2, fValue3;
				if(strstr(szToken, ":"))
					sscanf(szToken, "%d:%d:%d", &fValue1, &fValue2, &fValue3);
				else
					sscanf(szToken, "%d.%d.%d", &fValue1, &fValue2, &fValue3);
				m_specInfo.m_stopTime.hr   = fValue1;
				m_specInfo.m_stopTime.m    = fValue2;
				m_specInfo.m_stopTime.sec  = fValue3;
				szToken = NULL;
				continue;
			}

			// Also check the name...
			if(curCol == m_col.name){
				m_specInfo.m_name.Format(szToken);
				szToken = NULL;
				continue;
			}

			// ignore columns whose value cannot be parsed into a float
			if(1 != sscanf(szToken, "%lf", &fValue)){
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.position){
				m_specInfo.m_scanAngle = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.position2){
				m_specInfo.m_scanAngle2 = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.intensity){
				m_specInfo.m_peakIntensity = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.fitIntensity){
				m_specInfo.m_fitIntensity = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.fitSaturation){
				m_specInfo.m_fitIntensity = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.peakSaturation){
				m_specInfo.m_peakIntensity = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.offset){
				m_specInfo.m_offset = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.delta){
				m_evResult.m_delta = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.chiSquare){
				m_evResult.m_chiSquare = (float)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.nSpec){
				m_specInfo.m_numSpec = (long)fValue;
				szToken = NULL;
				continue;
			}

			if(curCol == m_col.expTime){
				m_specInfo.m_exposureTime = (long)fValue;
				szToken = NULL;
				continue;
			}

			for(int k = 0; k < m_col.nSpecies; ++k){
				if(curCol == m_col.column[k]){
					m_evResult.m_ref[k].m_column = (float)fValue;
					break;
				}
				if(curCol == m_col.columnError[k]){
					m_evResult.m_ref[k].m_columnError = (float)fValue;
					break;
				}
				if(curCol == m_col.shift[k]){
					m_evResult.m_ref[k].m_shift = (float)fValue;
					break;
				}
				if(curCol == m_col.shiftError[k]){
					m_evResult.m_ref[k].m_shiftError = (float)fValue;
					break;
				}
				if(curCol == m_col.squeeze[k]){
					m_evResult.m_ref[k].m_squeeze = (float)fValue;
					break;
				}
				if(curCol == m_col.squeezeError[k]){
					m_evResult.m_ref[k].m_squeezeError = (float)fValue;
					break;
				}
			}
			szToken = NULL;
			}

			// start reading the next line in the evaluation log (i.e. the next
			//  spectrum in the scan). Insert the data from this spectrum into the 
			//  CScanResult structure

			// If this is the first spectrum in the new scan, then make
			//	an initial guess for how large the arrays are going to be...
			if(measNr == 0 && m_scanNum > 1){
				// If this is the first spectrum in a new scan, then initialize the 
				//	size of the arrays, to save some time on re-allocating memory
				m_scan[sortOrder[m_scanNum]].InitializeArrays(m_scan[m_scanNum-1].GetEvaluatedNum());
			}

			m_specInfo.m_scanIndex = measNr;
			if(Equals(m_specInfo.m_name, "sky")){
				m_scan[sortOrder[m_scanNum]].SetSkySpecInfo(m_specInfo);
			}else if(Equals(m_specInfo.m_name, "dark")){
				m_scan[sortOrder[m_scanNum]].SetDarkSpecInfo(m_specInfo);
			}else if(Equals(m_specInfo.m_name, "offset")){
				m_scan[sortOrder[m_scanNum]].SetOffsetSpecInfo(m_specInfo);
			}else if(Equals(m_specInfo.m_name, "dark_cur")){
				m_scan[sortOrder[m_scanNum]].SetDarkCurrentSpecInfo(m_specInfo);
			}else{
				m_scan[sortOrder[m_scanNum]].AppendResult(m_evResult, m_specInfo);
				m_scan[sortOrder[m_scanNum]].SetFlux(flux);
				m_scan[sortOrder[m_scanNum]].SetInstrumentType(m_instrumentType);
			}

			double dynamicRange = 1.0; // <-- unknown
			if(m_col.peakSaturation != -1){ // If the intensity is specified as a saturation ratio...
				dynamicRange = CSpectrometerModel::GetMaxIntensity(m_specInfo.m_specModel);
			}
			m_scan[sortOrder[m_scanNum]].CheckGoodnessOfFit(m_specInfo);
			++measNr;
		}

		// close the evaluation log
		fclose(f);
	}
	singleLock.Unlock();

	// If the sky and dark were specified, remove them from the measurement
	if(m_scanNum >= 0 && fabs(m_scan[sortOrder[m_scanNum]].GetScanAngle(1) - 180.0) < 1){
		m_scan[sortOrder[m_scanNum]].RemoveResult(0); // remove sky
		m_scan[sortOrder[m_scanNum]].RemoveResult(0); // remove dark
	}

	// Calculate the offset
	if(m_scanNum >= 0){
		m_scan[sortOrder[m_scanNum]].CalculateOffset(m_evResult.m_ref[0].m_specieName);
	}

	// make sure that scan num is correct
	++m_scanNum;

	// Sort the scans in order of collection
//	SortScans();

	return SUCCESS;
}

/** Makes a quick scan through the evaluation-log 
	to count the number of scans in it */
long CEvaluationLogFileHandler::CountScansInFile(){
	char  expTimeStr[] = _T("exposuretime"); // this string only exists in the header line.
	char szLine[8192];
	long  nScans = 0;

	// If no evaluation log selected, quit
	if(strlen(m_evaluationLog) <= 1)
		return 0;

	CSingleLock singleLock(&g_evalLogCritSect);
	singleLock.Lock();
	if(singleLock.IsLocked()){

		// Open the evaluation log
		FILE *f = fopen(m_evaluationLog, "r");
		if(NULL == f){
			singleLock.Unlock();
			return 0;
		}

		// Read the file, one line at a time
		while(fgets(szLine, 8192, f)){
			// convert the string to all lower-case letters
			for(unsigned int it = 0; it < strlen(szLine); ++it){
				szLine[it] = tolower(szLine[it]);
			}

			// find the next start of a scan 
			if(NULL != strstr(szLine, expTimeStr)){
				++nScans;
			}
		}

		fclose(f);
		
	}
	singleLock.Unlock();

	// Return the number of scans found in the file
	return nScans;
}

/** Makes a quick scan through the evaluation-log 
	to get the start-times of each scan.
	@return the number of scans in the file */
long CEvaluationLogFileHandler::GetScanStartTimes(CArray<CDateTime*, CDateTime*> &array){
	char  scanInfoStart[] = _T("<scaninformation>"); // this string indicates the beginning of a 'scanInformation' section
	char  scanInfoStop[] = _T("</scaninformation>"); // this string indicates the beginning of a 'scanInformation' section
	char szLine[8192];
	long  nScans = 0;
	char *pt = NULL;
	bool inScanInfoSection = false;
	CDateTime scanStartTime;
	int tmpInt[3];
	bool foundDate = false, foundTime = false;

	// If no evaluation log selected, quit
	if(strlen(m_evaluationLog) <= 1)
		return 0;

	CSingleLock singleLock(&g_evalLogCritSect);
	singleLock.Lock();
	if(singleLock.IsLocked()){

		// Open the evaluation log
		FILE *f = fopen(m_evaluationLog, "r");
		if(NULL == f){
			singleLock.Unlock();
			return 0;
		}

		// Read the file, one line at a time
		while(fgets(szLine, 8192, f)){
			// convert the string to all lower-case letters
			for(unsigned int it = 0; it < strlen(szLine); ++it){
				szLine[it] = tolower(szLine[it]);
			}

			// find the next start of a scan 
			if(NULL != strstr(szLine, scanInfoStart)){
				inScanInfoSection = true;
			}

			if(NULL != strstr(szLine, scanInfoStop)){
				if(foundDate && foundTime){
					array.SetAtGrow(nScans, new CDateTime(scanStartTime));
				}
				inScanInfoSection = false;
				foundDate = false;
				foundTime = false;
				++nScans;
			}
			
			if(inScanInfoSection){
				if(pt = strstr(szLine, "date=")){
					if(3 == sscanf(pt+5, "%d.%d.%d", &tmpInt[0], &tmpInt[1], &tmpInt[2])){
						scanStartTime.day   = tmpInt[0];
						scanStartTime.month = tmpInt[1];
						scanStartTime.year  = tmpInt[2];
						foundDate = true;
						continue;
					}
				}
				if(pt = strstr(szLine, "starttime=")){
					if(3 == sscanf(pt+10, "%d:%d:%d", &tmpInt[0], &tmpInt[1], &tmpInt[2])){
						scanStartTime.hour   = tmpInt[0];
						scanStartTime.minute = tmpInt[1];
						scanStartTime.second = tmpInt[2];
						foundTime = true;
					}
					continue;
				}
			}
		}
		fclose(f);
	}
	singleLock.Unlock();

	// Return the number of scans found in the file
	return nScans;
}

/** Reads and parses the 'scanInfo' header before the scan */
void CEvaluationLogFileHandler::ParseScanInformation(CSpectrumInfo &scanInfo, double &flux, FILE *f){
	char szLine[8192];
	char *pt = NULL;
	int tmpInt[3];
	double tmpDouble;
	char instrumentType[64];

	// Reset the column- and spectrum info
	ResetColumns();

	// read the additional scan-information, line by line
	while(fgets(szLine, 8192, f)){

		// convert to lower-case
		for(unsigned int it = 0; it < strlen(szLine); ++it){
			szLine[it] = tolower(szLine[it]);
		}

		if(pt = strstr(szLine, "</scaninformation>")){
			break;
		}
		if(pt = strstr(szLine, "compiledate=")){
			continue;
		}
		if(pt = strstr(szLine, "site=")){
			scanInfo.m_site.Format("%s", pt+5);
			scanInfo.m_site.Remove('\n'); // Remove newline characters
			continue;
		}
		if(pt = strstr(szLine, "date=")){
			sscanf(pt+5, "%d.%d.%d", &tmpInt[0], &tmpInt[1], &tmpInt[2]);
			scanInfo.m_date[0] = tmpInt[2];
			scanInfo.m_date[1] = tmpInt[1];
			scanInfo.m_date[2] = tmpInt[0];
			continue;
		}
		if(pt = strstr(szLine, "starttime=")){
			if(3 == sscanf(pt+10, "%d:%d:%d", &tmpInt[0], &tmpInt[1], &tmpInt[2])){
				scanInfo.m_startTime.hr		= tmpInt[0];
				scanInfo.m_startTime.m		= tmpInt[1];
				scanInfo.m_startTime.sec	= tmpInt[2];
			}
			continue;
		}
		if(pt = strstr(szLine, "stoptime=")){
			if(3 == sscanf(pt+9, "%d.%d.%d", &tmpInt[0], &tmpInt[1], &tmpInt[2])){
				scanInfo.m_stopTime.hr		= tmpInt[0];
				scanInfo.m_stopTime.m		= tmpInt[1];
				scanInfo.m_stopTime.sec	= tmpInt[2];
			}
			continue;
		}
		if(pt = strstr(szLine, "compass=")){
			sscanf(pt+8, "%lf", &tmpDouble);
			scanInfo.m_compass = (float)fmod(tmpDouble, 360.0);
			continue;
		}
		if(pt = strstr(szLine, "tilt=")){
			sscanf(pt + 5, "%lf", &tmpDouble);
			scanInfo.m_pitch = (float)tmpDouble;
		}

		if(pt = strstr(szLine, "lat=")){
			sscanf(pt+4, "%lf", &tmpDouble);
			scanInfo.m_gps.m_latitude = tmpDouble;
			continue;
		}
		if(pt = strstr(szLine, "long=")){
			sscanf(pt+5, "%lf", &tmpDouble);
			scanInfo.m_gps.m_longitude = tmpDouble;
			continue;
		}
		if(pt = strstr(szLine, "alt=")){
			sscanf(pt+4, "%lf", &tmpDouble);
			scanInfo.m_gps.m_altitude = (long)tmpDouble;
			continue;
		}
		if(pt = strstr(szLine, "serial=")){
			scanInfo.m_device.Format("%s", pt + 7);
			scanInfo.m_device.Remove('\n'); // remove remaining strange things in the serial-number
			scanInfo.m_device.MakeUpper();	// Convert the serial-number to all upper case letters

			// Extract the spectrometer-model from the serial-number of the spectrometer
			if(strstr(scanInfo.m_device, "D2J")){
				scanInfo.m_specModel = S2000;
			}else if(strstr(scanInfo.m_device, "I2J")){
				scanInfo.m_specModel = S2000;
			}else if(strstr(scanInfo.m_device, "USB2")){
				scanInfo.m_specModel = USB2000;
			}else if(strstr(scanInfo.m_device, "USB4C")){
				scanInfo.m_specModel = USB4000;
			}else if(strstr(scanInfo.m_device, "HR2")){
				scanInfo.m_specModel = HR2000;
			}else if(strstr(scanInfo.m_device, "HR4")){
				scanInfo.m_specModel = HR4000;
			}else if(strstr(scanInfo.m_device, "QE")){
				scanInfo.m_specModel = QE65000;
			}else if (strstr(scanInfo.m_device, "MAYAPRO")) {
				scanInfo.m_specModel = MAYAPRO;
			}

			continue;
		}
		if(pt = strstr(szLine, "spectrometer=")){
			// TODO:: read in the spectrometer model somewhere
			continue;
		}

		if(pt = strstr(szLine, "volcano=")){
			scanInfo.m_volcano.Format("%s", pt+8);
			scanInfo.m_volcano.Remove('\n'); // Remove newline characters
			continue;
		}

		if(pt = strstr(szLine, "observatory=")){
			scanInfo.m_observatory.Format("%s", pt+12);
			scanInfo.m_observatory.Remove('\n'); // Remove newline characters
			continue;
		}

		if(pt = strstr(szLine, "channel=")){
			sscanf(pt + 8, "%lf", &tmpDouble);
			scanInfo.m_channel = (unsigned char)tmpDouble;
		}
		if(pt = strstr(szLine, "coneangle=")){
			sscanf(pt + 10, "%lf", &tmpDouble);
			scanInfo.m_coneAngle = (float)tmpDouble;
		}

		if(pt = strstr(szLine, "flux=")){
			sscanf(pt + 5, "%lf", &tmpDouble);
			flux = tmpDouble;
		}

		if(pt = strstr(szLine, "battery=")){
			sscanf(pt + 8, "%f", &scanInfo.m_batteryVoltage);
		}

		if(pt = strstr(szLine, "temperature")){
			sscanf(pt + 12, "%f", &scanInfo.m_temperature);
		}

		if(pt = strstr(szLine, "instrumenttype=")){
			sscanf(pt + 15, "%s", instrumentType);
			if(Equals(instrumentType, "heidelberg"))
				m_instrumentType = INSTR_HEIDELBERG;
			else
				m_instrumentType = INSTR_GOTHENBURG;
		}
	}
}

void CEvaluationLogFileHandler::ParseFluxInformation(CWindField &windField, double &flux, FILE *f){
	char szLine[8192];
	char *pt = NULL;
	double windSpeed = 10, windDirection = 0, plumeHeight = 1000;
	MET_SOURCE windSpeedSource			= MET_USER;
	MET_SOURCE windDirectionSource		= MET_USER;
	MET_SOURCE plumeHeightSource		= MET_USER;
	char source[512];

	// read the additional scan-information, line by line
	while(fgets(szLine, 8192, f)){
		if(pt = strstr(szLine, "</fluxinfo>")){
			// save all the values
			windField.SetPlumeHeight(plumeHeight, plumeHeightSource);
			windField.SetWindDirection(windDirection, windDirectionSource);
			windField.SetWindSpeed(windSpeed, windSpeedSource);
			break;
		}

		if(pt = strstr(szLine, "flux=")){
			sscanf(pt+5, "%lf", &flux);
			continue;
		}

		if(pt = strstr(szLine, "windspeed=")){
			sscanf(pt+10, "%lf", &windSpeed);
			continue;
		}

		if(pt = strstr(szLine, "winddirection=")){
			sscanf(pt+14, "%lf", &windDirection);
			continue;
		}

		if(pt = strstr(szLine, "plumeheight=")){
			sscanf(pt+12, "%lf", &plumeHeight);
			continue;
		}

		if(pt = strstr(szLine, "windspeedsource=")){
			sscanf(pt+16, "%s", source);
			if(strstr(source, "user")){
				windSpeedSource = MET_USER;
			}else if(strstr(source, "default")){
				windSpeedSource = MET_DEFAULT;
			}else if(strstr(source, "ecmwf_forecast")){
				windSpeedSource = MET_ECMWF_FORECAST;
			}else if(strstr(source, "ecmwf_analysis")){
				windSpeedSource = MET_ECMWF_ANALYSIS;
			}else if(strstr(source, "dual_beam_measurement")){
				windSpeedSource = MET_DUAL_BEAM_MEASUREMENT;
			}
			continue;
		}

		if(pt = strstr(szLine, "winddirectionsource=")){
			sscanf(pt+20, "%s", source);
			if(strstr(source, "user")){
				windDirectionSource = MET_USER;
			}else if(strstr(source, "default")){
				windDirectionSource = MET_DEFAULT;
			}else if(strstr(source, "ecmwf_forecast")){
				windDirectionSource = MET_ECMWF_FORECAST;
			}else if(strstr(source, "ecmwf_analysis")){
				windDirectionSource = MET_ECMWF_ANALYSIS;
			}else if(strstr(source, "triangulation")){
				windDirectionSource = MET_GEOMETRY_CALCULATION;
			}
			continue;
		}

		if(pt = strstr(szLine, "plumeheightsource=")){
			sscanf(pt+18, "%s", source);
			if(strstr(source, "user")){
				plumeHeightSource = MET_USER;
			}else if(strstr(source, "default")){
				plumeHeightSource = MET_DEFAULT;
			}else if(strstr(source, "ecmwf_forecast")){
				plumeHeightSource = MET_ECMWF_FORECAST;
			}else if(strstr(source, "ecmwf_analysis")){
				plumeHeightSource = MET_ECMWF_ANALYSIS;
			}else if(strstr(source, "triangulation")){
				plumeHeightSource = MET_GEOMETRY_CALCULATION;
			}
			continue;
		}
	}
}

void CEvaluationLogFileHandler::ResetColumns(){
	for(int k = 0; k < MAX_N_REFERENCES; ++k){
		m_col.column[k] = -1;
		m_col.columnError[k] = -1;
		m_col.shift[k] = -1;
		m_col.shiftError[k] = -1;
		m_col.squeeze[k] = -1;
		m_col.squeezeError[k] = -1;
	}
	m_col.delta = m_col.intensity = m_col.position = m_col.position2 = -1;
	m_col.nSpecies =  m_evResult.m_speciesNum = 0;
	m_col.expTime = m_col.nSpec = -1;
	m_col.name	= -1;
	m_specieNum = m_curSpecie = 0;
}

void CEvaluationLogFileHandler::ResetScanInformation(){
	m_specInfo.m_channel = 0;
	m_specInfo.m_compass = m_specInfo.m_scanAngle = 0.0;

	m_col.starttime  = -1; m_col.stoptime = -1;
}

/** Returns true if the most recently read evaluation log file is a 
			wind speed measurement. */
bool	CEvaluationLogFileHandler::IsWindSpeedMeasurement(int scanNo){
	// check so that there are some scans read, and that the scan index is ok
	if(m_scanNum < 1 || scanNo > m_scanNum || scanNo < 0)
		return false;

	return m_scan[scanNo].IsWindMeasurement();
}

/** Returns true if the scan number 'scanNo' in the most recently read 
		evaluation log file is a wind speed measurement of heidelberg type. */
bool	CEvaluationLogFileHandler::IsWindSpeedMeasurement_Heidelberg(int scanNo){
	// check so that there are some scans read, and that the scan index is ok
	if(m_scanNum < 1 || scanNo > m_scanNum || scanNo < 0)
		return false;

	return m_scan[scanNo].IsWindMeasurement_Heidelberg();
}

/** Sorts the scans in order of collection */
void CEvaluationLogFileHandler::SortScans(){

	// make sure that the array is just big enough...
	m_scan.SetSize(m_scanNum);

	// If the scans are already in order then we don't need to sort them
	//	or if there is only one scan then we don't need to fix it.
	if(IsSorted() || m_scanNum <= 1){
		return;
	}

	// Then sort the array
	//	CEvaluationLogFileHandler::SortScans(m_scan);
	CEvaluationLogFileHandler::BubbleSortScans(m_scan);

	return;
}

/** Returns true if the scans are already ordered */
bool	CEvaluationLogFileHandler::IsSorted(){
	CDateTime time1, time2;

	for(int k = 0; k < m_scanNum - 1; ++k){
		// Get the start-times
		m_scan[k].GetStartTime(0,		time1);
		m_scan[k+1].GetStartTime(0, time2);

		// If the second scan has started before the first, 
		//	then change the order, otherwise don't do anything
		if(time2 < time1){
			return false;
		}else{
			continue;
		}
	}

	return true; // no un-ordered scans were found
}

/** Sorts the CDateTime-objects in the given array.
		Algorithm based on bubble sort (~O(N2)) */
void CEvaluationLogFileHandler::SortScanStartTimes(CArray<CDateTime*, CDateTime*> &allStartTimes, CArray<unsigned int, unsigned int&> &sortOrder){
	bool change;
	unsigned int k, tmpIndex;
	unsigned int nElements = allStartTimes.GetSize();
	CDateTime *tmpTime = NULL;
	CArray<unsigned int, unsigned int&> tmpOrder;
	
	if(nElements == 0)
		return;

	// fill up the sort-order array with the original indices
	for(k = 0; k < nElements; ++k)
		tmpOrder.SetAtGrow(k, k);

	// Sort the start-times using bubble sort
	do{
		change = false;
		for(k = 0; k < nElements - 1; ++k){
			// If the second scan has started before the first, 
			//	then change the order, otherwise don't do anything
			if(*allStartTimes[k+1] < *allStartTimes[k]){
				tmpTime = allStartTimes[k];
				tmpIndex = tmpOrder[k];

				allStartTimes[k] = allStartTimes[k+1];
				tmpOrder[k] = tmpOrder[k+1];

				allStartTimes[k+1] = tmpTime;
				tmpOrder[k+1] = tmpIndex;

				change		= true;
			}else{
				continue;
			}
		}
	}while(change);
	
	// create the array sortOrder
	for(k = 0; k < nElements; ++k)
		sortOrder.SetAtGrow(tmpOrder[k], k);
	
	return;
}


/** Writes the contents of the array 'm_scan' to a new evaluation-log file */
RETURN_CODE CEvaluationLogFileHandler::WriteEvaluationLog(const CString fileName){
	CString string, specieName;
	CString wsSrc, wdSrc, phSrc;
	CDateTime startTime;

	// 1. Test if the file already exists, if so then return false
	if(IsExistingFile(fileName))
		return FAIL;

	// 2. Write the file
	FILE *f = fopen(fileName, "w");

	for(int scanIndex = 0; scanIndex < this->m_scanNum; ++scanIndex){
		Evaluation::CScanResult &scan = this->m_scan[scanIndex];
		CWindField							&wind	= this->m_windField[scanIndex];

		scan.GetStartTime(0, startTime);

		// ----------------- Create the additional scan-information ----------------------
		string.Format("\n<scaninformation>\n");
		string.AppendFormat("\tdate=%02d.%02d.%04d\n",			startTime.day, startTime.month, startTime.year);
		string.AppendFormat("\tstarttime=%02d:%02d:%02d\n", startTime.hour, startTime.minute, startTime.second);
		string.AppendFormat("\tcompass=%.1lf\n",						scan.GetCompass());
		string.AppendFormat("\ttilt=%.1lf\n",								scan.GetPitch());
		string.AppendFormat("\tlat=%.6lf\n",								scan.GetLatitude());
		string.AppendFormat("\tlong=%.6lf\n",								scan.GetLongitude());
		string.AppendFormat("\talt=%ld\n",									scan.GetAltitude());

		string.AppendFormat("\tvolcano=%s\n",								m_specInfo.m_volcano);
		string.AppendFormat("\tsite=%s\n",									m_specInfo.m_site);
		string.AppendFormat("\tobservatory=%s\n",						m_specInfo.m_observatory);

		string.AppendFormat("\tserial=%s\n",								scan.GetSerial());
		switch(m_specInfo.m_specModel){
			case S2000:				string.AppendFormat("\tspectrometer=s2000\n");	break;
			case USB2000:			string.AppendFormat("\tspectrometer=usb2000\n");	break;
			case USB4000:			string.AppendFormat("\tspectrometer=usb4000\n");	break;
			case HR2000:			string.AppendFormat("\tspectrometer=hr2000\n");	break;
			case HR4000:			string.AppendFormat("\tspectrometer=hr4000\n");	break;
			case QE65000:			string.AppendFormat("\tspectrometer=qe65000\n");	break;
			case MAYAPRO:			string.AppendFormat("\tspectrometer=mayapro\n");	break;
			default:					string.AppendFormat("\tspectrometer=s2000\n");	break;
		}
		string.AppendFormat("\tchannel=%d\n",							m_specInfo.m_channel);
		string.AppendFormat("\tconeangle=%.1lf\n",				scan.GetConeAngle());
		string.AppendFormat("\tinterlacesteps=%d\n",			m_specInfo.m_interlaceStep);
		string.AppendFormat("\tstartchannel=%d\n",				m_specInfo.m_startChannel);
		string.AppendFormat("\tspectrumlength=%d\n",			2048);
		string.AppendFormat("\tflux=%.2lf\n",							scan.GetFlux());
		string.AppendFormat("\tbattery=%.2f\n",						scan.GetBatteryVoltage());
		string.AppendFormat("\ttemperature=%.2f\n",				scan.GetTemperature());
		// The mode
		if(scan.IsDirectSunMeasurement())
			string.AppendFormat("\tmode=direct_sun\n");
		else if(scan.IsLunarMeasurement())
			string.AppendFormat("\tmode=lunar\n");
		else if(scan.IsWindMeasurement())
			string.AppendFormat("\tmode=wind\n");
		else if(scan.IsStratosphereMeasurement())
			string.AppendFormat("\tmode=stratospheric\n");
		else if(scan.IsCompositionMeasurement())
			string.AppendFormat("\tmode=composition\n");
		else
			string.AppendFormat("\tmode=plume\n");

		// The type of instrument used...
		if(scan.GetInstrumentType() == INSTR_GOTHENBURG){
			string.AppendFormat("\tinstrumenttype=gothenburg\n");
		}else if(scan.GetInstrumentType() == INSTR_HEIDELBERG){
			string.AppendFormat("\tinstrumenttype=heidelberg\n");
		}
		double maxIntensity = CSpectrometerModel::GetMaxIntensity(m_specInfo.m_specModel);

		// Finally, the version of the file and the version of the program
		string.AppendFormat("\tversion=2.0\n");
		string.AppendFormat("\tsoftwareversion=%d.%02d\n", CVersion::majorNumber, CVersion::minorNumber);
		string.AppendFormat("\tcompiledate=%s\n",					  __DATE__);

		string.AppendFormat("</scaninformation>\n\n");
		fprintf(f, string);

		// ----------------- Create the flux-information ----------------------
		wind.GetWindSpeedSource(wsSrc);
		wind.GetWindDirectionSource(wdSrc);
		wind.GetPlumeHeightSource(phSrc);
		string.Format("<fluxinfo>\n");
		string.AppendFormat("\tflux=%.4lf\n",							scan.GetFlux());
		string.AppendFormat("\twindspeed=%.4lf\n",				wind.GetWindSpeed());
		string.AppendFormat("\twinddirection=%.4lf\n",		wind.GetWindDirection());
		string.AppendFormat("\tplumeheight=%.2lf\n",			wind.GetPlumeHeight());
		string.AppendFormat("\twindspeedsource=%s\n",			wsSrc);
		string.AppendFormat("\twinddirectionsource=%s\n",	wdSrc);
		string.AppendFormat("\tplumeheightsource=%s\n",		phSrc);
		//if(fabs(spectrometer.m_scanner.compass) > 360.0)
		//	string.AppendFormat("\tcompasssource=compassreading\n");
		//else
		//	string.AppendFormat("\tcompasssource=user\n");
		string.AppendFormat("</fluxinfo>\n");
		fprintf(f, string);

		// ----------------------- write the header --------------------------------
		if(m_instrumentType == INSTR_GOTHENBURG){
			string.Format("#scanangle\t");
		}else if(m_instrumentType == INSTR_HEIDELBERG){
			string.Format("#observationangle\tazimuth\t");
		}
		string.AppendFormat("starttime\tstoptime\tname\tspecsaturation\tfitsaturation\tdelta\tchisquare\texposuretime\tnumspec\t");

		for(int itSpecie = 0; itSpecie < scan.GetSpecieNum(0); ++itSpecie){
			specieName.Format("%s", scan.GetSpecieName(0, itSpecie));
			string.AppendFormat("column(%s)\tcolumnerror(%s)\t",		specieName, specieName);
			string.AppendFormat("shift(%s)\tshifterror(%s)\t",			specieName, specieName);
			string.AppendFormat("squeeze(%s)\tsqueezeerror(%s)\t",	specieName, specieName);
		}
		string.AppendFormat("isgoodpoint\toffset\tflag");
		string.AppendFormat("\n<spectraldata>\n");

		fprintf(f, string);


		// ------------------- Then write the parameters for each spectrum ---------------------------
		for(int itSpectrum = 0; itSpectrum < scan.GetEvaluatedNum(); ++itSpectrum){
			// 3a. Pretty print the result and the spectral info into a string
			FormatEvaluationResult(&scan.GetSpectrumInfo(itSpectrum), scan.GetResult(itSpectrum), m_instrumentType, 0.0, scan.GetSpecieNum(itSpectrum), string);

			// 3b. Write it to the evaluation log file
			fprintf(f, string);
			fprintf(f, "\n");
		}
		fprintf(f, "</spectraldata>\n");

	}



	// Remember to close the file
	fclose(f);

	return SUCCESS;
}

RETURN_CODE CEvaluationLogFileHandler::FormatEvaluationResult(const CSpectrumInfo *info, const Evaluation::CEvaluationResult *result, INSTRUMENT_TYPE iType, double maxIntensity, int nSpecies, CString &string){
	int itSpecie;
	Common common;

	// 1. The Scan angle
	string.Format("%.0lf\t", info->m_scanAngle);

	// 2. The azimuth angle
	if(iType == INSTR_HEIDELBERG)
		string.AppendFormat("%.0lf\t", info->m_scanAngle2);

	// 3. The start time
	string.AppendFormat("%02d:%02d:%02d\t", info->m_startTime.hr, info->m_startTime.m, info->m_startTime.sec);

	// 4. The stop time
	string.AppendFormat("%02d:%02d:%02d\t", info->m_stopTime.hr, info->m_stopTime.m, info->m_stopTime.sec);

	// 5 The name of the spectrum
	string.AppendFormat("%s\t", common.SimplifyString(info->m_name));

	// 6. The (maximum) saturation ratio of the whole spectrum,
	//			the (maximum) saturation ratio in the fit-region
	//			and the normalized maximum intensity of the whole spectrum
	if(maxIntensity > 0.0){
		string.AppendFormat("%.2lf\t", info->m_peakIntensity / maxIntensity);
		string.AppendFormat("%.2lf\t", info->m_fitIntensity  / maxIntensity);
	}else{
	  string.AppendFormat("%.2lf\t", info->m_peakIntensity);
		string.AppendFormat("%.2lf\t", info->m_fitIntensity);
	}
	string.AppendFormat("%.2lf\t", (info->m_peakIntensity - info->m_offset) / info->m_exposureTime);

	// 7. The delta of the fit
	if(result != NULL)
		string.AppendFormat("%.2e\t", result->m_delta);
	else
		string.AppendFormat("%.2e\t", 0.0);

	// 8. The chi-square of the fit
	if(result != NULL)
		string.AppendFormat("%.2e\t", result->m_chiSquare);
	else
		string.AppendFormat("%.2e\t", 0.0);

	// 9. The exposure time and the number of spectra averaged
	string.AppendFormat("%ld\t%ld\t", info->m_exposureTime, info->m_numSpec);

	// 10. The column/column error for each specie
	for(itSpecie = 0; itSpecie < nSpecies; ++itSpecie){
		if(result != NULL){
			if( (fabs(result->m_ref[itSpecie].m_column) > 5e-2) && (fabs(result->m_ref[itSpecie].m_columnError) > 5e-2))
				string.AppendFormat("%.2lf\t%.2lf\t", result->m_ref[itSpecie].m_column,		result->m_ref[itSpecie].m_columnError);
			else
				string.AppendFormat("%.2e\t%.2e\t", result->m_ref[itSpecie].m_column,		result->m_ref[itSpecie].m_columnError);
			string.AppendFormat("%.2lf\t%.2lf\t", result->m_ref[itSpecie].m_shift,		result->m_ref[itSpecie].m_shiftError);
			string.AppendFormat("%.2lf\t%.2lf\t", result->m_ref[itSpecie].m_squeeze,	result->m_ref[itSpecie].m_squeezeError);
		}else{
			string.AppendFormat("0.0\t0.0\t0.0\t0.0\t0.0\t0.0\t");
		}
	}

	// 11. The quality of the fit
	if(result != NULL)
		string.AppendFormat("%d\t", result->IsOK());
	else
		string.AppendFormat("%d\t", 1);

	// 12. The offset
	string.AppendFormat("%.0lf\t", info->m_offset);

	// 13. The 'flag' in the spectra
	string.AppendFormat("%d\n", info->m_flag);

	return SUCCESS;
}

/** Sorts the CDateTime-objects in the given array.
		Algorithm based on MergeSort (~O(NlogN)) */
void FileHandler::CEvaluationLogFileHandler::SortScans(CArray<Evaluation::CScanResult, Evaluation::CScanResult&> &array, bool ascending){
	unsigned long nElements = (unsigned long)array.GetSize(); // number of elements
	unsigned long it				= 0; // <-- iterator
	unsigned long halfSize	= nElements / 2;

	if(nElements <= 1){
		return; // <-- We're actually already done
	}else if(nElements <= 7){
		// For small lists, its much faster to do a bubble sorting of the 
		//	list since this does not require so much copying back and forth
		BubbleSortScans(array, ascending);
		return;
	}else{
		CArray <Evaluation::CScanResult, Evaluation::CScanResult&> left;
		CArray <Evaluation::CScanResult, Evaluation::CScanResult&> right;
		left.SetSize(halfSize);
		right.SetSize(halfSize);

		// Make two copies of the array, one of the first half and one of the second half
		while(it < halfSize){
			left.SetAtGrow(it, array.GetAt(it));
			++it;
		}
		while(it < nElements){
			right.SetAtGrow(it - halfSize, array.GetAt(it));
			++it;
		}

		// Sort each of the two halves
		SortScans(left,  ascending);
		SortScans(right, ascending);

		// Merge the two...
		MergeArrays(left, right, array, ascending);
	}
}

/** Merges the two arrays in a sorted way and stores the
		result in the output-array 'result' */
void FileHandler::CEvaluationLogFileHandler::MergeArrays(CArray<Evaluation::CScanResult, Evaluation::CScanResult&> &array1, CArray<Evaluation::CScanResult, Evaluation::CScanResult&> &array2, CArray<Evaluation::CScanResult, Evaluation::CScanResult&> &result, bool ascending){
	CDateTime	time1, time2;
	unsigned long it1 = 0; // iterator for array1
	unsigned long it2 = 0; // iterator for array2
	unsigned long itr = 0; // iterator for result
	unsigned long nE1	= (unsigned long)array1.GetSize();
	unsigned long nE2	= (unsigned long)array2.GetSize();

	// Clear the output-array
	result.RemoveAll();
	result.SetSize(nE1 + nE2); // we can just as well set the size of the array from the start

	// 1. As long as there are elements in both arrays, do this
	while(it1 < nE1 && it2 < nE2){
		// Get the times of the first and the second
		array1.GetAt(it1).GetStartTime(0, time1);
		array2.GetAt(it2).GetStartTime(0, time2);

			// Compare the two start-times
		if(time1 == time2){
			// if equal
			result.SetAt(itr++, array1.GetAt(it1++));
			continue;
		}else if(time1 < time2){
			// time1 < time2
			if(ascending){
				result.SetAt(itr++, array1.GetAt(it1++));
				continue;
			}else{
				result.SetAt(itr++, array2.GetAt(it2++));
				continue;
			}
		}else{
			// time1 > time2
			if(ascending){
				result.SetAt(itr++, array2.GetAt(it2++));
				continue;
			}else{
				result.SetAt(itr++, array1.GetAt(it1++));
				continue;
			}
		}
	}

	// 2. If we're out of elements in array2 but not in array1, do this
	while(it1 < nE1){
		result.SetAt(itr++, array1.GetAt(it1++));
	}

	// 3. If we're out of elements in array1 but not in array2, do this
	while(it2 < nE2){
		result.SetAt(itr++, array2.GetAt(it2++));
	}
}

/** Sorts the CScanResult-objects in the given array.
		Algorithm based on BubbleSort (~O(N2))
		Quite efficient for small arrays since the elements does not have to be copied
			and thus uses very little memory */
void FileHandler::CEvaluationLogFileHandler::BubbleSortScans(CArray<Evaluation::CScanResult, Evaluation::CScanResult&> &array, bool ascending){
	CDateTime time1, time2;
	bool change;
	unsigned long nElements = (unsigned long)array.GetSize(); // number of elements
	unsigned int k;
	CDateTime tmpTime;
	int tmpIndex;
	CArray<Evaluation::CScanResult, Evaluation::CScanResult&> copiedArray;

	// 1. Find the start-time of all the scans
	CDateTime *allStartTimes = new CDateTime[nElements];
	unsigned int *sortOrder = new unsigned int[nElements];
	for(k = 0; k < nElements; ++k){
		array[k].GetStartTime(0, allStartTimes[k]);
		sortOrder[k] = k;
	}

	// 2. Sort the start-times using bubble sort
	do{
		change = false;
		for(k = 0; k < nElements - 1; ++k){
			// If the second scan has started before the first, 
			//	then change the order, otherwise don't do anything
			if(allStartTimes[k+1] < allStartTimes[k]){
				tmpTime = allStartTimes[k];
				tmpIndex = sortOrder[k];

				allStartTimes[k] = allStartTimes[k+1];
				sortOrder[k] = sortOrder[k+1];

				allStartTimes[k+1] = tmpTime;
				sortOrder[k+1] = tmpIndex;

				change		= true;
			}else{
				continue;
			}
		}
	}while(change);
	
	// 3. Sort the array using the indices found in 'sortOrder'
	
	// 3a. Copy all items to a backup-list
	copiedArray.SetSize(nElements);
	for(k = 0; k < nElements; ++k){
		if(sortOrder[k] != k){
			copiedArray[k] = array[k];
		}
	}
	
	// 3b. Put the items back in the correct order
	for(k = 0; k < nElements; ++k){
		if(sortOrder[k] != k){
			array[k] = copiedArray[sortOrder[k]];
		}
	}

	// 4. Clean up!
	copiedArray.RemoveAll();
	delete [] allStartTimes;
	delete [] sortOrder;

	return;
}