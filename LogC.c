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

#define LOGC_TIMESTAMP_FORMAT_DATE      "%Y-%m-%d"
#define LOGC_TIMESTAMP_FORMAT_TIME      "%H:%M:%S"
#define LOGC_TIMESTAMP_FORMAT_DATETIME  LOGC_TIMESTAMP_FORMAT_DATE "_" LOGC_TIMESTAMP_FORMAT_TIME /* e.g. 2018-10-03_12:34:56, see strftime() function */
#define LOGC_PREFIX_FORMAT_LOGTYPE      "[%s]"
#define LOGC_PREFIX_FORMAT_FCTNAME      "in function \"%s()\""
#define LOGC_PREFIX_FORMAT_FILEINFO     "\"%s\"@line %d"  /* e.g. [Error]"myfile.c"@line 123 */
#define LOGC_PREFIX_FORMAT_FILEFCTNAME  LOGC_PREFIX_FORMAT_FILEINFO ", " LOGC_PREFIX_FORMAT_FCTNAME

#define LOGC_TEXT_UNKNOWN "???"

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
  #endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
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
#define LOGC_OPTIONS_ENABLED(log,option) (((log)->uiLogOptions&(option))==(option))

#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* >= C99 */
  #define INLINE_FCT inline
  #define INLINE_PROT static inline
#else /* No inline available from C Standard */
  #define INLINE_FCT
  #define INLINE_PROT
#endif /* __STDC_VERSION__ >= C99 */

#define LOGC_PATH_MAXLEN           260 /* Should be enough for any usual cases */
#define LOGC_DEFAULT_FILEQUEUESIZE 10

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
}tagLogTypes_m[]=
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

INLINE_PROT int iLogC_snprintf(char *pcDest,
                               size_t szBufferSize,
                               const char *pcFormat,
                               ...);

INLINE_PROT int iLogC_vsnprintf(char *pcBuffer,
                                size_t szBufferSize,
                                const char *pcFormat,
                                va_list vaArgs);

INLINE_PROT int iLogC_AddTimeStamp_m(LogC ptagLog,
                                     size_t *pszBufferPos);
INLINE_PROT int iLogC_AddPrefix_m(LogC ptagLog,
                                  size_t *pszBufferPos,
                                  const struct TagLogType *ptagLogType,
                                  const char *pcFileName,
                                  int iLineNr,
                                  const char *pcFunction);

INLINE_PROT int iLogC_SetPrefixFormat_m(LogC ptagLog,
                                        unsigned int uiOptions);
INLINE_PROT int iLogC_SetLogOptions_m(LogC ptagLog,
                                      unsigned int uiOptions);

INLINE_PROT const struct TagLogType *ptagLogC_GetLogType_m(int iLogType);

/* Functions if logfile is enabled */
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
INLINE_FCT int iLogC_CheckFilePathValid_m(const char *pcPath);
INLINE_PROT int iLogC_WriteEntriesToDisk_m(LogC ptagLog);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
INLINE_PROT void vLogC_StoragePush_m(LogC ptagLog,
                                     TagLogCEntry *ptagEntry);

INLINE_PROT char *pcLogC_StoragePop_m(LogC ptagLog,
                                      size_t *pszEntryLength);
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

LogC LogC_New(int logLevel,
              size_t maxEntryLength,
              unsigned int logOptions
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
              ,LogCFile *logFile
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
              ,size_t maxStorageCount
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
              )
{
  LogC ptagNewLog;
  size_t szNewLogSize=0;

  if(maxEntryLength<10)
    return(NULL);
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  if((logFile) && (iLogC_CheckFilePathValid_m(logFile->pcFilePath)))
      return(NULL);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

  szNewLogSize=maxEntryLength+2; /* +2 for '\n'+'\0' */
  /* Check for Overflow of size_t */
  if(szNewLogSize+sizeof(struct TagLog_t)<maxEntryLength)
    return(NULL);
  if(!(ptagNewLog=malloc(szNewLogSize+sizeof(struct TagLog_t))))
    return(NULL);

  ptagNewLog->uiLogOptions=0;
  if((iLogC_SetLogOptions_m(ptagNewLog,
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
                            BITS_UNSET(logOptions,(0x00FF|LOGC_OPTION_THREADSAFE))
#else
                            BITS_UNSET(logOptions,0x00FF)
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
                            )) ||
     (iLogC_SetPrefixFormat_m(ptagNewLog,
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
                              BITS_UNSET(logOptions,(0xFF00|LOGC_OPTION_THREADSAFE))
#else
                              BITS_UNSET(logOptions,0xFF00)
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
                              )))
  {
    free(ptagNewLog);
    return(NULL);
  }
  ptagNewLog->pcTextBuffer=((char*)ptagNewLog)+sizeof(struct TagLog_t);
  ptagNewLog->szMaxEntryLength=maxEntryLength;
  ptagNewLog->iLogLevel=logLevel;

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  if(logFile)
  {
    if(!(ptagNewLog->pcLogFileQueueBuffer=malloc(LOGC_DEFAULT_FILEQUEUESIZE*(maxEntryLength+2))))
    {
      free(ptagNewLog);
      return(NULL);
    }
    strcpy(ptagNewLog->caLogPath,logFile->pcFilePath);
  }
  else
  {
    ptagNewLog->pcLogFileQueueBuffer=NULL;
  }
  ptagNewLog->szLogFileQueueCount=0;
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  ptagNewLog->szMaxStorageCount=maxStorageCount;
  ptagNewLog->szStoredLogsCount=0;
  ptagNewLog->ptagSavedLogFirst=NULL;
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  ptagNewLog->uiLogOptions|=(logOptions&LOGC_OPTION_THREADSAFE);
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
  LOGC_MUTEX_INIT(ptagNewLog);
  return(ptagNewLog);
}

int LogC_End(LogC log)
{
  LOGC_MUTEX_LOCK(log);
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  /* Check if there are entries to be written */
  if((log->pcLogFileQueueBuffer) && (log->szLogFileQueueCount))
  {
    if(iLogC_WriteEntriesToDisk_m(log))
    {
      LOGC_MUTEX_UNLOCK(log);
      return(-1);
    }
    free(log->pcLogFileQueueBuffer);
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  while(log->szStoredLogsCount)
  {
    free(pcLogC_StoragePop_m(log,NULL));
  }
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  LOGC_MUTEX_UNLOCK(log);
  LOGC_MUTEX_DESTROY(log);
  free(log);
  return(0);
}

int LogC_SetPrefixFormat(LogC log,
                         unsigned int newPrefixFormat)
{
  int iRc;
  LOGC_MUTEX_LOCK(log);
  iRc=iLogC_SetPrefixFormat_m(log,newPrefixFormat);
  LOGC_MUTEX_UNLOCK(log);
  return(iRc);
}

int LogC_SetLogOptions(LogC log,
                       unsigned int newOptions)
{
  int iRc;
  LOGC_MUTEX_LOCK(log);
  iRc=iLogC_SetLogOptions_m(log,newOptions);
  LOGC_MUTEX_UNLOCK(log);
  return(iRc);
}

int LogC_AddEntry_Text(LogC log,
                       int logType,
                       const char *fileName,
                       int lineNr,
                       const char *functionName,
                       const char *logText,
                       ...)
{
  int iRc=0;
  size_t szCurrBufferPos=0;
  va_list vaArgs;
  const struct TagLogType *ptagCurrLogType;

  if(logType<log->iLogLevel)
    return(0);
  if(!(ptagCurrLogType=ptagLogC_GetLogType_m(logType)))
    return(-1);

  LOGC_MUTEX_LOCK(log);
  /* Add Timestamp, if needed */
  if(iLogC_AddTimeStamp_m(log,&szCurrBufferPos))
  {
    LOGC_MUTEX_UNLOCK(log);
    return(-1);
  }
  if(iLogC_AddPrefix_m(log,&szCurrBufferPos,ptagCurrLogType,fileName,lineNr,functionName))
  {
    LOGC_MUTEX_UNLOCK(log);
    return(-1);
  }
  if(szCurrBufferPos) /* Add ': ' */
  {
    if(log->szMaxEntryLength-szCurrBufferPos>2)
    {
      log->pcTextBuffer[szCurrBufferPos++]=':';
      log->pcTextBuffer[szCurrBufferPos++]=' ';
    }
    else
      return(-1);
  }
  errno=0;
  va_start(vaArgs,logText);
  iRc=iLogC_vsnprintf(&log->pcTextBuffer[szCurrBufferPos],
                      log->szMaxEntryLength+1-szCurrBufferPos, /* +1 is okay, we have 2 more bytes reserved then szMaxEntryLength */
                      logText,
                      vaArgs);
  va_end(vaArgs);
  if(errno==EINVAL)
  {
    LOGC_MUTEX_UNLOCK(log);
    return(-1);
  }
  /* Check for truncation */
  if(iRc<1)
  {
    szCurrBufferPos=log->szMaxEntryLength;
    log->pcTextBuffer[szCurrBufferPos]='\n';
    log->pcTextBuffer[++szCurrBufferPos]='\0';
  }
  else
  {
    szCurrBufferPos+=iRc;
    /* Check if there's already a newline at the end */
    if(log->pcTextBuffer[szCurrBufferPos-1]!='\n')
    {
      log->pcTextBuffer[szCurrBufferPos]='\n';
      log->pcTextBuffer[++szCurrBufferPos]='\0';
    }
  }
  switch(ptagCurrLogType->eOutStream)
  {
    case LOGC_STDOUT:
      if(!LOGC_OPTIONS_ENABLED(log,LOGC_OPTION_IGNORE_STDOUT))
        fputs(log->pcTextBuffer,stdout);
      break;
    case LOGC_STDERR:
      if(!LOGC_OPTIONS_ENABLED(log,LOGC_OPTION_IGNORE_STDERR))
        fputs(log->pcTextBuffer,stderr);
      break;
    default:
      break;
  }
  ++szCurrBufferPos;
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  /* Add entry to filequeue, if needed */
  if(log->pcLogFileQueueBuffer)
  {
    assert(log->szLogFileQueueCount<LOGC_DEFAULT_FILEQUEUESIZE);
    memcpy(&log->pcLogFileQueueBuffer[log->szLogFileQueueCount*(log->szMaxEntryLength+2)],
           log->pcTextBuffer,
           szCurrBufferPos);
    ++log->szLogFileQueueCount;
    if(log->szLogFileQueueCount==LOGC_DEFAULT_FILEQUEUESIZE)
    {
      if(iLogC_WriteEntriesToDisk_m(log))
      {
        LOGC_MUTEX_UNLOCK(log);
        return(-1);
      }
    }
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  if(log->szMaxStorageCount)
  {
    char *pcTmp;
    if(!(pcTmp=malloc(sizeof(TagLogCEntry)+szCurrBufferPos)))
    {
      LOGC_MUTEX_UNLOCK(log);
      return(-1);
    }
    ((TagLogCEntry*)(pcTmp+szCurrBufferPos))->pcText=pcTmp;
    ((TagLogCEntry*)(pcTmp+szCurrBufferPos))->szTextLength=szCurrBufferPos;
    memcpy(pcTmp,log->pcTextBuffer,szCurrBufferPos);
    vLogC_StoragePush_m(log,((TagLogCEntry*)(pcTmp+szCurrBufferPos)));
  }
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  LOGC_MUTEX_UNLOCK(log);
  return(0);
}

INLINE_FCT int iLogC_snprintf(char *pcDest,
                              size_t szBufferSize,
                              const char *pcFormat,
                              ...)
{
  int iRc;
  va_list args;
  va_start(args,pcFormat);
/* For (v)snprintf C99 is needed, or on Windows, VS 15+ */
#if ((defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || (defined(_MSC_VER) && _MSC_VER >= 1900))
  /* vsnprintf() which should be conformant to STDC */
  if((iRc=vsnprintf(pcDest,szBufferSize,pcFormat,args))>=(int)szBufferSize)
    iRc=-1;
#elif _WIN32 /* Win32's (old) _vsnprintf() is a bit diffrent from STDC... */
  if((iRc=_vsnprintf(pcDest,szBufferSize-1,pcFormat,args))<0)
    pcDest[szBufferSize-1]='\0'; /* Manual termination */
#else /* Terminate compilation with an Error, if no vsnprintf is available */
  #error No snprintf or equivalent available!
  /* Use of !UNSAFE! ANSI-C sprintf() will work, but can cause buffer overflow!
     Use at own risk! Or better implement a custom function behaving like snprintf(). */
  iRc=sprintf(pcDest,pcFormat,vaArgs);
#endif
  va_end(args);
  return(iRc);
}

INLINE_FCT int iLogC_vsnprintf(char *pcDest,
                               size_t szBufferSize,
                               const char *pcFormat,
                               va_list args)
{
  int iRc;
/* For vsnprintf C99 is needed, or on Windows, VS 15+ */
#if ((defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || (defined(_MSC_VER) && _MSC_VER >= 1900))
  /* vsnprintf() which should be conformant to STDC */
  if((iRc=vsnprintf(pcDest,szBufferSize,pcFormat,args))>=(int)szBufferSize)
    iRc=-1;
#elif _WIN32 /* Win32's (old) _vsnprintf() is a bit diffrent from STDC... */
  if((iRc=_vsnprintf(pcDest,szBufferSize-1,pcFormat,args))<0)
    pcDest[szBufferSize-1]='\0'; /* Manual termination */
#else /* Terminate compilation with an Error, if no vsnprintf is available */
  #error No vsnprintf or equivalent available!
  /* Use of !UNSAFE! ANSI-C vsprintf() will work, but can cause buffer overflow!
     Use at own risk! Or better implement a custom function behaving like vsnprintf(). */
  iRc=vsprintf(pcDest,pcFormat,vaArgs);
#endif
  return(iRc);
}

INLINE_FCT int iLogC_AddPrefix_m(LogC ptagLog,
                                 size_t *pszBufferPos,
                                 const struct TagLogType *ptagLogType,
                                 const char *pcFileName,
                                 int iLineNr,
                                 const char *pcFunction)
{
  int iRc=0;
  if(!pcFunction)
    pcFunction=LOGC_TEXT_UNKNOWN;
  if(!pcFileName)
    pcFileName=LOGC_TEXT_UNKNOWN;

  if(*pszBufferPos) /* Add ' ' */
  {
    if(ptagLog->szMaxEntryLength-*pszBufferPos>2)
      ptagLog->pcTextBuffer[(*pszBufferPos)++]=' ';
    else
      return(-1);
  }
  /* Add Fileinfo + Functionname */
  if(LOGC_OPTIONS_ENABLED(ptagLog,(LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_FUNCTIONNAME)))
  {
    if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_LOGTYPETEXT))
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         ptagLog->szMaxEntryLength-*pszBufferPos,
                         LOGC_PREFIX_FORMAT_LOGTYPE LOGC_PREFIX_FORMAT_FILEFCTNAME,
                         ptagLogType->pcText,
                         pcFileName,
                         iLineNr,
                         pcFunction);
    }
    else
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         ptagLog->szMaxEntryLength-*pszBufferPos,
                         LOGC_PREFIX_FORMAT_FILEFCTNAME,
                         pcFileName,
                         iLineNr,
                         pcFunction);
    }
  }
  /* Add Fileinfo only */
  else if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_FILEINFO))
  {
    if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_LOGTYPETEXT))
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         ptagLog->szMaxEntryLength-*pszBufferPos,
                         LOGC_PREFIX_FORMAT_LOGTYPE LOGC_PREFIX_FORMAT_FILEINFO,
                         ptagLogType->pcText,
                         pcFileName,
                         iLineNr);
    }
    else
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         ptagLog->szMaxEntryLength-*pszBufferPos,
                         LOGC_PREFIX_FORMAT_FILEINFO,
                         pcFileName,
                         iLineNr);
    }
  }
  /* Add Function name only */
  else if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_FUNCTIONNAME))
  {
    if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_LOGTYPETEXT))
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         ptagLog->szMaxEntryLength-*pszBufferPos,
                         LOGC_PREFIX_FORMAT_LOGTYPE LOGC_PREFIX_FORMAT_FCTNAME,
                         ptagLogType->pcText,
                         pcFunction);
    }
    else
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         ptagLog->szMaxEntryLength-*pszBufferPos,
                         LOGC_PREFIX_FORMAT_FCTNAME,
                         pcFunction);
    }
  }
  else /* Just add Logtypetext, if needed*/
  {
    if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_LOGTYPETEXT))
    {
      iRc=iLogC_snprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                         *pszBufferPos,
                         LOGC_PREFIX_FORMAT_LOGTYPE,
                         ptagLogType->pcText);
    }
  }
  if(iRc<0)
    return(-1);
  if(iRc)
    *pszBufferPos+=iRc;
  else if(*pszBufferPos) /* Remove previously added ' ', if no text was added here */
    --(*pszBufferPos);
  return(0);
}

INLINE_FCT int iLogC_AddTimeStamp_m(LogC ptagLog, size_t *pszBufferPos)
{
  if(   LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_DATE)
     || LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME))
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
    if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_TIMESTAMP_UTC))
      ptagTime=gmtime(&tTime);
    else if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_TIMESTAMP_LOCALTIME))
      ptagTime=localtime(&tTime);
    else
      return(-1);
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY && !WIN32 */

    if(!(*pszBufferPos+=strftime(ptagLog->pcTextBuffer,
                                 ptagLog->szMaxEntryLength,
                                 (LOGC_OPTIONS_ENABLED(ptagLog,(LOGC_OPTION_PREFIX_TIMESTAMP_DATE|LOGC_OPTION_PREFIX_TIMESTAMP_TIME)))?
                                 LOGC_TIMESTAMP_FORMAT_DATETIME:(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_DATE))?
                                 LOGC_TIMESTAMP_FORMAT_DATE:LOGC_TIMESTAMP_FORMAT_TIME,
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
  if(LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS))
  {
#if defined _WIN32
    SYSTEMTIME tCurrSysTime;
    GetSystemTime(&tCurrSysTime);
    *pszBufferPos+=sprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                           ((LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME)) || (!*pszBufferPos))?".%.3u":" .%.3u",
                           tCurrSysTime.wMilliseconds);
#elif defined (__unix__) /* Checked above for availibility */
    struct timespec tagTime;
    clock_gettime(CLOCK_REALTIME,&tagTime);
    *pszBufferPos+=sprintf(&ptagLog->pcTextBuffer[*pszBufferPos],
                           ((LOGC_OPTIONS_ENABLED(ptagLog,LOGC_OPTION_PREFIX_TIMESTAMP_TIME)) || (!*pszBufferPos))?".%.3u":" .%.3u",
                           tagTime.tv_nsec/1000000);
#endif /* _WIN32 */
  }
  return(0);
}

INLINE_FCT int iLogC_SetPrefixFormat_m(LogC ptagLog,
                                       unsigned int uiFormat)
{
  /* Check for invalid Options */
  if(BITS_UNSET(uiFormat,
                (LOGC_OPTION_PREFIX_TIMESTAMP_DATE           |
                 LOGC_OPTION_PREFIX_TIMESTAMP_TIME           |
                 LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS |
                 LOGC_OPTION_PREFIX_LOGTYPETEXT              |
                 LOGC_OPTION_PREFIX_FILEINFO                 |
                 LOGC_OPTION_PREFIX_FUNCTIONNAME)))
  {
    return(-1);
  }
  ptagLog->uiLogOptions&=0xFF00; /* Reset all Format options, but keep others */
  ptagLog->uiLogOptions|=uiFormat;
  return(0);
}

INLINE_FCT int iLogC_SetLogOptions_m(LogC ptagLog,
                                     unsigned int uiOptions)
{
  if(BITS_UNSET(uiOptions,
                (LOGC_OPTION_TIMESTAMP_UTC        |
                 LOGC_OPTION_TIMESTAMP_LOCALTIME  |
                 LOGC_OPTION_IGNORE_STDOUT        |
                 LOGC_OPTION_IGNORE_STDERR)))
  {
    return(-1);
  }
  /* UTC/Localtime are mutually exclusive */
  if((uiOptions&(LOGC_OPTION_TIMESTAMP_UTC|LOGC_OPTION_TIMESTAMP_LOCALTIME)) ==
      (LOGC_OPTION_TIMESTAMP_UTC|LOGC_OPTION_TIMESTAMP_LOCALTIME))
    return(-1);
  /* Check timezone setting, set to UTC (default) if not selected */
  if(!(uiOptions&(LOGC_OPTION_TIMESTAMP_UTC|LOGC_OPTION_TIMESTAMP_LOCALTIME)))
  {
    if(!LOGC_OPTIONS_ENABLED(ptagLog,(LOGC_OPTION_TIMESTAMP_UTC|LOGC_OPTION_TIMESTAMP_LOCALTIME)))
      uiOptions|=LOGC_OPTION_TIMESTAMP_UTC;
    else
      uiOptions|=ptagLog->uiLogOptions&(LOGC_OPTION_TIMESTAMP_UTC|LOGC_OPTION_TIMESTAMP_LOCALTIME);
  }
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  ptagLog->uiLogOptions&=(0x00FF|LOGC_OPTION_THREADSAFE); /* Reset logoptions, but keep format options+threadsafe bit if set */
#else
  ptagLog->uiLogOptions&=0x00FF; /* Reset logoptions, but keep format options */
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
  ptagLog->uiLogOptions|=uiOptions;
  return(0);
}

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
int LogC_SetFilePath(LogC log,
                     const char *newPath)
{
  /* If Path is set, check if it's valid */
  if((newPath) && (iLogC_CheckFilePathValid_m(newPath)))
    return(-1);

  LOGC_MUTEX_LOCK(log);
  /* Write queue to old file first, if needed */
  if((log->pcLogFileQueueBuffer) && (log->szLogFileQueueCount))
    if(iLogC_WriteEntriesToDisk_m(log))
    {
      LOGC_MUTEX_UNLOCK(log);
      return(-1);
    }
  if(newPath) /* If new path is set, copy anyway */
  {
    strcpy(log->caLogPath,newPath);
    if(!log->pcLogFileQueueBuffer)
      if(!(log->pcLogFileQueueBuffer=malloc(LOGC_DEFAULT_FILEQUEUESIZE*(log->szMaxEntryLength+2))))
      {
        LOGC_MUTEX_UNLOCK(log);
        return(-1);
      }
  }
  else if(log->pcLogFileQueueBuffer) /* Path was set before, but is not needed anymore */
  {
    free(log->pcLogFileQueueBuffer);
    log->pcLogFileQueueBuffer=NULL;
  }
  LOGC_MUTEX_UNLOCK(log);
  return(0);
}

int LogC_WriteEntriesToDisk(LogC log)
{
  int iRc;
  LOGC_MUTEX_LOCK(log);
  if((!log->pcLogFileQueueBuffer) || (!log->szLogFileQueueCount))
    iRc=0;
  else
    iRc=iLogC_WriteEntriesToDisk_m(log);
  LOGC_MUTEX_UNLOCK(log);
  return(iRc);
}

INLINE_FCT int iLogC_WriteEntriesToDisk_m(LogC ptagLog)
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

INLINE_FCT int iLogC_CheckFilePathValid_m(const char *pcPath)
{
  FILE *fp;

  if(!pcPath)
    return(-1);

  if(strlen(pcPath)>LOGC_PATH_MAXLEN-1)
    return(-1);

  /* Check for valid file path and write permissions */
  if(!(fp=fopen(pcPath,"a")))
    return(-1);

  fclose(fp);
  return(0);
}
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
char *LogC_StorageGetNextLog(LogC ptagLog,
                             size_t *pszEntryLength)
{
  char *pcTmp;
  LOGC_MUTEX_LOCK(ptagLog);
  pcTmp=pcLogC_StoragePop_m(ptagLog,pszEntryLength);
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(pcTmp);
}

INLINE_FCT void vLogC_StoragePush_m(LogC ptagLog,
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

INLINE_FCT char *pcLogC_StoragePop_m(LogC ptagLog,
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
  for(szIndex=0;szIndex<sizeof(tagLogTypes_m)/sizeof(struct TagLogType);++szIndex)
  {
    if(tagLogTypes_m[szIndex].iLogType==iLogType)
      return(&tagLogTypes_m[szIndex]);
  }
  return(NULL);
}



