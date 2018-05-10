#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#include "LogC.h"

#define LOGC_DATETIME_FORMAT           "%Y-%m-%d_%H:%M:%S "                         /* e.g. 2018-10-03_12:34:56, see strftime() function */
#define LOGC_PREFIX_FORMAT_FILEINFO    "[%s]\"%s\"@line %d in function \"%s()\": "  /* e.g. [Error]"myfile.c"@line 123 in function "func()": */
#define LOGC_PREFIX_FORMAT_NO_FILEINFO "[%s]in function \"%s()\": "                 /* e.g. [Error]in function "func()": */

#define LOGC_TEXT_NO_FUNCTION "???"

#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  #ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN /* Avoid include of useless windows headers */
      #include <windows.h>
    #undef WIN32_LEAN_AND_MEAN
    typedef CRITICAL_SECTION TMutex;
    #define LOGC_MUTEX_INIT(log)    if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {InitializeCriticalSection(&(log)->tMutex);}
    #define LOGC_MUTEX_DESTROY(log) if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {DeleteCriticalSection(&(log)->tMutex);}
    #define LOGC_MUTEX_LOCK(log)    if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {EnterCriticalSection(&(log)->tMutex);}
    #define LOGC_MUTEX_UNLOCK(log)  if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {LeaveCriticalSection(&(log)->tMutex);}
  #elif defined(__unix__)
    #include <unistd.h> /* For determining the current POSIX-Version */
    #if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L) /* POSIX IEEE 1003.1-2001 or newer */
      #include <pthread.h>
      #include <assert.h>
      typedef pthread_mutex_t TMutex;
      #define LOGC_MUTEX_INIT(log)    if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {assert(!pthread_mutex_init(&(log)->tMutex,NULL));}
      #define LOGC_MUTEX_DESTROY(log) if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {assert(!pthread_mutex_destroy(&(log)->tMutex));}
      #define LOGC_MUTEX_LOCK(log)    if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {assert(!pthread_mutex_lock(&(log)->tMutex));}
      #define LOGC_MUTEX_UNLOCK(log)  if(((log)->uiLogOptions&LOGC_OPTION_THREADSAFE)) {assert(!pthread_mutex_unlock(&(log)->tMutex));}
    #else
      #error POSIX Version too old, must be POSIX IEEE 1003.1-2001 or newer for pthreads.
    #endif
  #else
    #error No Mutex system implemented!
  #endif /* _WIN32 */
#else
  #define LOGC_MUTEX_INIT(log)
  #define LOGC_MUTEX_DESTROY(log)
  #define LOGC_MUTEX_LOCK(log)
  #define LOGC_MUTEX_UNLOCK(log)
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */

#define BITS_UNSET(val,bitmap) (val&(~(bitmap)))

#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* >= C99 */
  #define INLINE_FCT inline
  #define INLINE_PROT static inline
#else /* No inline available from C Standard */
  #define INLINE_FCT
  #define INLINE_PROT
#endif /* __STDC_VERSION__ >= C99 */

#define LOGC_PATH_MAXLEN           260 /* Should be enough for any usual cases */
#define LOGC_DEFAULT_FILEQUEUESIZE 20

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

#if defined(LOGC_FEATURE_ENABLE_LOGFILE) || defined(LOGC_FEATURE_ENABLE_LOG_STORAGE)
struct TagLogCEntry_t
{
  size_t szTextLength;
  char *pcText;
  struct TagLogCEntry_t *ptagNext;
};
typedef struct TagLogCEntry_t TagLogCEntry;
#endif /* LOGC_FEATURE_ENABLE_LOGFILE || LOGC_FEATURE_ENABLE_LOG_STORAGE */

struct TagLog_t
{
  int iLogLevel;
  unsigned int uiLogOptions;
  size_t szMaxEntryLength;
  char *pcTextBuffer;
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  size_t szMaxFileQueueSize;
  size_t szCurrentFileQueueSize;
  TagLogCEntry *ptagFileQueueFirst;
  TagLogCEntry *ptagFileQueueLast;
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

/* Functions if logfile is enabled */
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
INLINE_PROT int iLogC_WriteEntriesToDisk_m(TagLog *ptagLog);
INLINE_PROT void vLogc_FileQueue_Push_m(TagLog *ptagLog,
                                             TagLogCEntry *ptagEntry);

INLINE_PROT TagLogCEntry *ptagLogC_FileQueue_Pop_m(TagLog *ptagLog);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
INLINE_PROT void vLogC_StoragePush_m(TagLog *ptagLog,
                                           TagLogCEntry *ptagEntry);

INLINE_PROT char *pcLogC_StoragePop_m(TagLog *ptagLog,
                                            size_t *pszEntryLength);
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

const struct TagLogType *ptagLogC_GetLogType_m(int iLogType)
{
  size_t szIndex;
  for(szIndex=0;szIndex<sizeof(tagLogTypes)/sizeof(struct TagLogType);++szIndex)
  {
    if(tagLogTypes[szIndex].iLogType==iLogType)
      return(&tagLogTypes[szIndex]);
  }
  return(NULL);
}

TagLog *ptagLogC_New_g(int iLogLevel,
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

  /* Check for invalid log-Options*/
  if(BITS_UNSET(uiLogOptions,
                (LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_TIMESTAMP
#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
                 |LOGC_OPTION_THREADSAFE
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
                 )))
    return(NULL);
  if(szMaxEntryLength<sizeof(LOGC_DATETIME_FORMAT)+sizeof(LOGC_PREFIX_FORMAT_FILEINFO)+10)
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

  ptagNewLog->pcTextBuffer=((char*)ptagNewLog)+sizeof(TagLog);
  ptagNewLog->szMaxEntryLength=szMaxEntryLength;
  ptagNewLog->iLogLevel=iLogLevel;
  ptagNewLog->uiLogOptions=uiLogOptions;

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  if(ptagLogFile)
  {
    ptagNewLog->szMaxFileQueueSize=ptagLogFile->szMaxFileQueueSize;
    strcpy(ptagNewLog->caLogPath,ptagLogFile->pcFilePath);
  }
  else
  {
    ptagNewLog->szMaxFileQueueSize=0;
    ptagNewLog->caLogPath[0]='\0';
  }
  ptagNewLog->szCurrentFileQueueSize=0;
  ptagNewLog->ptagFileQueueFirst=NULL;
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  ptagNewLog->szMaxStorageCount=szMaxStorageCount;
  ptagNewLog->szStoredLogsCount=0;
  ptagNewLog->ptagSavedLogFirst=NULL;
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  LOGC_MUTEX_INIT(ptagNewLog);
  return(ptagNewLog);
}

int iLogC_End_g(TagLog *ptagLog)
{
  LOGC_MUTEX_LOCK(ptagLog);
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  /* Check if there are entries to be written */
  if((ptagLog->caLogPath[0]!='\0') && (ptagLog->szCurrentFileQueueSize))
  {
    if(iLogC_WriteEntriesToDisk_m(ptagLog))
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
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

int iLogC_SetPrefixOptions_g(TagLog *ptagLog,
                             unsigned int uiNewPrefixOptions)
{
  /* Check for invalid Options */
  if(BITS_UNSET(uiNewPrefixOptions,(LOGC_OPTION_PREFIX_FILEINFO|LOGC_OPTION_PREFIX_TIMESTAMP)))
    return(-1);
  ptagLog->uiLogOptions&=(UINT_MAX&~0xFF);
  ptagLog->uiLogOptions|=(uiNewPrefixOptions&0xFF);
  return(0);
}

int iLogC_AddEntry_Text_g(TagLog *ptagLog,
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
  /* Check if Timestamp is needed */
  if((ptagLog->uiLogOptions&LOGC_OPTION_PREFIX_TIMESTAMP))
  {
    time_t tTime=time(NULL);
#if defined(LOGC_FEATURE_ENABLE_THREADSAFETY) && !defined(_WIN32) /* Check if threadsafe implementation is needed (WIN32 localtime() is threadsafe anyway) */
    struct tm tagTime;
  #if defined(_POSIX_VERSION) && (_POSIX_VERSION >= 200112L) /* POSIX IEEE 1003.1-2001 or newer */
    localtime_r(&tTime,&tagTime);
  #else /* No POSIX nor Win32 */
    #error No threadsafe localtime() available!
  #endif /* _POSIX_VERSION >= 200112L */
#else /* !LOGC_FEATURE_ENABLE_THREADSAFETY or WIN32 */
    struct tm *ptagTime=localtime(&tTime);
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY && !WIN32 */

    if(!(szCurrBufferPos+=strftime(ptagLog->pcTextBuffer,
                                   ptagLog->szMaxEntryLength,
                                   LOGC_DATETIME_FORMAT,
#if defined(LOGC_FEATURE_ENABLE_THREADSAFETY) && !defined(_WIN32)
                                   &tagTime
#else /* !LOGC_FEATURE_ENABLE_THREADSAFETY or WIN32 */
                                   ptagTime
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY && !WIN32 */
                                   )))
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
  }

  /* Check if Fileinfo is needed */
  if((ptagLog->uiLogOptions&LOGC_OPTION_PREFIX_FILEINFO))
  {
    if(sizeof(LOGC_PREFIX_FORMAT_FILEINFO)+((pcFunction)?strlen(pcFunction):sizeof(LOGC_TEXT_NO_FUNCTION))+strlen(pcFileName)+10>ptagLog->szMaxEntryLength-szCurrBufferPos)
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
    printf("1: iRc=%d, szCurrBufferPos=%lu\n",iRc,(unsigned long)szCurrBufferPos);   // TODO: remove this
  }
  else
  {
    if(sizeof(LOGC_PREFIX_FORMAT_NO_FILEINFO)+((pcFunction)?strlen(pcFunction):sizeof(LOGC_TEXT_NO_FUNCTION))+10>ptagLog->szMaxEntryLength-szCurrBufferPos)
    {
      LOGC_MUTEX_UNLOCK(ptagLog);
      return(-1);
    }
    iRc=sprintf(&ptagLog->pcTextBuffer[szCurrBufferPos],
                LOGC_PREFIX_FORMAT_NO_FILEINFO,
                ptagCurrLogType->pcText,
                (pcFunction)?pcFunction:LOGC_TEXT_NO_FUNCTION);
    printf("2: iRc=%d, szCurrBufferPos=%lu\n",iRc,(unsigned long)szCurrBufferPos);   // TODO: remove this
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
  printf("3: iRc=%d, szCurrBufferPos=%lu\n",iRc,(unsigned long)szCurrBufferPos);   // TODO: remove this


  /* Check for truncation */
  if(iRc<1)
  {
    puts("TRUNCATED!");   // TODO: remove this
    szCurrBufferPos=ptagLog->szMaxEntryLength;
    ptagLog->pcTextBuffer[szCurrBufferPos]='\n';
    ptagLog->pcTextBuffer[++szCurrBufferPos]='\0';
  }
  else
  {
    puts("NOT TRUNCATED!");   // TODO: remove this
    szCurrBufferPos+=iRc;
    /* Check if there's already a newline at the end */
    if(ptagLog->pcTextBuffer[szCurrBufferPos-1]!='\n')
    {
      ptagLog->pcTextBuffer[szCurrBufferPos]='\n';
      ptagLog->pcTextBuffer[++szCurrBufferPos]='\0';
    }
  }
  printf("4: iRc=%d, szCurrBufferPos=%lu\n",iRc,(unsigned long)szCurrBufferPos);   // TODO: remove this

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
  if(ptagLog->caLogPath[0]!='\0')
  {
    TagLogCEntry *ptagNewEntry;
    if(!(ptagNewEntry=malloc(sizeof(TagLogCEntry)+szCurrBufferPos)))
      return(-1);

    ptagNewEntry->szTextLength=szCurrBufferPos;
    ptagNewEntry->pcText=((char*)ptagNewEntry)+sizeof(TagLogCEntry);
    memcpy(ptagNewEntry->pcText,ptagLog->pcTextBuffer,szCurrBufferPos);
    vLogc_FileQueue_Push_m(ptagLog,ptagNewEntry);
    if((ptagLog->szMaxFileQueueSize) && (ptagLog->szCurrentFileQueueSize>=ptagLog->szMaxFileQueueSize))
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
/* For vsnprintf C99 is needed, or MSC >=1900(VS 15) */
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L))
  #ifdef _WIN32 /* Win32 _vsnprintf() is a bit diffrent from STDC... */
    if((iRc=_vsnprintf(pcBuffer,szBufferSize-1,pcFormat,vaArgs))<1)
      pcBuffer[szBufferSize-1]='\0';
    return(iRc);
  #else /* vsnprintf() which should be conformant to STDC */
    if((iRc=vsnprintf(pcBuffer,szBufferSize,pcFormat,vaArgs))>=(int)szBufferSize)
      iRc=-1;
    return(iRc);
  #endif
#else /* Terminate compilation with an Error, if no vsnprintf is available */
  #error No vsnprintf or equivalent available!
  /* Use of !UNSAFE! ANSI-C vsprintf() will work, but can cause buffer overflow!
     Use at own risk! Or better implement a custom function behaving like vsnprintf(). */
  #define LOGC_VSNPRINTF(ret,buf,size,format,arglist) ret=vsprintf(buf,format,arglist)
#endif
}

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
int iLogC_SetFilePath_g(TagLog *ptagLog,
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
  /* Write queue to old file first */
  if(iLogC_WriteEntriesToDisk_m(ptagLog))
  {
    LOGC_MUTEX_UNLOCK(ptagLog);
    return(-1);
  }
  if(pcNewPath)
    memcpy(ptagLog->caLogPath,pcNewPath,szPathLength);
  else
    ptagLog->caLogPath[0]='\0';
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(0);
}

int iLogC_WriteEntriesToDisk_g(TagLog *ptagLog)
{
  int iRc;
  LOGC_MUTEX_LOCK(ptagLog);
  iRc=iLogC_WriteEntriesToDisk_m(ptagLog);
  LOGC_MUTEX_UNLOCK(ptagLog);
  return(iRc);
}

INLINE_FCT int iLogC_WriteEntriesToDisk_m(TagLog *ptagLog)
{
  FILE *fp;

  if((!ptagLog->caLogPath[0]) || (!ptagLog->szCurrentFileQueueSize))
    return(0);

  if(!(fp=fopen(ptagLog->caLogPath,"a")))
  {
    perror("Failed to Open LogFile: ");
  }
  while(ptagLog->szCurrentFileQueueSize)
  {
    TagLogCEntry *ptagCurrEntry;
    if(!(ptagCurrEntry=ptagLogC_FileQueue_Pop_m(ptagLog)))
    {
      if(fp)
        fclose(fp);
      return(-1);
    }
    if(fp)
      fputs(ptagCurrEntry->pcText,fp);
    free(ptagCurrEntry);
  }
  if(fp)
    fclose(fp);
  return((fp)?0:-1);
}

INLINE_FCT void vLogc_FileQueue_Push_m(TagLog *ptagLog,
                                       TagLogCEntry *ptagEntry)
{
  /* Check if first entry in queue */
  ptagEntry->ptagNext=NULL;
  if(!ptagLog->ptagFileQueueFirst)
  {
    ptagLog->ptagFileQueueFirst=ptagEntry;
    ptagLog->ptagFileQueueLast=ptagEntry;
    ptagLog->szCurrentFileQueueSize=1;
    return;
  }
  ptagLog->ptagFileQueueLast->ptagNext=ptagEntry;
  ptagLog->ptagFileQueueLast=ptagEntry;
  ++ptagLog->szCurrentFileQueueSize;
  return;
}

INLINE_FCT TagLogCEntry *ptagLogC_FileQueue_Pop_m(TagLog *ptagLog)
{
  TagLogCEntry *ptagTmp;

  if(!ptagLog->ptagFileQueueFirst)
    return(NULL);

  ptagTmp=ptagLog->ptagFileQueueFirst;
  ptagLog->ptagFileQueueFirst=ptagLog->ptagFileQueueFirst->ptagNext;
  --ptagLog->szCurrentFileQueueSize;
  return(ptagTmp);
}
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
char *pcLogC_StorageGetNextLog_g(TagLog *ptagLog,
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
