#include "StdAfx.h"
#include "CompositionMeasurement.h"

// This is for debugging only!
#include "../VolcanoInfo.h"

extern CConfigurationSetting g_settings;	// <-- The settings
extern CWinThread *g_comm;					// <-- the communication controller
extern CVolcanoInfo g_volcanoes;			// <-- A list of all known volcanoes

using namespace Composition;

CCompositionMeasurement::CCompositionMeasurement(void)
{
}

CCompositionMeasurement::~CCompositionMeasurement(void)
{
}

/** Runs through the history of the CSpectrometer and judges 
		if we should perform a composition measurement now. 
		@return true if a composition measurement should be started else return false */
bool CCompositionMeasurement::IsTimeForCompositionMeasurement(const Evaluation::CSpectrometer *spectrometer){
	CString dateStr, timeStr, debugFile, serial;
	Common common;
	common.GetDateText(dateStr);
	common.GetTimeText(timeStr);

	// For debugging...
	debugFile.Format("%sOutput\\%s\\Debug_CompositionMeas.txt", g_settings.outputDirectory, dateStr);
	serial.Format("%s", spectrometer->m_scanner.spec[0].serialNumber);
	
	int thisVolcano = -1;
	for(unsigned int k = 0; k < g_volcanoes.m_volcanoNum; ++k){
		if(Equals(spectrometer->m_scanner.volcano, g_volcanoes.m_name[k])){
			thisVolcano = k;
			break;
		}
	}

	//// -1. This checking should only be performed on master-channel spectrometers...
	//if(spectrometer->m_channel != 0)
	//	return false;

	// 0. Local handles, to get less dereferencing...
	int	stablePeriod	= 5;		// <-- the plume should be stable for at least 5 scans before we do any measurements
	int	interval		= 3 * 3600;	// <-- don't make these measurements more often than every 3 hours
	int	minColumn		= 150;		// <-- don't make composition measurements for too weak plumes

	// 1. Check the history of the spectrometer

	// 1a. Have we recieved any scans today?
	int nScans = spectrometer->m_history->GetNumScans();
	if(nScans < stablePeriod){ // <-- there must be enough scans received from the instrument today for us to make any wind-measurement
		return false; // <-- too few scans recieved today
	}

	// 2b. How long time has passed since the last composition measurement?
	int sPassed = spectrometer->m_history->SecondsSinceLastCompMeas();
	if(sPassed > 0 && sPassed < interval){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: Little time since last measurement\n", timeStr, serial);
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2c. What's the average time between the start of two scans today?
	double	timePerScan	= spectrometer->m_history->GetScanInterval();
	if(timePerScan < 0)
		return false; // <-- no scans have arrived today

	// 2d. Get the plume centre variation over the last few scans
	//			If any scan missed the plume, return false
	double centreMin, centreMax;
	if(false == spectrometer->m_history->GetPlumeCentreVariation(stablePeriod, 0, centreMin, centreMax)){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: At least one of the last %d scans has missed the plume\n", timeStr, serial, stablePeriod); 
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2e. If there's a substantial variation in plume centre over the last
	//			few scans then don't make any measurement
	if(fabs(centreMax - centreMin) > 50){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: Too large variation in plume centre\n", timeStr, serial); 
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2f. If the plumeCentre is at lower angles than 20 degrees from the horizon
	double plumeCentre = spectrometer->m_history->GetPlumeCentre(stablePeriod);
	if(fabs(plumeCentre) > 70){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: Too low plume\n", timeStr, serial); 
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2g. If any of the plume-edges is at lower angles than 10 degrees from the horizon
	double plumeEdge[2];
	spectrometer->m_history->GetPlumeEdges(stablePeriod, plumeEdge[0], plumeEdge[1]);
	if(fabs(plumeEdge[0]) > 80 || fabs(plumeEdge[1]) > 80){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: Too low plume edges\n", timeStr, serial);
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2h. If the plume completeness is less than 70%
	double plumeCompleteness = spectrometer->m_history->GetPlumeCompleteness(stablePeriod);
	if(plumeCompleteness < 0.7){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: Plume not complete\n", timeStr, serial);
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2i. Check so that the maximum column averaged over the last few scans is at least
	//			'minColumn' ppmm
	double maxColumn = spectrometer->m_history->GetColumnMax(stablePeriod);
	if(maxColumn < minColumn){
		FILE *f = fopen(debugFile, "a+");
		if(f != NULL){
			fprintf(f, "%s\t%s\tNo measurement: Too weak plume\n", timeStr, serial);
			fclose(f);
			UploadToNOVACServer(debugFile, thisVolcano, false);
		}
		return false;
	}

	// 2j. Check so that the average exposure-time over the last few scans is not
	//			too large. This to ensure that we can measure fast enough
	double expTime = spectrometer->m_history->GetExposureTime(stablePeriod);
	if(spectrometer->m_scanner.instrumentType == INSTR_GOTHENBURG){
		if(expTime < 0 || expTime > 300){
			FILE *f = fopen(debugFile, "a+");
			if(f != NULL){
				fprintf(f, "%s\t%s\tNo measurement: Too long exptimes\n", timeStr, serial);
				fclose(f);
				UploadToNOVACServer(debugFile, thisVolcano, false);
			}
			return false;
		}
	}else if(spectrometer->m_scanner.instrumentType == INSTR_HEIDELBERG){
		if(expTime < 0 || expTime > 1000){
			FILE *f = fopen(debugFile, "a+");
			if(f != NULL){
				fprintf(f, "%s\t%s\tNo measurement: Too long exptimes\n", timeStr, serial);
				fclose(f);
				UploadToNOVACServer(debugFile, thisVolcano, false);
			}
			return false;
		}
	}

	FILE *f = fopen(debugFile, "a+");
	if(f != NULL){
		fprintf(f, "%s\t%s\tOk to do composition measurement!!\n", timeStr, serial);
		fclose(f);
		UploadToNOVACServer(debugFile, thisVolcano, false);
	}

	// We've passed all tests, it's now ok to make a composition measurement
	return true;
}

/** Starts an automatic composition measurement for the supplied spectrometer */
void	CCompositionMeasurement::StartCompositionMeasurement(const Evaluation::CSpectrometer *spec){
	static CString message, spectrometerType;
	CString dateTime, directory;
	Common common;
	double plumeCentre, plumeEdge[2];
	int stablePeriod = 5;
	int measurementsInPlume;
	double	alphaStep = 3.6; // the angle between each measurement
	double  alpha;

	// The number of steps in one round...
	int	stepsPerRound		= spec->m_scanner.motor[0].stepsPerRound;

	// The number of degrees for each step the motor makes
	double stepAngle		= 360.0 / stepsPerRound;

	// The maximum exposure-time that we can afford...
	double	maxExpTime;
	if(spec->m_scanner.instrumentType == INSTR_GOTHENBURG){
		maxExpTime = 200;
	}else if(spec->m_scanner.instrumentType == INSTR_HEIDELBERG){
		maxExpTime = 1200;
	}

	// Get the centre of the plume
	plumeCentre = spec->m_history->GetPlumeCentre(stablePeriod);

	// Get the positions of the edges of the plume
	spec->m_history->GetPlumeEdges(stablePeriod, plumeEdge[0], plumeEdge[1]);
	
	// The number of measurement positions inside the plume...
	measurementsInPlume	= (plumeEdge[1] - plumeEdge[0]) / stepAngle;
	if(spec->m_scanner.instrumentType == INSTR_GOTHENBURG){
		if(measurementsInPlume < 5)
			return; // there's not enough space to make 5 measurements in the plume. Quit!
		measurementsInPlume	= min(measurementsInPlume, 8); // we can't make more than 8 measurements
	}else if(spec->m_scanner.instrumentType == INSTR_HEIDELBERG){
		if(measurementsInPlume < 2)
			return; // there's not enough space to make 3 measurements in the plume. Quit!
		measurementsInPlume	= min(measurementsInPlume, 4); // we can't make more than 8 measurements
	}
	// The angle between each measurement
	alphaStep						= (plumeEdge[1] - plumeEdge[0]) / measurementsInPlume;
	alphaStep						= stepAngle * floor(alphaStep / stepAngle); // the steps has to be a multiple of 1.8 degrees

	// The number of repetitions for the normal measurements, in the Manne-box only
	int	repetitions = 67; 
	if(spec->m_scanner.instrumentType == INSTR_GOTHENBURG){
		repetitions = 67;
	}else if(spec->m_scanner.instrumentType == INSTR_HEIDELBERG){
		repetitions = 30;
	}

	// allocate the strings	
	CString *serialNumber = new CString();
	CString *fileName = new CString();
	
	// 1. Get the serial-number of the selected spectrometer
	serialNumber->Format(spec->m_scanner.spec[0].serialNumber);

	// 2. Get the directory where to temporarily store the cfgonce.txt
	if(strlen(g_settings.outputDirectory) > 0){
		directory.Format("%sTemp\\%s\\", g_settings.outputDirectory, serialNumber);
		if(CreateDirectoryStructure(directory)){
			common.GetExePath();
			fileName->Format("%s\\cfgonce.txt", common.m_exePath);
		}else{
			fileName->Format("%scfgonce.txt", directory);
		}
	}else{
		common.GetExePath();
		fileName->Format("%s\\cfgonce.txt", common.m_exePath);
	}
	
	FILE *f = fopen(*fileName, "w");
	if(f == NULL){
		ShowMessage("Could not open cfgonce.txt for writing. Composition measurement failed!!");
		return;
	}

	// 3. Write the configuration-file

	// 3a. A small header 
	common.GetDateTimeText(dateTime);
	fprintf(f, "%%-------------Modified at %s------------%\n\n",dateTime);
	fprintf(f, "%%Questions? email\n% mattias.johansson@chalmers.se\n\n");

	// 3c. Write the Spectrum transfer information
	fprintf(f, "%% The following channels defines which channels in the spectra that will be transferred\n");
	fprintf(f, "STARTCHN=0\n");
	fprintf(f, "STOPCHN=2047\n\n");

	// 3d. Don't use real-time collection
	fprintf(f, "%% If Realtime=1 then the spectra will be added to work.pak one at a time.\n");
	fprintf(f, "%% If RealTime=0 then the spectra will be added to work.pak one scan at a time\n");
	fprintf(f, "REALTIME=0\n\n");

	if(spec->m_scanner.instrumentType == INSTR_GOTHENBURG){

		// 3e. Write the motor information
		//fprintf(f, "%% StepsPerRound defines the number of steps the steppermotor divides one round into\n");
		//fprintf(f, "STEPSPERROUND=%d\n",	stepsPerRound);
		//fprintf(f, "MOTORSTEPCOMP=%d\n",	spec->m_scanner.motor[0].motorStepsComp);
		fprintf(f, "%% If Skipmotor=1 then the scanner will not be used. ONLY FOR TESTING PURPOSES\n");
		fprintf(f, "SKIPMOTOR=0\n");
		if(spec->m_scanner.coneAngle == 90.0)
			fprintf(f, "DELAY=%d\n\n",				 200);
		else
			fprintf(f, "DELAY=%d\n\n",				 400);

		// 3f. Write the geometry (compass, tilt...)
		fprintf(f, "%% The geometry: compassDirection  tiltX(=roll)  tiltY(=pitch)  temperature\n");
		fprintf(f, "COMPASS=%.1lf 0.0 0.0\n\n", spec->m_scanner.compass);

		// 3g. Write other things
		fprintf(f, "%% Percent defines how big part of the spectrometers dynamic range we want to use\n");
		fprintf(f,  "PERCENT=%.2lf\n\n",			0.6);
		fprintf(f, "%% The maximum integration time that we allow the spectrometer to use. In milli seconds\n");
		fprintf(f,	"MAXINTTIME=%.0lf\n\n",		maxExpTime);
		fprintf(f, "%% The debug-level, the higher number the more output will be created\n");
		fprintf(f,  "DEBUG=1\n\n");

		// 3h. Write the measurement information
		fprintf(f, "%% sum1 is inside the specrometer [1 to 15]\n%%-----pos----time-sum1-sum2--chn--basename----- repetitions\n");

		// 3i. The offset-measurement
		fprintf(f, "MEAS=100 3 15 100 0 offset 1 0\n");
		
		// 3j. The dark-current measurement
		fprintf(f, "MEAS=100 10000 1 1 0 dark_cur 1 0\n");

		// 3k. Each of the measurements
		
		// 3k1. The first of the measurements outside the plume...
		alpha	= (plumeEdge[0] - 90) / 2;
		fprintf(f, "MEAS=%.0lf -1 15 %d 0 sky 1 0\n", alpha / stepAngle, repetitions);

		// 3k2. The measurements inside the plume
		alpha = plumeCentre - (measurementsInPlume - 1) * alphaStep / 2;
		for(int k = 0; k < measurementsInPlume; ++k){
			fprintf(f, "MEAS=%.0lf -1 15 %d 0 comp 1 0\n", alpha / stepAngle, repetitions);

			alpha += alphaStep;
		}

		// 3k2. The second of the measurements outside the plume...
		alpha	= (plumeEdge[1] + 90) / 2;
		fprintf(f, "MEAS=%.0lf -1 15 %d 0 sky 1 0\n", alpha / stepAngle, repetitions);
	}else{
		// Heidelberg instrument!
		double alpha_center_of_mass = spec->m_history->GetPlumeCentre(stablePeriod, 0);
		double phi_center_of_mass		= spec->m_history->GetPlumeCentre(stablePeriod, 1);

		int stepsPerRound1				=	abs (int(spec->m_scanner.motor[0].stepsPerRound));
		int stepsPerRound2				= abs (int(spec->m_scanner.motor[1].stepsPerRound));
		double stepsPerDegree1		=	stepsPerRound1/360.0;
		double stepsPerDegree2		=	stepsPerRound2/360.0;

		int motorPosition2				=	int (round(stepsPerDegree2*phi_center_of_mass));

		if(SUCCESS != CSpectrometerModel::ToString(spec->m_scanner.spec[0].model, spectrometerType)){
			spectrometerType.Format("HR2000");
		}
		fprintf (f, "SPECTROMETERTYPE=%s\n\n", spectrometerType);  //can spectrometertype be retrieved automatically from configuration?

		// 3e. Write the motor information
		//fprintf(f, "%% StepsPerRound defines the number of steps the steppermotor divides one round into\n");
		//fprintf(f, "STEPSPERROUND=%d %d\n",	-stepsPerRound1, -stepsPerRound2); //2 motors HD
		//fprintf(f, "MOTORSTEPSCOMP=%d %d\n",	int (round(spec->m_scanner.motor[0].motorStepsComp)), int (round(spec->m_scanner.motor[1].motorStepsComp))); //2 motors HD
		fprintf(f, "%% If Skipmotor=1 then the scanner will not be used. ONLY FOR TESTING PURPOSES\n");
		fprintf(f, "SKIPMOTOR=2\n");
		fprintf(f, "DELAY=%d\n\n",	 2); // what is an appropriate value

		// 3g. Write other things
		fprintf(f, "%% Percent defines how big part of the spectrometers dynamic range we want to use\n");
		fprintf(f,  "PERCENT=%.2lf\n\n",			0.8);
		fprintf(f, "%% The maximum integration time that we allow the spectrometer to use. In milli seconds\n");
		fprintf(f,	"MAXINTTIME=%.0lf\n\n",		1200.0);
		fprintf(f, "%% The pixel where we want to measure the intensity of the spectra \n");
		fprintf(f,	"CHANNEL=-1\n\n");
		fprintf(f, "%% The debug-level, the higher number the more output will be created\n");
		fprintf(f,  "DEBUG=1\n\n");

		fprintf(f, "%% sum1 is inside the specrometer [1 to 15]\n%%--zenith----azimuth----time-sum1-sum2--chn--basename-----repetitions\n");

		// 3i. The offset-measurement
		fprintf(f, "MEAS=%d %d 3 15 50 0 offset 1 0\n", stepsPerRound1/180, motorPosition2);
		
		// 3j. The dark-current measurement
		fprintf(f, "MEAS=%d %d 10000 1 1 0 dark_cur 1 0\n", stepsPerRound1/180, motorPosition2);

		// 3h. Each of the measurements
		
		bool useFirstSky = (plumeCentre >= 0) ? true : false;
		// 3k1. The first of the measurements outside the plume...
		if(useFirstSky){
			alpha	= (plumeEdge[0] - 90) / 2;
			fprintf(f, "MEAS=%.0lf %d -1 15 %d 0 sky 1 0\n", alpha / stepAngle, motorPosition2, repetitions);
		}

		// 3k2. The measurements inside the plume
		alpha = plumeCentre - (measurementsInPlume - 1) * alphaStep / 2;
		
		fprintf(f, "MEAS=%.0lf %d -1 15 %d 0 comp1 1 0\n", plumeCentre / stepAngle, motorPosition2, repetitions);

		// 3k2. The second of the measurements outside the plume...
		if(!useFirstSky){
			alpha	= (plumeEdge[1] + 90) / 2;
			fprintf(f, "MEAS=%.0lf %d -1 15 %d 0 sky2 1 0\n", alpha / stepAngle, motorPosition2, repetitions);
		}
	}

	// Close the file
	fclose(f);

	// 4. Tell the communication controller that we want to upload a file
	if(g_comm != NULL){
		g_comm->PostThreadMessage(WM_STATUS_UPLOAD, (WPARAM)serialNumber, (LPARAM)fileName);

		// 4b. Tell the user what we've done...
		message.Format("Telling spectrometer %s to perform composition measurement", serialNumber);
		ShowMessage(message);
	}
}

