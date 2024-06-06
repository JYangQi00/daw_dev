This is a software based on the DAW_Demo

Version 1.1 :  Allow it to take data for a fixed time 

```
DAW_Demo /etc/DAW_Demo/DAW_Config.txt 23
```

Version 1.2 : Allow it to delete the first useless samples 
```
DAW_Demo /etc/DAW_Demo/DAW_Config.txt 23 200
```


Version 1.3 : Add support to the A4818 Adapter

DAW_Config.txt : OPEN A4818 0 0 


Version 1.4 : Migrate DAW_Demo from c to c++ and new makefile

=== Additions from Jianyang ===
The primary change is for the dead-time handling. When the event rate gets high, DAW basically cannot take data as the channels will be out of sync with each other. To work around this:
- In the readout loop, read the "BUSY" flag for each channel
- If any of the channels are busy, stop data taking
- Save the current data
- Mark down the timestamp at which data taking was stopped
- Restart data taking
- Mark the timestamp at which data taking was restarted

-----------------------------------------------------------------------------

                   --- CAEN SpA - Computing Division ---

                                www.caen.it

  -----------------------------------------------------------------------------

  Program: DAW_Demo


  -----------------------------------------------------------------------------


  Content
  -----------------------------------------------------------------------------

  README.txt        :  This file.

  ReleaseNotes.txt  :  Revision History and notes.

   src		:  Source files
   inc		:  Include files
                   template specific files (config.txt)


  System Requirements
  -----------------------------------------------------------------------------
  - Linux 32/64Bit
  - Digitizer X730/ x725 family with DAW firmware
  - CAENDigitizer Lib
  - CAENVME Lib 
  - g++ 


  How to get support
  ------------------

  CAEN makes available the technical support of its specialists for requests
  concerning CAEN products. Use the support form available at the following link:
  https://www.caen.it/support-services/support-form
  
  Tsinghua Relics Collaboration : Chengjie Jia
  Wechat : MeAPikachu 
  Gmail: meapikachu@gmail.com


  Description:
  -----------------------------------------------------------------------------
  Takes data using the DAW firmware

  Syntax
  -----------------------------------------------------------------------------
  DAW_Demo [config file]
