#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "LogC.h"

#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  #define LOGC_TEST_THREADS
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */

#ifdef LOGC_TEST_THREADS
  #define LOGC_TEST_THREADS_COUNT 10
  #define LOGC_TEST_THREADS_ENTRIES 100
  int iLogTest_Threads_m(TagLog *ptagLog);
  int iLogTest_ThreadsStart_m;
  #ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN /* Avoid include of useless windows headers */
      #include <windows.h>
      #include <process.h>
    #undef LOGC_TEST_THREADS
    #undef WIN32_LEAN_AND_MEAN
    #define LOGC_TEST_THREADS 1
    typedef unsigned int tThreadReturn;
  #elif defined(__unix__)
    #include <unistd.h> /* For determining the current POSIX-Version */
    #if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L) /* POSIX IEEE 1003.1-2001 or newer */
      #include <pthread.h>
      #undef LOGC_TEST_THREADS
      #define LOGC_TEST_THREADS 2
      typedef void* tThreadReturn;
    #else
      #error POSIX Version too old, must be POSIX IEEE 1003.1-2001 or newer for pthreads.
    #endif /* POSIX Version */
  #else
    #error No Threading system implemented!
  #endif
#endif

#define LOGC_TEST_TEXT(log,logtype,...) assert(!LOG_TEXT(log,logtype,__VA_ARGS__))

#define LOGC_TEST_TRACE(txt) puts("----------\n" \
                                  "LogC_Test: " txt "\n" \
                                  "----------")

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  #define LOGFILE_PATH "F:\\Projects\\Libraries\\LibLog\\LibLogTest\\Test1.log"
  #define LOGFILE_PATH2 "F:\\Projects\\Libraries\\LibLog\\LibLogTest\\Test2.log"
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  #define LOG_MAX_STORAGE_COUNT 5
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */


#ifdef LOGFILE_PATH
  int iLogTest_File_g(TagLog *ptagLog);
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
  int iLogTest_Storage_g(TagLog *ptagLog);
#endif /* LOG_MAX_STORAGE_COUNT */

#define LOGTEST_EXIT_FAILURE(log) if(iLogC_End_g(log))LOGC_TEST_TRACE("LOGTEST_EXIT_FAILURE(): iLogC_End_g() failed!"); exit(EXIT_FAILURE)

TagLog *ptagLog_m;

void vLogTestFunc1_g(void)
{
  LOGC_TEST_TEXT(ptagLog_m,LOGC_DEBUG_MORE,"Test_Debug_More_123456789abcdefghijklmnopqrstuvwxyz: %d, %s\n",123,"one-two-three");
}

void vLogTestFunc2_g(void)
{
  LOGC_TEST_TEXT(ptagLog_m,LOGC_DEBUG,     "Test_Debug_123456789abcdefghijklmnopqrstuvwxyz\n");
}

void vLogTestFunc3_g(void)
{
  LOGC_TEST_TEXT(ptagLog_m,LOGC_INFO,      "Test_Info_123456789abcdefghijklmnopqrstuvwxyz");
}

int main(void)
{
#ifdef LOGFILE_PATH
  TagLogFile tagLogFile;
  tagLogFile.pcFilePath=LOGFILE_PATH;
  tagLogFile.szMaxFileQueueSize=20;
#endif /* LOGFILE_PATH */
  LOGC_TEST_TRACE("Creating Log-Object...");
  if(!(ptagLog_m=ptagLogC_New_g(LOGC_ALL,
                                128,
#ifdef LOGC_TEST_THREADS
                                LOGC_OPTION_THREADSAFE
#else /* !LOGC_TEST_THREADS */
                                0
#endif /* LOGC_TEST_THREADS */
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  #ifdef LOGFILE_PATH
                                ,&tagLogFile
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
    printf("ptagLogC_New_g() failed\n");
    return(EXIT_FAILURE);
  }
  LOGC_TEST_TRACE("Adding some entries...");
  LOGC_TEST_TEXT(ptagLog_m,LOGC_WARNING,   "Test_Warning_123456789abcdefghijklmnopqrstuvwxyz\n");
  LOGC_TEST_TEXT(ptagLog_m,LOGC_ERROR,     "Test_Error_123456789abcdefghijklmnopqrstuvwxyz");
  LOGC_TEST_TEXT(ptagLog_m,LOGC_FATAL,     "Test_Fatal_123456789abcdefghijklmnopqrstuvwxyz");

  LOGC_TEST_TRACE("Changing Log options to LOGC_PREFIX_FILEINFO...");
  if(iLogC_SetPrefixOptions_g(ptagLog_m,
                              LOGC_OPTION_PREFIX_FILEINFO))
  {
    LOGC_TEST_TRACE("iLogC_SetPrefixOptions_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }

  LOGC_TEST_TRACE("Adding some more entries...");
  vLogTestFunc1_g();
  vLogTestFunc2_g();
  vLogTestFunc3_g();

  LOGC_TEST_TRACE("Changing Prefix options to LOGC_PREFIX_TIMESTAMP...");
  if(iLogC_SetPrefixOptions_g(ptagLog_m,
                              LOGC_OPTION_PREFIX_TIMESTAMP))
  {
    LOGC_TEST_TRACE("iLogC_SetPrefixOptions_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }

  LOGC_TEST_TRACE("Adding some more entries...");
  LOGC_TEST_TEXT(ptagLog_m,LOGC_WARNING,   "Test_Warning_123456789abcdefghijklmnopqrstuvwxyz\n");
  LOGC_TEST_TEXT(ptagLog_m,LOGC_ERROR,     "Test_Error_123456789abcdefghijklmnopqrstuvwxyz");
  LOGC_TEST_TEXT(ptagLog_m,LOGC_FATAL,     "Test_Fatal_123456789abcdefghijklmnopqrstuvwxyz");

  LOGC_TEST_TRACE("Changing Prefix options to (LOGC_PREFIX_FILEINFO|LOGC_PREFIX_TIMESTAMP)...");
  if(iLogC_SetPrefixOptions_g(ptagLog_m,
                           (LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_TIMESTAMP)))
  {
    LOGC_TEST_TRACE("iLogC_SetPrefixOptions_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }

  LOGC_TEST_TRACE("Adding some more entries...");
  vLogTestFunc1_g();
  vLogTestFunc2_g();
  vLogTestFunc3_g();

#ifdef LOGFILE_PATH
  if(iLogTest_File_g(ptagLog_m))
  {
    LOGC_TEST_TRACE("iLogTest_File_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
  if(iLogTest_Storage_g(ptagLog_m))
  {
    LOGC_TEST_TRACE("iLogTest_Storage_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }
#endif /* LOG_MAX_STORAGE_COUNT */

  LOGC_TEST_TRACE("Changing Prefix options to LOGC_PREFIX_TIMESTAMP...");
  if(iLogC_SetPrefixOptions_g(ptagLog_m,
                           LOGC_OPTION_PREFIX_TIMESTAMP))
  {
    LOGC_TEST_TRACE("iLogC_SetPrefixOptions_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }
#ifdef LOGC_TEST_THREADS
  if(iLogTest_Threads_m(ptagLog_m))
  {
    LOGC_TEST_TRACE("iLogTest_Threads_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }
#endif /* LOGC_TEST_THREADS */

  LOGC_TEST_TRACE("Ending log...");
  if(iLogC_End_g(ptagLog_m))
  {
    printf("iLogC_End_g() failed\n");
    return(EXIT_FAILURE);
  }
  LOGC_TEST_TRACE("Tests passed");
  return(EXIT_SUCCESS);
}

#ifdef LOGFILE_PATH
int iLogTest_File_g(TagLog *ptagLog)
{
  LOGC_TEST_TRACE("Testing: LOGC_FEATURE_ENABLE_LOGFILE");
  LOGC_TEST_TRACE("Changing logfilepath from " LOGFILE_PATH " to " LOGFILE_PATH2 "...");
  if(iLogC_SetFilePath_g(ptagLog,LOGFILE_PATH2))
  {
    printf("iLogC_SetFilePath_g() failed\n");
    return(-1);
  }
  vLogTestFunc1_g();
  vLogTestFunc2_g();
  vLogTestFunc3_g();

  LOGC_TEST_TRACE("Disabling logfile...");
  if(iLogC_SetFilePath_g(ptagLog,NULL))
  {
    printf("iLogC_SetFilePath_g() failed\n");
    return(-1);
  }
  LOGC_TEST_TEXT(ptagLog,LOGC_WARNING,   "Test_Warning_123456789abcdefghijklmnopqrstuvwxyz\n");
  LOGC_TEST_TEXT(ptagLog,LOGC_ERROR,     "Test_Error_123456789abcdefghijklmnopqrstuvwxyz");
  LOGC_TEST_TEXT(ptagLog,LOGC_FATAL,     "Test_Fatal_123456789abcdefghijklmnopqrstuvwxyz");
  return(0);
}
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
int iLogTest_Storage_g(TagLog *ptagLog)
{
  size_t szLength;
  int iLogsCount=0;
  char *pcLogText;
  LOGC_TEST_TRACE("Testing: LOGC_FEATURE_ENABLE_LOG_STORAGE");
  LOGC_TEST_TRACE("Get all stored logs and print them...");
  while((pcLogText=pcLogC_StorageGetNextLog_g(ptagLog,&szLength)))
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
tThreadReturn LogTest_ThreadFunc_m(void *pvArgs)
{
#if (LOGC_TEST_THREADS==1)
  unsigned long ulThreadNr=GetCurrentThreadId();
#elif (LOGC_TEST_THREADS==2)
  unsigned long ulThreadNr=(unsigned long)pthread_self();
#endif
  int iIndex;
  TagLog *ptagLog=pvArgs;

  while(!iLogTest_ThreadsStart_m); /* Wait for all threads to start before continue */

  printf("Thread %lu GO!\n",ulThreadNr);
  for(iIndex=0;iIndex<LOGC_TEST_THREADS_ENTRIES;++iIndex)
  {
    LOGC_TEST_TEXT(ptagLog,LOGC_INFO,"Hello No.%d from Thread No.%lu",iIndex,ulThreadNr);
  }
  printf("thread %lu done\n",ulThreadNr);
  return((tThreadReturn)0);
}
#endif

#if defined(LOGC_TEST_THREADS) && (LOGC_TEST_THREADS==1) /* Winthreads */
int iLogTest_Threads_m(TagLog *ptagLog)
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
                                                    ptagLog,
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
int iLogTest_Threads_m(TagLog *ptagLog)
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
                           ptagLog));
  }
  iLogTest_ThreadsStart_m=1;
  for(uiIndexB=0;uiIndexB<uiIndex;++uiIndexB)
  {
    pthread_join(taThreadHandles[uiIndexB],NULL);
  }
  return((uiIndex==LOGC_TEST_THREADS_COUNT)?0:-1);
}
#endif /* Test-Threads */
