/*
 *      Copyright (C) 2017 K-Sys
 *      https://www.ktv.zone/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#pragma once

#include "client.h"
#include "json.hpp"
#include "PVRRecorderJob.h"

class PVRRecorder
{
public:
	PVRRecorder(void);
	virtual ~PVRRecorder(void);

	PVR_ERROR 			getTimers(ADDON_HANDLE handle);
	PVR_ERROR 			addTimer(const PVR_TIMER &timer);
	PVR_ERROR 			deleteTimer(const PVR_TIMER &timer, bool bForceDelete);
	PVR_ERROR 			updateTimer(const PVR_TIMER &timer);
	PVR_ERROR 			getTimerTypes(PVR_TIMER_TYPE types[], int *size);
	int 				getTimersAmount(void);
	void 				loadCachedJobs(void);
	void 				saveJobs(void);

private:
  std::vector<PVRRecorderJob>  m_jobs;

};
