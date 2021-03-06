/* Implementation of core functionality of the OS					*/
/* this includes the main scheduling loop							*/
/* for comments on the functions see the associated .h-file */

/* ---------------------------------------------------------------- */
/* Include required external definitions */
#include <math.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "bs_types.h"
#include "globals.h"
#include "loader.h"
#include "core.h"
#include "executer.h"



/* ----------------------------------------------------------------	*/
/* Declarations of global variables visible only in this file 		*/

PCB_t process;		// the only user process used for batch and FCFS
PCB_t* pNewProcess;	// pointer for new process read from batch


/* ---------------------------------------------------------------- */
/*                Declarations of local helper functions            */


/* ---------------------------------------------------------------- */
/*                Externally available functions                    */
/* ---------------------------------------------------------------- */

void initOS(void)
{

	unsigned i;					// iteration variable

	/* init the status of the OS */
	// mark all process entries invalid
	for (i=0; i<MAX_PROCESSES; i++) processTable[i].valid=FALSE;
	process.pid=0;			// reset pid
}

void coreLoop(void)
{
	SchedulingEvent_t nextEvent;		// scheduling event to process
	unsigned int eventPid;				// pid of a process triggering an event
	PCB_t *candidateProcess = NULL;		// next process to start, already stored in process table
	pid_t newPid;						// pid of new process to start 
	PCB_t* nextReady = NULL;				// pointer to process that finishes next	
	unsigned delta = 0;					// time interval by which the systemTime is advanced
	Boolean isLaunchable = FALSE;	    // indicates if the next process in batch is launchable 


	do {	// loop until no more process to run 
		do {	// loop until no currenlty launchable process is in the batch file
			if (checkForProcessInBatch())
			{	// there is a process pending
				// predicate if the process is ready to be started is processed by the simulation: 
				if (isNewProcessReady())	// test if the process is ready to be started
				{	// the process is ready to be started
					isLaunchable = TRUE;
					newPid = getNextPid();					// get next valid pid
					initNewProcess(newPid, getNewPCBptr());			// Info on new process provided by simulation
					// now search for a suitable piece of memory for the process
					/* +++ this needs to be extended for real memory management +++	*/
					// this simple code must be replaced with searching for a memory location:
					if (usedMemory + processTable[newPid].size <= MEMORY_SIZE)
					{	// enough memory available, and location in memory found: start process
						processTable[newPid].status = running;	// all active processes are marked active
						runningCount++;						// and add to number of running processes
						usedMemory = usedMemory + processTable[newPid].size;	// update amount of used memory
						systemTime = systemTime + LOADING_DURATION;	// account for time used by OS
						logPidMem(newPid, "Process started and memory allocated");
						flagNewProcessStarted();	// process is now a running process, not a candidate any more 
					}
					else
					{
						processTable[newPid].status = blocked;	// not enough memory --> blocked due to "no ressources available"
						logPidMem(newPid, "Process too large, not started");
					}
				}
				else
				{
					isLaunchable = FALSE;
					logGeneric("Sim: Process read but it is not yet ready to run");
				}
			}
			// In case the candidate was started, check for another candiate before running the proceses
		} while ((!batchComplete) && (isLaunchable));

		// The checks for starting a process are done. Now work on the running processes
		/* +++++++++++++++++++++ */
		// get the process that will be completed next by searching the process table
		delta = runToNextEvent(&nextEvent, &eventPid);		// run the existing processes until a new arrives or one terminates

		//Now update OS-stats 
		updateAllVirtualTimes(delta);			// update all processes according to elapsed time

		// all running processes are updated
		systemTime = systemTime + delta;		// update system time by elapsed physical time
		// now quit the completed process (if any)
		/* +++ this needs to be extended for real memory management +++	*/
		if (nextEvent == completed)	// check if a process needs to be terminated
		{
			usedMemory = usedMemory - processTable[eventPid].size;	// mark memory of the process free
			logPidMem(eventPid, "Process terminated, memory freed");
			deleteProcess(&processTable[eventPid]);	// terminate process
			runningCount--;				// one running process less 
		}
		// loop until no running processes exist any more and no process is waiting t be started
	} while ((runningCount > 0) || (batchComplete == FALSE));
}

	
/* ----------------------------------------------------------------- */
unsigned getNextPid()
	{
	static unsigned pidCounter=1; 
	unsigned i=0;		// iteration variable;
	// determine next available pid make sure not to search infinitely
	while ((processTable[pidCounter].valid) && (i<MAX_PID))
		{
			// determine next available pid 
			pidCounter=(pidCounter+1) % MAX_PID;
			if (pidCounter==0) pidCounter++;	// pid=0 is invalid
			i++;		// count the checked entries
		}
	if (i==MAX_PID) return 0;			// indicate error
	else		   return pidCounter;
	}

int initNewProcess(pid_t newPid, PCB_t* pProcess)
/* Initialised the PCB at the given index of the process table with the		*/
/* process information provided in the PCB-struct giben by the pointer		*/
{
	if (pProcess == NULL)
		return 0;
	else {	/* PCB correctly passed, now delete it */
		processTable[newPid].pid = newPid;
		processTable[newPid].ppid = pProcess->ppid;
		processTable[newPid].ownerID = pProcess->ownerID;
		processTable[newPid].start = pProcess->start;
		processTable[newPid].duration = pProcess->duration;
		processTable[newPid].size = pProcess->size;
		processTable[newPid].usedCPU = pProcess->usedCPU;
		processTable[newPid].type = pProcess->type;
		processTable[newPid].status = init;
		processTable[newPid].valid = TRUE;
		// new process is initialised, now invalidate the PCB passed to this funtion
		pProcess->valid = FALSE; 
		return 1;
	}
}

int deleteProcess(PCB_t* pProcess)
/* Voids the PCB handed over in pProcess, this includes setting the valid-	*/
/* flag to invalid and setting other values to invalid values.				*/
/* retuns 0 on error and 1 on success										*/
{
	if (pProcess == NULL)
		return 0;
	else {	/* PCB correctly passed, now delete it */
		pProcess->valid = FALSE;
		pProcess->pid = 0;
		pProcess->ppid = 0;
		pProcess->ownerID = 0;
		pProcess->start = 0;
		pProcess->duration = 0;
		pProcess->size = 0;
		pProcess->usedCPU = 0;
		pProcess->type = os;
		pProcess->status = ended;
		return 1;
	}
}


/* ----------------------------------------------------------------- */
/*                       Local helper functions                      */
/* ----------------------------------------------------------------- */


