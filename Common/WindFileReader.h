#pragma once

#include "WindFieldRecord.h"

// Include synchronization classes
#include <afxmt.h>

namespace FileHandler{
	class CWindFileReader
	{
	public:
		/** Default constructor */
		CWindFileReader(void);

		/** Default destructor */
		~CWindFileReader(void);

		/** The name and path of the wind-information file */
		CString m_windFile;

		// ------------------- PUBLIC METHODS -------------------------

		/** Reads the wind file */
		RETURN_CODE ReadWindFile();

		/** Writes the contents of this object to a new wind file */
		RETURN_CODE WriteWindFile(const CString fileName);

		/** Returns an interpolation from the most recently read in
				wind-field */
		RETURN_CODE InterpolateWindField(const CDateTime desiredTime, CWindField &desiredWindField);

		/** Returns the number of points in the database */
		long GetRecordNum();

		// ------------------- PUBLIC DATA -------------------------
		bool m_containsWindDirection; // True if the last wind-field file read contains a wind-direction
		bool m_containsWindSpeed;     // True if the last wind-field file read contains a wind-speed
		bool m_containsPlumeHeight;   // True if the last wind-field file read contains a plume-height

	protected:

		typedef struct LogColumns{
			int altitude;
			int windSpeed;
			int windSpeedError;
			int windDirection;
			int windDirectionError;
			int plumeHeight;
			int plumeHeightError;
			int date;
			int time;
		}LogColumns;

		// ------------------- PROTECTED METHODS -------------------------

		/** Reads the header line for the file and retrieves which 
		  column represents which value. */
		void ParseFileHeader(const char szLine[8192]);

		/** Resets the information about which column data is stored in */
		void ResetColumns();

		/** Parses the section containing the source of the wind-field
			The string should be converted to lower-case characters before
			calling this function */
		void ParseSourceString(const char szLine[8192], MET_SOURCE &source);

		// ------------------- PROTECTED DATA -------------------------

		/** Keeping track of which column contains what */
		LogColumns m_col;

		/** This class contains critical sections of code */
		CCriticalSection m_critSect;

		/** Information about the wind */
		CWindFieldRecord m_windRecord;
	};
}