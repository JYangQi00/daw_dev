/******************************************************************************
*
* CAEN SpA - Front End Division
* Via Vetraia, 11 - 55049 - Viareggio ITALY
* +390594388398 - www.caen.it
*
***************************************************************************//**
* \note TERMS OF USE:
* This program is free software; you can redistribute it and/or modify it under
* the terms of the GNU General Public License as published by the Free Software
* Foundation. This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. The user relies on the
* software, documentation and results solely at his own risk.
******************************************************************************/

#define DAWrunner_Release        "1.0"
#define DAWrunner_Release_Date   "May 2017"

#include "CAENDigitizer.h"
#include "CAENDigitizerType.h"
#include "DAWDemoFunc.h"
#include <time.h>
#include <sys/stat.h>
#include <chrono>

//code added by chengjie 
#include <unistd.h>

char path[256];

static long get_time()
{
	long time_ms;
#ifdef WIN32
	struct _timeb timebuffer;
	_ftime(&timebuffer);
	time_ms = (long)timebuffer.time * 1000 + (long)timebuffer.millitm;
#else
	struct timeval t1;
	struct timezone tz;
	gettimeofday(&t1, &tz);
	time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
#endif
	return time_ms;
}

int main(int argc, char *argv[])
{
	uint32_t ev ; 
	int  board, chan; // loop indices
	int *handle = NULL;// pointer to handles
	ERROR_CODES ErrCode = ERR_NONE;

	uint32_t SPIBusyAddr = 0x1088;
	uint32_t SPIBusy;

	// Data and config structures
	CAEN_DGTZ_730_DAW_Event_t      **Event = NULL; // as many events as the maximum number obtainable with a BLT transfer access must be allocated 
	
	DAWPlot_t                       PlotVar; // struct containing the plot options	
	DAWConfig_t			            ConfigVar; // struct containing the config file options
	// File I/O and related variables
	FILE **RawFile;
	FILE ***WaveFile;
	FILE *f_ini = NULL;
	int *RawFileIndex;
	struct stat buf;
	int FileIndex = 0;
	uint64_t PrevCheckTime; // printout and plot time
	Counter_t *Counter, *CounterOld;
	int PrintFlag=0,PlotFlag=0;
	uint32_t AllocatedSize,BufferSize;
	char *buffer = NULL;
	char ConfigFileName[255];
	char tmpConfigFileName[512];
	uint32_t *NumEvents;
	int EventPlotted=-1;
	uint32_t BLTn,MaxBLTn=0,MaxBLTnHIndex;
	uint32_t MSize, MaxMSize=0;
	int MaxMSizeHIndex;
	struct stat info;
	uint32_t BoardInfo;
	// code added by chengjie 
	printf("argc number : %d\n",argc);
	printf("**************************************************************\n");
	printf("                      X725/X730 DAW Demo %s\n", DAWrunner_Release);
	printf("**************************************************************\n");
	
#ifdef  WIN32
	sprintf(path, "%s\\DAW_DEMO\\", getenv("USERPROFILE"));
	_mkdir(path);
#else
	sprintf(path," ");
#endif
	
	//Jianyang's reformatting
	//********************** Command Line Argument Handling **********************
	
	int c = 0;
	int iNRuns = 1;
	
	bool bConfigFileExternal = false;
	bool bDesignTimeOn = false; //The time the program will run
	bool bExternalPath = false;
	bool bExternalFile = false;
	
	char cExternalOutPath[400];
	char cRunMode[200];
	char* runID;
	
	int useless_set=0;
	int design_time;
	
	bool busy = false;

	while ((c = getopt(argc, argv, "c:T:u:p:r:n:")) != -1){
		switch (c){
			case 'c':
				bConfigFileExternal = true;
				strcpy(ConfigFileName, optarg);
				ErrCode = OpenConfigFile(&f_ini, ConfigFileName);
				break;
			case 'T':
				bDesignTimeOn = true;
				design_time = atoi(optarg);
				printf("Designed run_time %d s \n", design_time);
				break;
			case 'u':
				useless_set=atoi(optarg);
				printf("useless_data delete %d\n",useless_set);
				break;
			case 'p':
				bExternalPath = true;
				strcpy(cExternalOutPath, optarg);
				break;
			case 'r':
				bExternalFile = true;
				strcpy(cRunMode, optarg);
				break;
			case 'n':
				iNRuns = atoi(optarg);
				break;
			default:
				break;
		}
	}
	
	runID = time_stamp();

	if (!bConfigFileExternal) {
		strcpy(tmpConfigFileName, DEFAULT_CONFIG_FILE);
		sprintf(ConfigFileName, "%s%s", path, tmpConfigFileName);
	}
	
	if (ErrCode == ERR_NONE) ErrCode = ParseConfigFile(f_ini, &ConfigVar);
	if (f_ini!=NULL) fclose(f_ini);
	
	//Adjust the config file if there are user specified arguments
	
	if (bExternalPath) {
		printf("Path specified in terminal flag\n");
		strcat(cExternalOutPath, cRunMode);
		strcat(cExternalOutPath, "/");

		#ifdef WIN32
		sprintf(ConfigVar.OutFilePath, "%s%s", path, cExternalOutPath);
		#else
		sprintf(ConfigVar.OutFilePath, "%s%s", getenv("HOME"), cExternalOutPath);
		#endif
		strcpy(ConfigVar.OutFilePath, cExternalOutPath);

		MakePath(cExternalOutPath, runID);

	}
	
	if (bExternalFile) strncpy(ConfigVar.OutFileName, cRunMode, 200);
	
	
	// Allocate space for handles and counters according to the number of boards in the acquisition chain
	if (ErrCode == ERR_NONE) {
		if (((handle = (int*)calloc(ConfigVar.Nhandle,sizeof(int))) == NULL) ||
			((Counter = (Counter_t*)calloc(ConfigVar.Nhandle, sizeof(Counter_t))) == NULL) ||
			((CounterOld = (Counter_t*)calloc(ConfigVar.Nhandle, sizeof(Counter_t))) == NULL) ||
			((NumEvents = (uint32_t*)calloc(ConfigVar.Nhandle, sizeof(int))) == NULL) ||
			((Event = (CAEN_DGTZ_730_DAW_Event_t**)calloc(ConfigVar.Nhandle, sizeof(CAEN_DGTZ_730_DAW_Event_t*))) == NULL) ||
			((RawFileIndex = (int*)calloc(ConfigVar.Nhandle, sizeof(int))) == NULL) ||
			((RawFile = (FILE**)calloc(ConfigVar.Nhandle, sizeof(FILE*))) == NULL) ||
			((WaveFile = (FILE***)calloc(ConfigVar.Nhandle, sizeof(FILE**))) == NULL)
			) ErrCode = ERR_MALLOC;
	}

	// Open the digitizer and read the board information
	if (ErrCode == ERR_NONE) {
		printf("Open digitizers\n");
		if (OpenDigitizer(handle, &ConfigVar)) ErrCode = ERR_DGZ_OPEN;
	}
	
	// Print board info and set board-specific parameters
	if (ErrCode == ERR_NONE) {
		printf("Get board info and set board-specific parameters\n");
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			if (CAEN_DGTZ_GetInfo(handle[board],&ConfigVar.BoardConfigVar[board]->BoardInfo)) ErrCode = ERR_BOARD_INFO_READ;
			else {
				printf("***************************************\n");
				printf("Connected to CAEN Digitizer Model %s\n", ConfigVar.BoardConfigVar[board]->BoardInfo.ModelName);
				printf("Board serial number %d\n", ConfigVar.BoardConfigVar[board]->BoardInfo.SerialNumber);
				printf("ROC FPGA Release is %s\n", ConfigVar.BoardConfigVar[board]->BoardInfo.ROC_FirmwareRel);
				printf("AMC FPGA Release is %s\n", ConfigVar.BoardConfigVar[board]->BoardInfo.AMC_FirmwareRel);
				CAEN_DGTZ_ReadRegister(handle[board], 0x108C, &BoardInfo);
				if (((BoardInfo >> 8) & 0xff) != DAW_FW_ID) { ErrCode = ERR_WRONG_FW; break; }
			}
		}
		printf("***************************************\n");
	}

    // Program the digitizer
	if (ErrCode == ERR_NONE) {
		printf("Program digitizers\n");
		if (ProgramDigitizers(handle, &ConfigVar)) ErrCode = ERR_DGZ_PROGRAM;
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			// get the handles with the highest values of event size, events/block transfer
			CAEN_DGTZ_GetMaxNumEventsBLT(handle[board], &BLTn); if (BLTn > MaxBLTn) { BLTn = MaxBLTn; MaxBLTnHIndex = board; }
			if ((MSize = CheckMallocSize(handle[board])) > MaxMSize) { MaxMSize = MSize; MaxMSizeHIndex = board; }
		}
	}
	
	CAEN_DGTZ_TriggerMode_t trigmode;
	CAEN_DGTZ_GetExtTriggerInputMode(handle[0], &trigmode);
	printf("External trigger mode is %i\n", trigmode);

	// Open the output files
	if (ErrCode == ERR_NONE) {
		if (stat(ConfigVar.OutFilePath, &info) != 0) {
			#ifdef  WIN32
			if (_mkdir(ConfigVar.OutFilePath) != 0) { printf("Output directory %s could not be created. Please verify that the path exists and is writable\n", ConfigVar.OutFilePath); ErrCode = ERR_OUTDIR_OPEN; }
			else printf("Output directory %s created\n", ConfigVar.OutFilePath);
			#else 
			if (mkdir(ConfigVar.OutFilePath, 0777) != 0) { printf("Output directory %s could not be created. Please verify that the path exists and is writable\n", ConfigVar.OutFilePath); ErrCode = ERR_OUTDIR_OPEN; }
			else printf("Output directory %s created\n", ConfigVar.OutFilePath);
			#endif
		}
		if (ErrCode == ERR_NONE) {
			for (board = 0; board < ConfigVar.Nhandle; board++) {
				RawFileIndex[board] = 0;
				if (ConfigVar.OFRawEnable) { if ((ErrCode = OpenRawFile(RawFile + board, board, RawFileIndex[board], ConfigVar.OutFilePath, ConfigVar.OutFileName, runID)) != ERR_NONE) break; }
				if (ConfigVar.OFWaveEnable) {
					if ((*(WaveFile + board) = (FILE**)calloc(ConfigVar.BoardConfigVar[board]->BoardInfo.Channels, sizeof(FILE*))) == NULL) { ErrCode = ERR_MALLOC; break; }
					if ((ErrCode = OpenWaveFile(WaveFile + board, board, ConfigVar.BoardConfigVar[board], ConfigVar.OutFilePath, ConfigVar.OutFileName)) != ERR_NONE) break;
				}
			}
		}
	}
 
    // WARNING: The mallocs must be done after the digitizer programming
    // Allocate memory for the readout buffer
	if (ErrCode == ERR_NONE) {
		printf("Readout buffer malloc\n");
		if (CAEN_DGTZ_MallocReadoutBuffer(handle[MaxMSizeHIndex], &buffer, &AllocatedSize)) ErrCode = ERR_MALLOC;
	}
	// Allocate memory for the events 
	if (ErrCode == ERR_NONE) {
		printf("Event malloc\n");
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			if (CAEN_DGTZ_MallocDPPEvents(handle[MaxBLTnHIndex], (void**)&Event[board], &AllocatedSize)) ErrCode = ERR_MALLOC;
		}
	}

	// reset counters
	if(ErrCode==ERR_NONE) {
		for (board = 0; board < ConfigVar.Nhandle; board++) {
			ResetCounter(Counter + board);
			ResetCounter(CounterOld + board);
		}
	}

	// No memory allocation for waveform required in this firmware
    // Open the plotter and configure its options
	if (ErrCode == ERR_NONE) {
		printf("Open plotter\n");
		ErrCode = OpenPlotter(&ConfigVar, &PlotVar);
	}   

	/******************************************************
	 * 													  *
	 *                    READOUT LOOP                    *
	 *													  * 
	 ******************************************************/


    // Readout Loop 
	if (ErrCode == ERR_NONE) {

		for (int run = 0; run<iNRuns; run++){
			// int run = 0;
			printf("[s] start/stop the acquisition, [q] quit, [space key] help\n");
			if (run>0) {
				runID = time_stamp();
				MakePath(cExternalOutPath, runID);

				for (board = 0; board < ConfigVar.Nhandle; board++) {
					if (RawFile[board] != NULL) {
						fstat(fileno(RawFile[board]), &buf);
						RawFileIndex[board] = 0; 
						OpenRawFile(&RawFile[board], board, RawFileIndex[board], ConfigVar.OutFilePath, ConfigVar.OutFileName, runID);
					}
				}

				ConfigVar.Quit = 0;
			}

			FILE *metadata;
			FILE *deadtimes;
			char metadata_path[600], deadtime_path[600];
			sprintf(metadata_path, "%s%s/metadata_%s.ini", cExternalOutPath, runID, runID);
			sprintf(deadtime_path, "%s%s/deadtimes_%s.bin", cExternalOutPath, runID, runID);
			metadata = fopen(metadata_path, "w+");
			deadtimes = fopen(deadtime_path, "w+");
			fputs("[metadata]\n", metadata);
			fputs("UnixTime = ", metadata);
			char timestamp_buffer[100];
			std::chrono::time_point<std::chrono::high_resolution_clock> time_start = std::chrono::high_resolution_clock::now();
			unsigned long time_start_ns = std::chrono::time_point_cast<std::chrono::nanoseconds> (time_start).time_since_epoch().count();
			sprintf(timestamp_buffer, "%lu", time_start_ns);
			fputs(timestamp_buffer, metadata);
			fclose(metadata);

			int auto_already=0 ; 
			int auto_stop=0 ; 
			int start_time= time((time_t *)NULL);
			int useless_count=1;

			printf("start acquisition time : %d\n",start_time);
			PrevCheckTime = get_time();

			int ret = 0;
			while (!ConfigVar.Quit) {
				CheckKeyboardCommands(handle, &ConfigVar);

				if (UpdateTime(ConfigVar.PlotRefreshTime, &PrevCheckTime) && ConfigVar.AcqRun) PrintFlag = 1;
				// if continuous trigger is enabled, send software triggers

				if (ConfigVar.ContTrigger) for (board = 0; board < ConfigVar.Nhandle; board++) CAEN_DGTZ_SendSWtrigger(handle[board]);
				
				// // Read data from the board
				for (board = 0; board < ConfigVar.Nhandle; board++) {

					for (chan = 0; chan < MAX_V1730_CHANNEL_SIZE; chan++) {
						ret = CAEN_DGTZ_ReadRegister(handle[board], SPIBusyAddr+(chan<<8), &SPIBusy);
						if (SPIBusy % 2 == 1) {
							// printf("Board %i busy! Restarting acquisition\n", board);
							printf("Board %i busy! Tagging dead time\n", board);

							std::chrono::time_point<std::chrono::high_resolution_clock> deadtime_start = 
								std::chrono::high_resolution_clock::now();
							unsigned long deadtime_start_ns = 
								std::chrono::time_point_cast<std::chrono::nanoseconds> (deadtime_start).time_since_epoch().count();
							printf("Deadtime Start: %lu\n", deadtime_start_ns);
							fwrite(&deadtime_start_ns, sizeof(deadtime_start_ns), 1, deadtimes);

							ret = CAEN_DGTZ_SWStopAcquisition(handle[board]);
							usleep(1000);
							if (RawFile[board] != NULL) {
								RawFileIndex[board]++;
								OpenRawFile(&RawFile[board], board, RawFileIndex[board], ConfigVar.OutFilePath, ConfigVar.OutFileName, runID);
							}
							ret = CAEN_DGTZ_SWStartAcquisition(handle[board]);

							std::chrono::time_point<std::chrono::high_resolution_clock> deadtime_end = 
								std::chrono::high_resolution_clock::now();
							unsigned long deadtime_end_ns = 
								std::chrono::time_point_cast<std::chrono::nanoseconds> (deadtime_end).time_since_epoch().count();
							printf("Deadtime End: %lu\n", deadtime_end_ns);
							fwrite(&deadtime_end_ns, sizeof(deadtime_end_ns), 1, deadtimes);

							break;
						}
					}

					// Nhandle is the number of target boards . 
					if (CAEN_DGTZ_ReadData(handle[board], CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer, &BufferSize)) {
						ErrCode = ERR_READOUT;
						break;
					}

					// if (board == ConfigVar.BoardPlotted) EventPlotted = -1;

					if (BufferSize > 0) { // The BLT returned data
						Counter[board].ByteCnt += BufferSize;
						// allocate the buffered data in the struct and return the number of events
						// if (CAEN_DGTZ_GetDPPEvents(handle[board], buffer, BufferSize / 4, (void**)&Event[board], &NumEvents[board])) {
						// 	ErrCode = ERR_EVENT_BUILD;
						// 	break;
						// }

						Counter[board].MB_TS = (uint64_t)(get_time());

						// Counter[board].MB_TS = Event[board][NumEvents[board] - 1].timeStamp;
						
						// Counter[board].TrgCnt += NumEvents[board];
						if (ConfigVar.OFRawEnable) 
						{
							if (useless_count>useless_set)
							{ 
								fwrite(buffer, BufferSize, 1, RawFile[board]);
							}
							else 
							{
								useless_count=useless_count+1; 
							}
						}
					} else {
						NumEvents[board] = 0;
						continue; // the BLT returned no data, acquisition still on
					} 

					// check file size and open new file if the file size is larger than the value set in the config file
					if (RawFile[board] != NULL) {
						fstat(fileno(RawFile[board]), &buf);
						if ((int)(buf.st_size / MB_SIZE) > ConfigVar.MaxFileSize)
						{ RawFileIndex[board]++; 
						OpenRawFile(&RawFile[board], board, RawFileIndex[board], ConfigVar.OutFilePath, ConfigVar.OutFileName, runID);
						}
					}

					// // Analyze data for each event		
					// for (ev = 0; ev < NumEvents[board]; ev++) {	
					// 	for (chan = 0; chan < MAX_V1730_CHANNEL_SIZE; chan++) {
					// 		if (Event[board][ev].chmask & (1 << chan)) {
					// 			// add events
					// 			Counter[board].TrgCnt[chan]++;
					// 			// increase truncate counters if the related header flag is set
					// 			if (Event[board][ev].Channel[chan]->truncate) Counter[board].OFCnt[chan]++;
					// 			// remember the last event of the buffer where the plotted channel is present 
					// 			if ((board == ConfigVar.BoardPlotted) && (chan == ConfigVar.EnableTrack)) EventPlotted = ev;
					// 		}
					// 	}
					// }
				}



				// Print event info for the selected board and channel
				if ((PrintFlag || ConfigVar.SinglePlot) && ConfigVar.AcqRun) {
					// if plotflag is still 1 no event from the channel selected for plotting was found
					if(PlotFlag==1 && Counter[ConfigVar.BoardPlotted].ByteCnt!=CounterOld[ConfigVar.BoardPlotted].ByteCnt) {
						printf("The channel selected for plotting (board #%d, channel #%d) was not present in data\n", ConfigVar.BoardPlotted, ConfigVar.EnableTrack);
						PlotFlag = 0;
					}
					printf("==========================================\n");
					for (board = 0; board < ConfigVar.Nhandle; board++) {
						// printf("Board %d", board); if (board == ConfigVar.BoardPlotted) printf("(plotted): \n"); else printf("         : \n");
						PrintData(Counter + board, CounterOld + board,&ConfigVar);
						// for (chan = 0; chan < MAX_V1730_CHANNEL_SIZE; chan++) {
						// 	if ((ConfigVar.BoardConfigVar[board]->EnableMask & (1 << chan)) && (Counter[board].OFCnt[chan] != CounterOld[board].OFCnt[chan])) {
						// 		printf("fraction of truncated events in board %d, channel %d: %.2f%%\n", board, chan, (float)(100 * (Counter[board].OFCnt[chan] - CounterOld[board].OFCnt[chan])) / (float)NumEvents[board]);
						// 	}
						// }
						// save waves
						// if (ConfigVar.OFWaveEnable  ) {
						// 	if (useless_count>useless_set)
						// 	{
						// 	OpenWaveFile(WaveFile + board, board, ConfigVar.BoardConfigVar[board], ConfigVar.OutFilePath, ConfigVar.OutFileName);
						// 	WaveWrite(*(WaveFile + board), *(Event + board), ConfigVar.BoardConfigVar[board]);
						// 	}
						// 	else 
						// 	{
						// 		useless_count=useless_count+1; 
						// 	}
						// }
					}
					PrintFlag = 0;
					// if(ConfigVar.PlotEnable || ConfigVar.SinglePlot) PlotFlag = 1;
					// ConfigVar.SinglePlot = 0; // reset one-shot plot ("p" key during acquisition) 
				}

				// // Plotting
				// if (PlotFlag==1) { // if no event from the selected channel has been found, PlotFlag remains active
				// 	if ((~ConfigVar.BoardConfigVar[ConfigVar.BoardPlotted]->EnableMask) & (1 << ConfigVar.EnableTrack)) {
				// 		printf("The channel selected for plotting (board #%d, channel #%d) is not enabled\n", ConfigVar.BoardPlotted, ConfigVar.EnableTrack);
				// 		PlotFlag = 0;
				// 	} else if (EventPlotted != -1) {
				// 		PlotEvent(&ConfigVar, &PlotVar, &Event[ConfigVar.BoardPlotted][EventPlotted]);
				// 		PlotFlag = 0;
				// 	}
				// }

				if (auto_stop==1)
				{
					ConfigVar.Quit=1;	
				}

				
				if (bDesignTimeOn && auto_already==0 )
				{
					// code added by Chengjie
					int i ; 
					ConfigVar.Quit=0;
					ConfigVar.AcqRun=1;
					for (i = 0; i < ConfigVar.Nhandle; i++) 
					{ 
						if(ConfigVar.BoardConfigVar[i]->StartMode == CAEN_DGTZ_SW_CONTROLLED)
						{
							CAEN_DGTZ_SWStartAcquisition(handle[i]);
						}
					}
					auto_already=1; 
					usleep(500);
				}
			
				// new code added by Chengjie 	
				if (bDesignTimeOn)
				{
					int now_time=time((time_t *)NULL);
					int i ; 
					// printf("%d\n",now_time);
					if ( now_time > start_time + design_time)
					{
						for (i = 0; i < ConfigVar.Nhandle; i++) 
						{
							if (ConfigVar.BoardConfigVar[i]->StartMode == CAEN_DGTZ_SW_CONTROLLED) 
							{CAEN_DGTZ_SWStopAcquisition(handle[i]);	}
						}
						// ConfigVar.Quit=1;
						ConfigVar.AcqRun=0;
						auto_stop=1; 
						// We now wait for the next loop to stop 
					}
				}
				
				//new code added by Jianyang
			}

			fclose(deadtimes);
		}
		
	}

    if (ErrCode) {
        printf("\a%s\n", ErrMsg[ErrCode]);
        printf("Press a key to quit\n");
		getch();
    }

    // stop the acquisition
	if (ErrCode != ERR_DGZ_OPEN) for (board = 0; board < ConfigVar.Nhandle; board++) CAEN_DGTZ_SWStopAcquisition(handle[board]);
	// close the output files (if allocated)
	for (board = 0; board < ConfigVar.Nhandle; board++) {
		if (RawFile[board] != NULL) fclose(RawFile[board]);
	}
	// close the plotter
	if (PlotVar.plotpipe != NULL) ClosePlotter(&PlotVar.plotpipe);
	// free buffers (if allocated)
	if (buffer != NULL) CAEN_DGTZ_FreeReadoutBuffer(&buffer);
	for (board = 0; board < ConfigVar.Nhandle; board++) {
		if (Event[board] != NULL) {
			// free events and waveforms (if allocated)
			CAEN_DGTZ_FreeDPPEvents(handle[MaxBLTnHIndex],(void**)&Event[board]);
		}
	}
	if (ErrCode != ERR_DGZ_OPEN) for (board = 0; board < ConfigVar.Nhandle; board++) CAEN_DGTZ_CloseDigitizer(handle[board]);
	return 0;
}
