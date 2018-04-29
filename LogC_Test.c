#include <stdio.h>
#include <stdlib.h>

#include "LogC.h"

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  #define LOGFILE_PATH "Test1.log"
  #define LOGFILE_PATH2 "Test2.log"
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

#define LOGTEST_EXIT_FAILURE(log) if(iLogC_End_g(log))puts("LOGTEST_EXIT_FAILURE(): iLogC_End_g() failed!"); exit(EXIT_FAILURE)

TagLog *ptagLog_m;

void vLogTestFunc1_g(void)
{
  LOG_TEXT(ptagLog_m,LOGC_DEBUG_MORE,"Test_Debug_More_123456789abcdefghijklmnopqrstuvwxyz: %d, %s\n",123,"one-two-three");
}

void vLogTestFunc2_g(void)
{
  LOG_TEXT(ptagLog_m,LOGC_DEBUG,     "Test_Debug_123456789abcdefghijklmnopqrstuvwxyz\n");
}

void vLogTestFunc3_g(void)
{
  LOG_TEXT(ptagLog_m,LOGC_INFO,      "Test_Info_123456789abcdefghijklmnopqrs\ntuvwxyz");
}

int main(void)
{
#ifdef LOGFILE_PATH
  TagLogFile tagLogFile;
  tagLogFile.pcFilePath=LOGFILE_PATH;
  tagLogFile.szMaxFileQueueSize=20;
#endif /* LOGFILE_PATH */
  puts("Creating Log-Object...");
  if(!(ptagLog_m=ptagLogC_New_g(LOGC_ALL,
                                128,
                                0
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
  puts("Adding some entries...");
  LOG_TEXT(ptagLog_m,LOGC_WARNING,   "Test_Warning_123456789abcdefghijklmnopqrstuvwxyz\n");
  LOG_TEXT(ptagLog_m,LOGC_ERROR,     "Test_Error_123456789abcdefghijklmnopqrstuvwxyz");
  LOG_TEXT(ptagLog_m,LOGC_FATAL,     "Test_Fatal_123456789abcdefghijklmnopqrstuvwxyz");

  puts("Changing Prefix options to LOGC_PREFIX_FILEINFO...");
  vLogC_SetPrefixOptions_g(ptagLog_m,
                           LOGC_PREFIX_FILEINFO);

  puts("Adding some more entries...");
  vLogTestFunc1_g();
  vLogTestFunc2_g();
  vLogTestFunc3_g();

  puts("Changing Prefix options to LOGC_PREFIX_TIMESTAMP...");
  vLogC_SetPrefixOptions_g(ptagLog_m,
                           LOGC_PREFIX_TIMESTAMP);

  puts("Adding some more entries...");
  LOG_TEXT(ptagLog_m,LOGC_WARNING,   "Test_Warning_123456789abcdefghijklmnopqrstuvwxyz\n");
  LOG_TEXT(ptagLog_m,LOGC_ERROR,     "Test_Error_123456789abcdefghijklmnopqrstuvwxyz");
  LOG_TEXT(ptagLog_m,LOGC_FATAL,     "Test_Fatal_123456789abcdefghijklmnopqrstuvwxyz");

  puts("Changing Prefix options to (LOGC_PREFIX_FILEINFO|LOGC_PREFIX_TIMESTAMP)...");
  vLogC_SetPrefixOptions_g(ptagLog_m,
                           (LOGC_PREFIX_FILEINFO|LOGC_PREFIX_TIMESTAMP));

  puts("Adding some more entries...");
  vLogTestFunc1_g();
  vLogTestFunc2_g();
  vLogTestFunc3_g();

#ifdef LOGFILE_PATH
  if(iLogTest_File_g(ptagLog_m))
  {
    puts("iLogTest_File_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
  if(iLogTest_Storage_g(ptagLog_m))
  {
    puts("iLogTest_Storage_g() failed");
    LOGTEST_EXIT_FAILURE(ptagLog_m);
  }
#endif /* LOG_MAX_STORAGE_COUNT */

  puts("Ending log...");
  if(iLogC_End_g(ptagLog_m))
  {
    printf("iLogC_End_g() failed\n");
    return(EXIT_FAILURE);
  }
  puts("Tests passed");
  return(EXIT_SUCCESS);
}

#ifdef LOGFILE_PATH
int iLogTest_File_g(TagLog *ptagLog)
{
  puts("Testing: LOGC_FEATURE_ENABLE_LOGFILE");
  puts("Changing logfilepath from " LOGFILE_PATH " to " LOGFILE_PATH2 "...");
  if(iLogC_SetFilePath_g(ptagLog,LOGFILE_PATH2))
  {
    printf("iLogC_SetFilePath_g() failed\n");
    return(-1);
  }
  vLogTestFunc1_g();
  vLogTestFunc2_g();
  vLogTestFunc3_g();

  puts("Disabling logfile...");
  if(iLogC_SetFilePath_g(ptagLog,NULL))
  {
    printf("iLogC_SetFilePath_g() failed\n");
    return(-1);
  }
  LOG_TEXT(ptagLog,LOGC_WARNING,   "Test_Warning_123456789abcdefghijklmnopqrstuvwxyz\n");
  LOG_TEXT(ptagLog,LOGC_ERROR,     "Test_Error_123456789abcdefghijklmnopqrstuvwxyz");
  LOG_TEXT(ptagLog,LOGC_FATAL,     "Test_Fatal_123456789abcdefghijklmnopqrstuvwxyz");
  return(0);
}
#endif /* LOGFILE_PATH */

#ifdef LOG_MAX_STORAGE_COUNT
int iLogTest_Storage_g(TagLog *ptagLog)
{
  size_t szLength;
  int iLogsCount=0;
  char *pcLogText;
  puts("Testing: LOGC_FEATURE_ENABLE_LOG_STORAGE");
  puts("Get all stored logs and print them...");
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
