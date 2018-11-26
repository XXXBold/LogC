#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <limits.h>
#include "LogC.h"

#ifndef LOGC_LIBRARY_DEBUG
  #define NDEBUG
#endif /* LOGC_LIBRARY_DEBUG */
#include <assert.h>

#define LOGC_TIMESTAMP_FORMAT_DATE     "%Y-%m-%d"
#define LOGC_TIMESTAMP_FORMAT_TIME     "%H:%M:%S"
#define LOGC_TIMESTAMP_FORMAT_DATETIME LOGC_TIMESTAMP_FORMAT_DATE "_" LOGC_TIMESTAMP_FORMAT_TIME /* e.g. 2018-10-03_12:34:56, see strftime() function */
#define LOGC_PREFIX_FORMAT_FILEINFO    "[%s]\"%s\"@line %d in function \"%s()\":"  /* e.g. [Error]"myfile.c"@line 123 in function "func()": */
#define LOGC_PREFIX_FORMAT_NO_FILEINFO "[%s]in function \"%s()\":"                 /* e.g. [Error]in function "func()": */

#define LOGC_TEXT_NO_FUNCTION "???"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN /* Avoid include of useless windows headers */
    #include <windows.h>
  #undef WIN32_LEAN_AND_MEAN
  #ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
    typedef CRITICAL_SECTION TMutex;
    #define LOGC_MUTEX_INIT(log)    do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) InitializeCriticalSection(&(log)->tMutex); }while(0)
    #define LOGC_MUTEX_DESTROY(log) do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) DeleteCriticalSection(&(log)->tMutex);     }while(0)
    #define LOGC_MUTEX_LOCK(log)    do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) EnterCriticalSection(&(log)->tMutex);      }while(0)
    #define LOGC_MUTEX_UNLOCK(log)  do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) LeaveCriticalSection(&(log)->tMutex);      }while(0)
  #endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */ffff
//  #if (_WIN32_WINNT >= 0x602) /* Check if Windows 8 or newer */
//    #define GET_SYSTIME_AS_FILETIME(pFileTime) GetSystemTimePreciseAsFileTime(pFileTime)
//  #else
//    #define GET_SYSTIME_AS_FILETIME(pFileTime) GetSystemTimeAsFileTime(pFileTime)
//  #endif */
#elif defined(__unix__)
  #include <unistd.h> /* For determining the current POSIX-Version, etc. */
  #if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L) /* POSIX IEEE 1003.1-2001 or newer */
    #ifndef _POSIX_TIMERS
      #error POSIX_TIMERS not available!
    #endif /* POSIX_TIMERS */

    #ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
      #include <pthread.h>
      typedef pthread_mutex_t TMutex;
      #define LOGC_MUTEX_INIT(log)    do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) assert(!pthread_mutex_init(&(log)->tMutex,NULL)); }while(0)
      #define LOGC_MUTEX_DESTROY(log) do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) assert(!pthread_mutex_destroy(&(log)->tMutex));   }while(0)
      #define LOGC_MUTEX_LOCK(log)    do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) assert(!pthread_mutex_lock(&(log)->tMutex));      }while(0)
      #define LOGC_MUTEX_UNLOCK(log)  do{ if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) assert(!pthread_mutex_unlock(&(log)->tMutex));    }while(0)
    #endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
  #else
    #error POSIX Version too old, must be POSIX IEEE 1003.1-2001 or newer.
  #endif
#else
  #error Unknown Platform!
#endif /* _WIN32 */

#ifndef LOGC_FEATURE_ENABLE_THREADSAFETY
  #define LOGC_MUTEX_INIT(log)
  #define LOGC_MUTEX_DESTROY(log)
  #define LOGC_MUTEX_LOCK(log)
  #define LOGC_MUTEX_UNLOCK(log)
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */

#define BITS_UNSET(val,bitmap) (val&(~(bitmap)))
#define LOGC_OPTION_ENABLED(log,option) (((log)->uiLogOptions&(option))==(option))

#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* >= C99 */
  #define INLINE_FCT inline
  #define INLINE_PROT static inline
#else /* No inline available from C Standard */
  #define INLINE_FCT
  #define INLINE_PROT
#endif /* __STDC_VERSION__ >= C99 */

#define LOGC_PATH_MAXLEN           260 /* Should be enough for any usual cases */
#define LOGC_DEFAULT_FILEQUEUESIZE 4

typedef enum
{
  LOGC_STDOUT,
  LOGC_STDERR
}ELogCOutStreams;

static const struct TagLogType
{
  int iLogType;
  ELogCOutStreams eOutStream;
  const char *pcText;
}tagLogTypes[]=
{
  {LOGC_DEBUG_MORE, LOGC_STDOUT, "Debug++"},
  {LOGC_DEBUG,      LOGC_STDOUT, "Debug"},
  {LOGC_INFO,       LOGC_STDOUT, "Info"},
  {LOGC_WARNING,    LOGC_STDERR, "Warning"},
  {LOGC_ERROR,      LOGC_STDERR, "Error"},
  {LOGC_FATAL,      LOGC_STDERR, "Fatal"},
};

#if defined(LOGC_FEATURE_ENABLE_LOG_STORAGE)
struct TagLogCEntry_t
{
  size_t szTextLength;
  char *pcText;
  struct TagLogCEntry_t *ptagNext;
};
typedef struct TagLogCEntry_t TagLogCEntry;
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

struct TagLog_t
{
  int iLogLevel;
  unsigned int uiLogOptions;
  size_t szMaxEntryLength;
  char *pcTextBuffer;
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  size_t szLogFileQueueCount;
  char *pcLogFileQueueBuffer;
  char caLogPath[LOGC_PATH_MAXLEN];
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  size_t szMaxStorageCount;
  size_t szStoredLogsCount;
  TagLogCEntry *ptagSavedLogFirst;
  TagLogCEntry *ptagSavedLogLast;
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  TMutex tMutex;
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
};

INLINE_PROT int iLogC_vsnprintf(char *pcBuffer,
                                size_t szBufferSize,
                                const char *pcFormat,
                                va_list vaArgs);

INLINE_PROT int iLogC_AddTimeStamp_m(TagLog *ptagLog,
                                     size_t *pszBufferPos);
INLINE_PROT int iLogC_SetPrefixOptions_m(TagLog *ptagLog,
                                         unsigned int uiOptions);
INLINE_PROT const struct TagLogType *ptagLogC_GetLogType_m(int iLogType);

/* Functions if logfile is enabled */
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
INLINE_PROT int iLogC_WriteEntriesToDisk_m(TagLog *ptagLog);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
INLINE_PROT void vLogC_StoragePush_m(TagLog *ptagLog,
                                     TagLogCEntry *ptagEntry);

INLINE_PROT char *pcLogC_StoragePop_m(TagLog *ptagLog,
                                      size_t *pszEntryLength);
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

TagLog *LogC_New(int iLogLevel,
                 size_t szMaxEntryLength,
                 unsigned int uiLogOptions
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
                 ,TagLogFile *ptagLogFile
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
                 ,size_t szMaxStorageCount
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
                 )
{
  TagLog *ptagNewLog;
  size_t szNewLogSize=0;

  if(szMaxEntryLength<sizeof(LOGC_TIMESTAMP_FORMAT_DATETIME)+sizeof(LOGC_PREFIX_FORMAT_FILEINFO)+10)
    return(NULL);
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  if(ptagLogFile)
  {
    if(!ptagLogFile->pcFilePath)
      return(NULL);

    if(strlen(ptagLogFile->pcFilePath)>LOGC_PATH_MAXLEN-1)
      return(NULL);
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

  szNewLogSize=szMaxEntryLength+2; /* +2 for '\n'+'\0' */
  /* Check for Overflow of size_t */
  if(szNewLogSize+sizeof(TagLog)<szMaxEntryLength)
    return(NULL);
  if(!(ptagNewLog=malloc(szNewLogSize+sizeof(TagLog))))
    return(NULL);

  ptagNewLog->uiLogOptions=0;
  if(iLogC_SetPrefixOptions_m(ptagNewLog,
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
                              BITS_UNSET(uiLogOptions,LOGC_OPTION_THREADSAFE)
#else
                              uiLogOptions
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
                              ))
  {
    free(ptagNewLog);
    return(NULL);
  }
  ptagNewLog->pcTextBuffer=((char*)ptagNewLog)+sizeof(TagLog);
  ptagNewLog->szMaxEntryLength=szMaxEntryLength;
  ptagNewLog->iLogLevel=iLogLevel;

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  if(ptagLogFile)
  {
    if(!(ptagNewLog->pcLogFileQueueBuffer=malloc(LOGC_DEFAULT_FILEQUEUESIZE*(szMaxEntryLength+2))))
    {
      free(ptagNewLog);
      return(NULL);
    }
    strcpy(ptagNewLog->caLogPath,ptagLogFile->pcFilePath);
  }
  else
  {
    ptagNewLog->pcLogFileQueueBuffer=NULL;
  }
  ptagNewLog->szLogFileQueueCount=0;
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  ptagNewLog->szMaxStorageCount=szMaxStorageCount;
  ptagNewLog->szStoredLogsCount=0;
  ptagNewLog->ptagSavedLogFirst=NULL;
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  ptagNewLog->uiLogOptions|=(uiLogOptions&LOGC_OPTION_THREADSAFE);
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
  LOGC_MUTEX_INIT(ptagNewLog);
  return(ptagNewLog);
}

int LogC_End(TagLog *ptagLog)
{
  LOGC_MUTEX_LOCK(ptagLog);
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  /* Check if there are entries to be written */
  if((ptagLog->pcLogFileQueueBuffer) && (ptagLog->szLogFileQueueCount))
  {
    if(iLogC_WriteEntriesToDisk_m(ptagLog))
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
    free(ptagLog->pcLogFileQueueBuffer);
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  while(ptagLog->szStoredLogsCount)
  {
    free(pcLogC_StoragePop_m(ptagLog,NULL));
  }
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  LOGC_MUTEX_UNLOCK(ptagLog);
  LOGC_MUTEX_DESTROY(ptagLog);
  free(ptagLog);
  return(0);
}

int LogC_SetPrefixOptions(TagLog *ptagLog,
                          unsigned int uiNewPrefixOptions)
{
  int iRc;
  LOGC_MUTEX_LOCK(ptagLog);
  iRc=iLogC_SetPrefixOptions_m(ptagLog,uiNewPrefixOptions);
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(iRc);
}

int LogC_AddEntry_Text(TagLog *ptagLog,
                       int iLogType,
                       const char *pcFileName,
                       int iLineNr,
                       const char *pcFunction,
                       const char *pcLogText,
                       ...)
{
  int iRc;
  size_t szCurrBufferPos=0;
  va_list vaArgs;
  const struct TagLogType *ptagCurrLogType;

  if(iLogType<ptagLog->iLogLevel)
    return(0);
  if(!(ptagCurrLogType=ptagLogC_GetLogType_m(iLogType)))
    return(-1);

  LOGC_MUTEX_LOCK(ptagLog);
  /* Add Timestamp, if needed */
  if(iLogC_AddTimeStamp_m(ptagLog,&szCurrBufferPos))
  {
    LOGC_MUTEX_UNLOCK(ptagLog);
    return(-1);
  }
  /* Check if Fileinfo is needed */
  if((ptagLog->uiLogOptions&LOGC_OPTION_PREFIX_FILEINFO))
  {
    if(sizeof(LOGC_PREFIX_FORMAT_FILEINFO)+((pcFunction)?strlen(pcFunction):sizeof(LOGC_TEXT_NO_FUNCTION))+strlen(pcFileName)+3>ptagLog->szMaxEntryLength-szCurrBufferPos)
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
    iRc=sprintf(&ptagLog->pcTextBuffer[szCurrBufferPos],
                LOGC_PREFIX_FORMAT_FILEINFO,
                ptagCurrLogType->pcText,
                pcFileName,
                iLineNr,
                (pcFunction)?pcFunction:LOGC_TEXT_NO_FUNCTION);
  }
  else
  {
    if(sizeof(LOGC_PREFIX_FORMAT_NO_FILEINFO)+((pcFunction)?strlen(pcFunction):sizeof(LOGC_TEXT_NO_FUNCTION))+3>ptagLog->szMaxEntryLength-szCurrBufferPos)
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
    iRc=sprintf(&ptagLog->pcTextBuffer[szCurrBufferPos],
                LOGC_PREFIX_FORMAT_NO_FILEINFO,
                ptagCurrLogType->pcText,
                (pcFunction)?pcFunction:LOGC_TEXT_NO_FUNCTION);
  }
  if(iRc<1)
  {
    LOGC_MUTEX_UNLOCK(ptagLog);
    return(-1);
  }

  szCurrBufferPos+=iRc;
  errno=0;
  va_start(vaArgs,pcLogText);
  iRc=iLogC_vsnprintf(&ptagLog->pcTextBuffer[szCurrBufferPos],
                     ptagLog->szMaxEntryLength+1-szCurrBufferPos, /* +1 is okay, we have 2 more bytes reserved then szMaxEntryLength */
                     pcLogText,
                     vaArgs);
  va_end(vaArgs);
  if(errno==EINVAL)
  {
    LOGC_MUTEX_UNLOCK(ptagLog);
    return(-1);
  }


  /* Check for truncation */
  if(iRc<1)
  {
    szCurrBufferPos=ptagLog->szMaxEntryLength;
    ptagLog->pcTextBuffer[szCurrBufferPos]='\n';
    ptagLog->pcTextBuffer[++szCurrBufferPos]='\0';
  }
  else
  {
    szCurrBufferPos+=iRc;
    /* Check if there's already a newline at the end */
    if(ptagLog->pcTextBuffer[szCurrBufferPos-1]!='\n')
    {
      ptagLog->pcTextBuffer[szCurrBufferPos]='\n';
      ptagLog->pcTextBuffer[++szCurrBufferPos]='\0';
    }
  }

  switch(ptagCurrLogType->eOutStream)
  {
    case LOGC_STDOUT:
      fputs(ptagLog->pcTextBuffer,stdout);
      break;
    case LOGC_STDERR:
      fputs(ptagLog->pcTextBuffer,stderr);
      break;
    default:
      break;
  }
  ++szCurrBufferPos;
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  /* Add entry to filequeue, if needed */
  if(ptagLog->pcLogFileQueueBuffer)
  {
    assert(ptagLog->szLogFileQueueCount<LOGC_DEFAULT_FILEQUEUESIZE);
    memcpy(&ptagLog->pcLogFileQueueBuffer[ptagLog->szLogFileQueueCount*(ptagLog->szMaxEntryLength+2)],
           ptagLog->pcTextBuffer,
           szCurrBufferPos);
    ++ptagLog->szLogFileQueueCount;
    if(ptagLog->szLogFileQueueCount==LOGC_DEFAULT_FILEQUEUESIZE)
    {
      if(iLogC_WriteEntriesToDisk_m(ptagLog))
      {
        LOGC_MUTEX_UNLOCK(ptagLog);
        return(-1);
      }
    }
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  if(ptagLog->szMaxStorageCount)
  {
    char *pcTmp;
    if(!(pcTmp=malloc(sizeof(TagLogCEntry)+szCurrBufferPos)))
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
    ((TagLogCEntry*)(pcTmp+szCurrBufferPos))->pcText=pcTmp;
    ((TagLogCEntry*)(pcTmp+szCurrBufferPos))->szTextLength=szCurrBufferPos;
    memcpy(pcTmp,ptagLog->pcTextBuffer,szCurrBufferPos);
    vLogC_StoragePush_m(ptagLog,((TagLogCEntry*)(pcTmp+szCurrBufferPos)));
  }
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(0);
}

INLINE_FCT int iLogC_vsnprintf(char *pcBuffer,
                               size_t szBufferSize,
                               const char *pcFormat,
                               va_list vaArgs)
{
  int iRc;
/* For vsnprintf C99 is needed, or MSC >=1900(VS 15 or higher) */
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))
  #ifdef _WIN32 /* Win32 _vsnprintf() is a bit diffrent from STDC... */
    if((iRc=_vsnprintf(pcBuffer,szBufferSize-1,pcFormat,vaArgs))<1)
      pcBuffer[szBufferSize-1]='\0';
    return(iRc);
  #else /* vsnprintf() which should be conformant to STDC */
    if((iRc=vsnprintf(pcBuffer,szBufferSize,pcFormat,vaArgs))>=(int)szBufferSize)
      return(-1);
    return(iRc);
  #endif
#else /* Terminate compilation with an Error, if no vsnprintf is available */
  #error No vsnprintf or equivalent available!
  /* Use of !UNSAFE! ANSI-C vsprintf() will work, but can cause buffer overflow!
     Use at own risk! Or better implement a custom function behaving like vsnprintf(). */
  return(vsprintf(pcBuffer,pcFormat,vaArgs));
#endif
}

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
int LogC_SetFilePath(TagLog *ptagLog,
                     const char *pcNewPath)
{
  size_t szPathLength=0;
  if(pcNewPath)
  {
    szPathLength=strlen(pcNewPath)+1;
    if(szPathLength>LOGC_PATH_MAXLEN)
      return(-1);
  }
  LOGC_MUTEX_LOCK(ptagLog);
  /* Write queue to old file first, if needed */
  if((ptagLog->pcLogFileQueueBuffer) && (ptagLog->szLogFileQueueCount))
    if(iLogC_WriteEntriesToDisk_m(ptagLog))
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
  if(pcNewPath) /* If new path is set, copy anyway */
  {
    memcpy(ptagLog->caLogPath,pcNewPath,szPathLength);
    if(!ptagLog->pcLogFileQueueBuffer)
      if(!(ptagLog->pcLogFileQueueBuffer=malloc(LOGC_DEFAULT_FILEQUEUESIZE*(ptagLog->szMaxEntryLength+2))))
      {
        LOGC_MUTEX_UNLOCK(ptagLog);
        return(-1);
      }
  }
  else if(ptagLog->pcLogFileQueueBuffer) /* Path was set before, but is not needed anymore */
  {
    free(ptagLog->pcLogFileQueueBuffer);
    ptagLog->pcLogFileQueueBuffer=NULL;
  }
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(0);
}

int LogC_WriteEntriesToDisk(TagLog *ptagLog)
{
  int iRc;
  LOGC_MUTEX_LOCK(ptagLog);
  if((!ptagLog->pcLogFileQueueBuffer) || (!ptagLog->szLogFileQueueCount))
    iRc=0;
  else
    iRc=iLogC_WriteEntriesToDisk_m(ptagLog);
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(iRc);
}

INLINE_FCT int iLogC_WriteEntriesToDisk_m(TagLog *ptagLog)
{
  FILE *fp;
  size_t szIndex;

  if(!(fp=fopen(ptagLog->caLogPath,"a")))
  {
    perror("Failed to Open LogFile: ");
    return(-1);
  }
  for(szIndex=0;szIndex<ptagLog->szLogFileQueueCount;++szIndex)
  {
    fputs(&ptagLog->pcLogFileQueueBuffer[szIndex*(ptagLog->szMaxEntryLength+2)],fp);
  }
  ptagLog->szLogFileQueueCount=0;
  fclose(fp);
  return(0);
}
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
char *LogC_StorageGetNextLog(TagLog *ptagLog,
                             size_t *pszEntryLength)
{
  char *pcTmp;
  LOGC_MUTEX_LOCK(ptagLog);
  pcTmp=pcLogC_StoragePop_m(ptagLog,pszEntryLength);
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(pcTmp);
}

INLINE_FCT void vLogC_StoragePush_m(TagLog *ptagLog,
                                    TagLogCEntry *ptagEntry)
{
  ptagEntry->ptagNext=NULL;
  /* Check if first entry in queue */
  if(!ptagLog->ptagSavedLogFirst)
  {
    ptagLog->ptagSavedLogFirst=ptagEntry;
    ptagLog->ptagSavedLogLast=ptagEntry;
    ptagLog->szStoredLogsCount=1;
    return;
  }
  ptagLog->ptagSavedLogLast->ptagNext=ptagEntry;
  ptagLog->ptagSavedLogLast=ptagEntry;
  if(ptagLog->szStoredLogsCount>=ptagLog->szMaxStorageCount)
  {
    TagLogCEntry *ptagTmp;
    ptagTmp=ptagLog->ptagSavedLogFirst;
    ptagLog->ptagSavedLogFirst=ptagLog->ptagSavedLogFirst->ptagNext;
    free(ptagTmp->pcText);
  }
  else
    ++ptagLog->szStoredLogsCount;
  return;
}

INLINE_FCT char *pcLogC_StoragePop_m(TagLog *ptagLog,
                                     size_t *pszEntryLength)
{
  TagLogCEntry *ptagTmp;

  if(!ptagLog->ptagSavedLogFirst)
    return(NULL);

  ptagTmp=ptagLog->ptagSavedLogFirst;
  ptagLog->ptagSavedLogFirst=ptagLog->ptagSavedLogFirst->ptagNext;
  if(pszEntryLength)
    *pszEntryLength=ptagTmp->szTextLength;
  --ptagLog->szStoredLogsCount;
  return(ptagTmp->pcText);
}
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

INLINE_FCT const struct TagLogType *ptagLogC_GetLogType_m(int iLogType)
{
  size_t szIndex;
  for(szIndex=0;szIndex<sizeof(tagLogTypes)/sizeof(struct TagLogType);++szIndex)
  {
    if(tagLogTypes[szIndex].iLogType==iLogType)
      return(&tagLogTypes[szIndex]);
  }
  return(NULL);
}

INLINE_FCT int iLogC_AddTimeStamp_m(TagLog *ptagLog, size_t *pszBufferPos)
{
  if(   LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_DATE)
     || LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME))
  {
    time_t tTime=time(NULL);
#if defined(LOGC_FEATURE_ENABLE_THREADSAFETY) && !defined(_WIN32) /* Check if threadsafe implementation is needed (WIN32 localtime() is threadsafe anyway) */
    struct tm tagTime;
  #if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L) /* POSIX IEEE 1003.1-2001 or newer */
    if(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_UTC))
      gmtime_r(&tTime,&tagTime);
    else if(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME))
      localtime_r(&tTime,&tagTime);
    else
      return(-1);
  #else /* No POSIX nor Win32 */
    #error No threadsafe localtime() available!
  #endif /* _POSIX_VERSION >= 200112L */
#else /* !LOGC_FEATURE_ENABLE_THREADSAFETY or WIN32 */
    struct tm *ptagTime;
    if(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_UTC))
      ptagTime=gmtime(&tTime);
    else if(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME))
      ptagTime=localtime(&tTime);
    else
      return(-1);
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY && !WIN32 */

    if(!(*pszBufferPos+=strftime(ptagLog->pcTextBuffer,
                                 ptagLog->szMaxEntryLength,
                                 (LOGC_OPTION_ENABLED(ptagLog,(LOGC_OPTION_PREFIX_TIMESTAMP_DATE|LOGC_OPTION_PREFIX_TIMESTAMP_TIME)))?
                                 LOGC_TIMESTAMP_FORMAT_DATETIME " ":(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_DATE))?
                                 LOGC_TIMESTAMP_FORMAT_DATE " ":LOGC_TIMESTAMP_FORMAT_TIME " ",
#if defined(LOGC_FEATURE_ENABLE_THREADSAFETY) && !defined(_WIN32)
                                 &tagTime
#else /* !LOGC_FEATURE_ENABLE_THREADSAFETY or WIN32 */
                                 ptagTime
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY && !WIN32 */
                                 )))
    {
      return(-1);
    }
  }
  if(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS))
  {
    if(LOGC_OPTION_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME)) /* Overwrite Space if Time is enabled */
      --(*pszBufferPos);
#if defined _WIN32
//    FILETIME tagCurrFileTime;
    SYSTEMTIME tCurrSysTime;
//    GET_SYSTIME_AS_FILETIME(&tagCurrFileTime);
    GetSystemTime(&tCurrSysTime);
//    FileTimeToSystemTime(&tagCurrFileTime,&tCurrSysTime);
    sprintf(&ptagLog->pcTextBuffer[*pszBufferPos],".%.3u ",tCurrSysTime.wMilliseconds);
#elif defined (__unix__) /* Checked above for availibility */
    struct timespec tagTime;
    clock_gettime(CLOCK_REALTIME,&tagTime);
    sprintf(&ptagLog->pcTextBuffer[*pszBufferPos],".%.3ld ",tagTime.tv_nsec/1000000);
#endif /* _WIN32 */
    *pszBufferPos+=5;
  }
  return(0);
}

INLINE_FCT int iLogC_SetPrefixOptions_m(TagLog *ptagLog,
                                        unsigned int uiOptions)
{
  /* Check for invalid Options */
  if(BITS_UNSET(uiOptions,
                (LOGC_OPTION_PREFIX_FILEINFO                 |
                 LOGC_OPTION_PREFIX_TIMESTAMP_DATE           |
                 LOGC_OPTION_PREFIX_TIMESTAMP_TIME           |
                 LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS |
                 LOGC_OPTION_PREFIX_TIMESTAMP_UTC            |
                 LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME)))
  {
    return(-1);
  }
  /* UTC/Localtime are mutually exclusive */
  if((uiOptions&(LOGC_OPTION_PREFIX_TIMESTAMP_UTC|LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME)) ==
      (LOGC_OPTION_PREFIX_TIMESTAMP_UTC|LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME))
    return(-1);
  /* Check timezone setting, set to UTC (default) if not selected */
  if(!(uiOptions&(LOGC_OPTION_PREFIX_TIMESTAMP_UTC|LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME)))
  {
    if(!(ptagLog->uiLogOptions&(LOGC_OPTION_PREFIX_TIMESTAMP_UTC|LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME)))
      uiOptions|=LOGC_OPTION_PREFIX_TIMESTAMP_UTC;
    else
      uiOptions|=ptagLog->uiLogOptions&(LOGC_OPTION_PREFIX_TIMESTAMP_UTC|LOGC_OPTION_PREFIX_TIMESTAMP_LOCALTIME);
  }
  ptagLog->uiLogOptions&=0xF000;
  ptagLog->uiLogOptions|=uiOptions;
  return(0);
}

