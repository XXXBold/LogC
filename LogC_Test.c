



#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <assert.h>
#include "LogC.h"

#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  #define LOGC_TEST_THREADS
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */

#ifdef LOGC_TEST_THREADS
  #define LOGC_TEST_THREADS_COUNT 10
  #define LOGC_TEST_THREADS_ENTRIES 100
  int LogTest_Threads_m(LogC ptagLog);
  int iLogTest_ThreadsStart_m;
  #ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN /* Avoid include of useless windows headers */
      #include <windows.h>
      #include <process.h>
    #undef LOGC_TEST_THREADS
    #undef WIN32_LEAN_AND_MEAN
    #define LOGC_TEST_THREADS 1
    typedef unsigned int TThreadReturn;
  #elif defined(__unix__)
    #include <unistd.h> /* For determining the current POSIX-Version */
    #if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L) /* POSIX IEEE 1003.1-2001 or newer */
      #include <pthread.h>
      #undef LOGC_TEST_THREADS
      #define LOGC_TEST_THREADS 2
      typedef void* TThreadReturn;
    #else
      #error POSIX Version too old, must be POSIX IEEE 1003.1-2001 or newer for pthreads.
    #endif /* POSIX Version */
  #else
    #error No Threading system implemented!
  #endif
#endif

#define LOGC_TEST_TEXT(log,logtype,...) assert(!LOG_TEXT(log,logtype,__VA_ARGS__))

#define LOGC_TEST_TRACE(txt) puts("LOGC_TEST: " txt "\n" \
                                  "--------------------------------------------------------------------------------")

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  #define LOGFILE_PATH  "Test1.log"
  #define LOGFILE_PATH2 "Test2.log"
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  #define LOG_MAX_STORAGE_COUNT 5
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

#ifdef LOGFILE_PATH
  int LogTest_File_g(LogC log);
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
  int LogTest_Storage_g(LogC log);
#endif /* LOG_MAX_STORAGE_COUNT */

#define LOGTEST_EXIT_FAILURE(log) if(LogC_End(log)){ \
                                    LOGC_TEST_TRACE("LOGTEST_EXIT_FAILURE(): LogC_End() failed!"); exit(EXIT_FAILURE); \
                                  }

LogC logCTest_m;
typedef struct
{
  unsigned int options;
  const char *description;
}TagLogOptions;

static const TagLogOptions logOptions_m[]={{0,"None"},
                                           {LOGC_OPTION_TIMESTAMP_LOCALTIME,"Localtime"},
                                           {LOGC_OPTION_IGNORE_STDOUT,"Ignore stdout"},
                                           {LOGC_OPTION_IGNORE_STDERR,"Ignore stderr"}
                                          };

static const TagLogOptions logPrefixFormat_m[]={{0,"None"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_DATE,"Date"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_TIME,"Time"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS,"Milliseconds"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_DATE|LOGC_OPTION_PREFIX_TIMESTAMP_TIME,"Date + Time"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_DATE|LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS,"Date + Milliseconds"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_TIME|LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS,"Time + Milliseconds"},
                                                {LOGC_OPTION_PREFIX_TIMESTAMP_DATE|LOGC_OPTION_PREFIX_TIMESTAMP_TIME|LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS,"Date + Time + Milliseconds"},
                                                {LOGC_OPTION_PREFIX_LOGTYPETEXT,"Logtypetext"},
                                                {LOGC_OPTION_PREFIX_FILEINFO,"Fileinfo"},
                                                {LOGC_OPTION_PREFIX_FUNCTIONNAME,"Function Name"},
                                                {LOGC_OPTION_PREFIX_LOGTYPETEXT|LOGC_OPTION_PREFIX_FILEINFO,"Logtypetext + Fileinfo"},
                                                {LOGC_OPTION_PREFIX_LOGTYPETEXT|LOGC_OPTION_PREFIX_FUNCTIONNAME,"Logtypetext + Function Name"},
                                                {LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_FUNCTIONNAME,"Fileinfo + Function Name"},
                                                {LOGC_OPTION_PREFIX_LOGTYPETEXT|LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_FUNCTIONNAME,"Logtypetext + Fileinfo + Function Name"},
                                                {LOGC_OPTION_PREFIX_LOGTYPETEXT|LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_FUNCTIONNAME|
                                                 LOGC_OPTION_PREFIX_TIMESTAMP_DATE|LOGC_OPTION_PREFIX_TIMESTAMP_TIME|LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS,
                                                 "Date + Time + Milliseconds + Logtypetext + Fileinfo + Function Name"}
                                               };

void LogCTest_AddTestEntrys_m(LogC log,
                              const char *pcInfo)
{
  LOGC_TEST_TRACE("Adding Test entries...");
  LOGC_TEST_TEXT(log,LOGC_INFO,"---Adding Logentries: %s---",pcInfo);
  LOGC_TEST_TEXT(log,LOGC_DEBUG_MORE,"1st entry...");
  LOGC_TEST_TEXT(log,LOGC_DEBUG,"another entry");
  LOGC_TEST_TEXT(log,LOGC_ERROR,"And %s arguments! time() is: %"PRIi64"","variadic",(long long)time(NULL));
  LOGC_TEST_TEXT(log,LOGC_FATAL,"Also Test a very long string, 123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
}


int main(void)
{
  unsigned int index;
  unsigned int indexB;
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  LogCFile logFile;
  logFile.pcFilePath=LOGFILE_PATH;
#endif
  LOGC_TEST_TRACE("Creating Log-Object...");
  if(!(logCTest_m=LogC_New(LOGC_ALL,
                           150,
#ifdef LOGC_TEST_THREADS
                           LOGC_OPTION_THREADSAFE
#else /* !LOGC_TEST_THREADS */
                           0
#endif /* LOGC_TEST_THREADS */
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  #ifdef LOGFILE_PATH
                           ,&logFile
  #else
                           ,NULL
  #endif
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  #ifdef LOG_MAX_STORAGE_COUNT
                           ,LOG_MAX_STORAGE_COUNT
  #else
                           ,0
  #endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE*/
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
                           )))
  {
    puts("LogC_New_g() failed!");
    return(EXIT_FAILURE);
  }
  /* Add entries going through all preset options and prefix formats */
  for(index=0;index<sizeof(logOptions_m)/sizeof(TagLogOptions);++index)
  {
    printf("*****Setting LogOptions to: 0x%x (%s)*****\n",logOptions_m[index].options,logOptions_m[index].description);
    if(LogC_SetLogOptions(logCTest_m,logOptions_m[index].options))
    {
      LOGC_TEST_TRACE("LogC_SetLogOptions() failed");
      LOGTEST_EXIT_FAILURE(logCTest_m);
    }
    for(indexB=0;indexB<sizeof(logPrefixFormat_m)/sizeof(TagLogOptions);++indexB)
    {
      printf("*****Setting Prefix Format to: 0x%X (%s)*****\n",logPrefixFormat_m[indexB].options,logPrefixFormat_m[indexB].description);
      if(LogC_SetPrefixFormat(logCTest_m,logPrefixFormat_m[indexB].options))
      {
        LOGC_TEST_TRACE("LogC_SetPrefixFormat() failed");
        LOGTEST_EXIT_FAILURE(logCTest_m);
      }
      LogCTest_AddTestEntrys_m(logCTest_m,logPrefixFormat_m[indexB].description);
    }
  }

#ifdef LOGFILE_PATH
  if(LogTest_File_g(logCTest_m))
  {
    LOGC_TEST_TRACE("iLogTest_File_g() failed");
    LOGTEST_EXIT_FAILURE(logCTest_m);
  }
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
  if(LogTest_Storage_g(logCTest_m))
  {
    LOGC_TEST_TRACE("iLogTest_Storage_g() failed");
    LOGTEST_EXIT_FAILURE(logCTest_m);
  }
#endif /* LOG_MAX_STORAGE_COUNT */

#ifdef LOGC_TEST_THREADS
  LOGC_TEST_TRACE("Changing Prefix Format to (LOGC_OPTION_PREFIX_TIMESTAMP_TIME|LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS)...");
  if(LogC_SetPrefixFormat(logCTest_m,
                          logPrefixFormat_m[6].options))
  {
    LOGC_TEST_TRACE("iLogC_SetPrefixOptions_g() failed");
    LOGTEST_EXIT_FAILURE(logCTest_m);
  }
  if(LogTest_Threads_m(logCTest_m))
  {
    LOGC_TEST_TRACE("iLogTest_Threads_g() failed");
    LOGTEST_EXIT_FAILURE(logCTest_m);
  }
#endif /* LOGC_TEST_THREADS */

  LOGC_TEST_TRACE("Ending log...");
  if(LogC_End(logCTest_m))
  {
    printf("iLogC_End_g() failed\n");
    return(EXIT_FAILURE);
  }
  LOGC_TEST_TRACE("Tests passed");
  return(EXIT_SUCCESS);
}

#ifdef LOGFILE_PATH
int LogTest_File_g(LogC log)
{
  LOGC_TEST_TRACE("Testing: LOGC_FEATURE_ENABLE_LOGFILE");
  if(LogC_SetPrefixFormat(log,logPrefixFormat_m[4].options))
  {
    puts("LogC_SetPrefixFormatOptions() failed");
    return(-1);
  }
  LOGC_TEST_TRACE("Changing logfilepath from \"" LOGFILE_PATH "\" to \"" LOGFILE_PATH2 "\"...");
  if(LogC_SetFilePath(log,LOGFILE_PATH2))
  {
    printf("iLogC_SetFilePath_g() failed\n");
    return(-1);
  }
  LogCTest_AddTestEntrys_m(log,"Testing LogFile...");
  LOGC_TEST_TRACE("Disabling logfile...");
  if(LogC_SetFilePath(log,NULL))
  {
    printf("iLogC_SetFilePath_g() failed\n");
    return(-1);
  }
  return(0);
}
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
int LogTest_Storage_g(LogC log)
{
  size_t szLength;
  int iLogsCount=0;
  char *pcLogText;
  LOGC_TEST_TRACE("Testing: LOGC_FEATURE_ENABLE_LOG_STORAGE");
  LOGC_TEST_TRACE("Get all stored logs and print them...");
  while((pcLogText=LogC_StorageGetNextLog(log,&szLength)))
  {
    ++iLogsCount;
    printf("Next LogText: \"%s\"\n",pcLogText);
    free(pcLogText);
  }
  if(iLogsCount>LOG_MAX_STORAGE_COUNT)
  {
    printf("Error in log-storage, max. allowed: %d, is: %d\n",LOG_MAX_STORAGE_COUNT,iLogsCount);
    return(-1);
  }
  return(0);
}
#endif /* LOG_MAX_STORAGE_COUNT */

#if defined(LOGC_TEST_THREADS)
TThreadReturn LogTest_ThreadFunc_m(void *pvArgs)
{
#if (LOGC_TEST_THREADS==1)
  unsigned long ulThreadNr=GetCurrentThreadId();
#elif (LOGC_TEST_THREADS==2)
  unsigned long ulThreadNr=(unsigned long)pthread_self();
#endif
  int iIndex;
  LogC ptagLog=pvArgs;

  while(!iLogTest_ThreadsStart_m); /* Wait for all threads to start before continue */

  printf("Thread %lu GO!\n",ulThreadNr);
  for(iIndex=0;iIndex<LOGC_TEST_THREADS_ENTRIES;++iIndex)
  {
    LOGC_TEST_TEXT(ptagLog,LOGC_INFO,"Hello No.%d from Thread No.%lu",iIndex,ulThreadNr);
  }
  printf("thread %lu done\n",ulThreadNr);
  return((TThreadReturn)0);
}
#endif

#if defined(LOGC_TEST_THREADS) && (LOGC_TEST_THREADS==1) /* Winthreads */
int LogTest_Threads_m(LogC log)
{
  HANDLE taThreadHandles[LOGC_TEST_THREADS_COUNT];
  unsigned int uiIndex, uiIndexB;
  iLogTest_ThreadsStart_m=0;

  LOGC_TEST_TRACE("Testing: LOGC_FEATURE_ENABLE_THREADSAFETY");

  for(uiIndex=0;uiIndex<sizeof(taThreadHandles)/sizeof(HANDLE);++uiIndex)
  {
    taThreadHandles[uiIndex]=(HANDLE)_beginthreadex(NULL,
                                                    0,
                                                    LogTest_ThreadFunc_m,
                                                    log,
                                                    0,
                                                    NULL);
    assert(taThreadHandles[uiIndex]);
  }
  iLogTest_ThreadsStart_m=1;
  for(uiIndexB=0;uiIndexB<uiIndex;++uiIndexB)
  {
    WaitForSingleObject(taThreadHandles[uiIndexB],INFINITE);
    CloseHandle(taThreadHandles[uiIndexB]);
  }
  return((uiIndex==LOGC_TEST_THREADS_COUNT)?0:-1);
}

#elif defined(LOGC_TEST_THREADS) && (LOGC_TEST_THREADS==2) /* POSIX-Threads */
int LogTest_Threads_m(LogC log)
{
  pthread_t taThreadHandles[LOGC_TEST_THREADS_COUNT];
  unsigned int uiIndex, uiIndexB;
  iLogTest_ThreadsStart_m=0;

  LOGC_TEST_TRACE("Testing: LOGC_FEATURE_ENABLE_THREADSAFETY");

  for(uiIndex=0;uiIndex<sizeof(taThreadHandles)/sizeof(pthread_t);++uiIndex)
  {
    assert(!pthread_create(&taThreadHandles[uiIndex],
                           NULL,
                           LogTest_ThreadFunc_m,
                           log));
  }
  iLogTest_ThreadsStart_m=1;
  for(uiIndexB=0;uiIndexB<uiIndex;++uiIndexB)
  {
    pthread_join(taThreadHandles[uiIndexB],NULL);
  }
  return((uiIndex==LOGC_TEST_THREADS_COUNT)?0:-1);
}
#endif /* Test-Threads */



