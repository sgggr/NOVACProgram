#pragma once

#ifndef WINDFIELDRECORD_H
#define WINDFIELDRECORD_H
#include <afxtempl.h>

#include "Common.h"
#include "WindField.h"

class CWindFieldRecord
{
	/** The original length of the array */
	static const int ORIGINAL_ARRAY_LENGTH = 10;

public:

	/** Default constructor */
	CWindFieldRecord(void);

	/** Default destructor */
	~CWindFieldRecord(void);


	// ------------------- PUBLIC METHODS -------------------------

	/** Inserts a given wind-field into the record */
	void	InsertWindField(const CWindField &wind);

	/** Searches through the read-in data and looks for the wind-field at the
			given time. 
			If the given time lies between to times in the 'database', 
				the interpolated wind-field is returned if the difference in time between the
				two data-points is not exceeding 24hours.
			If the given time exactly matches the time of one wind-field in the 'database'
				the wind-field at that time will be returned
			If the given time is before or after all times in the 'database' the function
				will return the closest known wind-value if the time difference is not larger
				than 3 hours, otherwise it will return FAIL.
			@param desiredTime - the time and date at which the wind-field is to be extracted
			@param desiredWindField - will on successful return be filled with the parameters of the wind-field at the given time.
			@return SUCCESS - if the wind could be interpolated
			@return FAIL - if the wind lies outside of the time-range of the 'database',
					or the distance between the two data-points to interpolate is larger than 24 hours. */
	RETURN_CODE InterpolateWindField(const CDateTime desiredTime, CWindField &desiredWindField);

	/** Returns the number of points in the database */
	long	GetRecordNum();

	// ------------------- PUBLIC DATA -------------------------


protected:
	// ------------------- PROTECTED METHODS -------------------------

	// ------------------- PROTECTED DATA -------------------------

	/** Information about the wind */
	CArray <CWindField, CWindField&> m_windField;

	/** How many wind-fields have been read in? */
	long		m_windFieldNum;

};

#endif