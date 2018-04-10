#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "LogC.h"

#define LOGC_DATETIME_FORMAT           "%d.%m.%Y-%H:%M:%S "                         /* e.g. 01.02.2018-12:34:56, see strftime() function */
#define LOGC_PREFIX_FORMAT_FILEINFO    "[%s]\"%s\"@line %d in function \"%s()\": "  /* e.g. [Error]"myfile.c"@line 123 in function "func()": */
#define LOGC_PREFIX_FORMAT_NO_FILEINFO "[%s]in function \"%s()\": "                 /* e.g. [Error]in function "func()": */

#define LOGC_TEXT_NO_FUNCTION "???"

#ifdef _WIN32
  #define LOGC_PATH_DELIMITER  '\\'
  #define LOGC_PATH_DELIMITER2 '/'
  #define LOGC_LOCALTIME(tTime,tStruct) localtime_s(tStruct,tTime)
#elif __linux__
  #define LOGC_PATH_DELIMITER  '/'
  #define LOGC_LOCALTIME(tTime,tStruct) localtime_r(tTime,tStruct)
#endif /* _WIN32 */

#if defined (__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L) /* >= C99 */
  #define INLINE_FCT_LOCAL inline
  #define INLINE_PROT_LOCAL static inline
#else /* No inline available from C Standard */
  #define INLINE_FCT_LOCAL
  #define INLINE_PROT_LOCAL
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
  unsigned int uiPrefixOptions;
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
};

/* Functions if logfile is enabled */
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
INLINE_PROT_LOCAL void vLogc_FileQueue_Push_m(TagLog *ptagLog,
                                             TagLogCEntry *ptagEntry);

INLINE_PROT_LOCAL TagLogCEntry *ptagLogC_FileQueue_Pop_m(TagLog *ptagLog);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
INLINE_PROT_LOCAL void vLogC_StoragePush_m(TagLog *ptagLog,
                                           TagLogCEntry *ptagEntry);

INLINE_PROT_LOCAL char *pcLogC_StoragePop_m(TagLog *ptagLog,
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
                       unsigned int uiPrefixOptions
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

  if(szMaxEntryLength<sizeof(LOGC_DATETIME_FORMAT)+sizeof(LOGC_PREFIX_FORMAT_FILEINFO)+20)
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

  szNewLogSize=szMaxEntryLength+1;
  /* Check for Overflow of size_t */
  if(szNewLogSize+sizeof(TagLog)<szMaxEntryLength)
    return(NULL);
  if(!(ptagNewLog=malloc(szNewLogSize+sizeof(TagLog))))
    return(NULL);

  ptagNewLog->pcTextBuffer=((char*)ptagNewLog)+sizeof(TagLog);
  ptagNewLog->szMaxEntryLength=szMaxEntryLength;
  ptagNewLog->iLogLevel=iLogLevel;
  ptagNewLog->uiPrefixOptions=uiPrefixOptions;

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
  return(ptagNewLog);
}

int iLogC_End_g(TagLog *ptagLog)
{
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
  /* Check if there are entries to be written */
  if((ptagLog->caLogPath[0]!='\0') && (ptagLog->szCurrentFileQueueSize))
  {
    if(iLogC_WriteEntriesToDisk_g(ptagLog))
      return(-1);
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  while(ptagLog->szStoredLogsCount)
  {
    free(pcLogC_StoragePop_m(ptagLog,NULL));
  }
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  free(ptagLog);
  return(0);
}

void vLogC_SetPrefixOptions_g(TagLog *ptagLog,
                              unsigned int uiNewPrefixOptions)
{
  ptagLog->uiPrefixOptions=uiNewPrefixOptions;
}

int iLogC_AddEntry_Text_g(TagLog *ptagLog,
                          int iLogType,
                          const char *pcFileName,
                          int iLineNr,
                          const char *pcFunction,
                          const char *pcLogText)
{
  int iRc;
  size_t szTextLen;
  size_t szCurrBufferPos=0;
  const struct TagLogType *ptagCurrLogType;

  if(iLogType<ptagLog->iLogLevel)
    return(0);
  if(!(ptagCurrLogType=ptagLogC_GetLogType_m(iLogType)))
    return(-1);
  if((ptagLog->uiPrefixOptions&LOGC_PREFIX_TIMESTAMP))
  {
    struct tm tagTime;
    time_t tTime=time(NULL);
    LOGC_LOCALTIME(&tTime,&tagTime);
    if(!(szCurrBufferPos+=strftime(ptagLog->pcTextBuffer,
                                   ptagLog->szMaxEntryLength,
                                   LOGC_DATETIME_FORMAT,
                                   &tagTime)))
      return(-1);
  }
  if((ptagLog->uiPrefixOptions&LOGC_PREFIX_FILEINFO))
  {
    if(sizeof(LOGC_PREFIX_FORMAT_FILEINFO)+((pcFunction)?strlen(pcFunction):sizeof(LOGC_TEXT_NO_FUNCTION))+strlen(pcFileName)+20>ptagLog->szMaxEntryLength)
      return(-1);
    iRc=sprintf(&ptagLog->pcTextBuffer[szCurrBufferPos],
                LOGC_PREFIX_FORMAT_FILEINFO,
                ptagCurrLogType->pcText,
                pcFileName,
                iLineNr,
                (pcFunction)?pcFunction:LOGC_TEXT_NO_FUNCTION);
  }
  else
  {
    if(sizeof(LOGC_PREFIX_FORMAT_NO_FILEINFO)+((pcFunction)?strlen(pcFunction):sizeof(LOGC_TEXT_NO_FUNCTION))+20>ptagLog->szMaxEntryLength)
      return(-1);
    iRc=sprintf(&ptagLog->pcTextBuffer[szCurrBufferPos],
                LOGC_PREFIX_FORMAT_NO_FILEINFO,
                ptagCurrLogType->pcText,
                (pcFunction)?pcFunction:LOGC_TEXT_NO_FUNCTION);
  }
  if(iRc<1)
    return(-1);

  szCurrBufferPos+=iRc;
  szTextLen=strlen(pcLogText);
  /* Check for length of Entry */
  if(szCurrBufferPos+szTextLen>ptagLog->szMaxEntryLength-1)
  {
    /* Truncate entry here */
    memcpy(&ptagLog->pcTextBuffer[szCurrBufferPos],
           pcLogText,
           ptagLog->szMaxEntryLength-szCurrBufferPos);
    szCurrBufferPos=ptagLog->szMaxEntryLength;
  }
  else
  {
    memcpy(&ptagLog->pcTextBuffer[szCurrBufferPos],
           pcLogText,
           szTextLen);

    szCurrBufferPos+=szTextLen;
    /* Add newline at the end, if not there yet */
    if(ptagLog->pcTextBuffer[szCurrBufferPos-1]!='\n')
      ++szCurrBufferPos;
  }
  ptagLog->pcTextBuffer[szCurrBufferPos-1]='\n';
  ptagLog->pcTextBuffer[szCurrBufferPos]='\0';
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
      return(iLogC_WriteEntriesToDisk_g(ptagLog));
    }
  }
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  if(ptagLog->szMaxFileQueueSize)
  {
    char *pcTmp;
    if(!(pcTmp=malloc(sizeof(TagLogCEntry)+szCurrBufferPos)))
      return(-1);

    ((TagLogCEntry*)(pcTmp+szCurrBufferPos))->pcText=pcTmp;
    ((TagLogCEntry*)(pcTmp+szCurrBufferPos))->szTextLength=szCurrBufferPos;
    memcpy(pcTmp,ptagLog->pcTextBuffer,szCurrBufferPos);
    vLogC_StoragePush_m(ptagLog,((TagLogCEntry*)(pcTmp+szCurrBufferPos)));
  }
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
  return(0);
}

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
int iLogC_SetFilePath_g(TagLog *ptagLog,
                        const char *pcNewPath)
{
  size_t szPathLength;
  if(pcNewPath)
  {
    szPathLength=strlen(pcNewPath)+1;
    if(szPathLength>LOGC_PATH_MAXLEN)
      return(-1);
  }
  /* Write queue to old file first */
  if(iLogC_WriteEntriesToDisk_g(ptagLog))
    return(-1);

  if(!pcNewPath)
    ptagLog->caLogPath[0]='\0';
  else
    memcpy(ptagLog->caLogPath,pcNewPath,szPathLength);
  return(0);
}

int iLogC_WriteEntriesToDisk_g(TagLog *ptagLog)
{
  FILE *fp;
  TagLogCEntry *ptagCurrEntry;

  if((!ptagLog->caLogPath[0]) || (!ptagLog->szCurrentFileQueueSize))
    return(0);

  if(!(fp=fopen(ptagLog->caLogPath,"a")))
    return(-1);

  while(ptagLog->szCurrentFileQueueSize)
  {
    if(!(ptagCurrEntry=ptagLogC_FileQueue_Pop_m(ptagLog)))
    {
      fclose(fp);
      return(-1);
    }
    fputs(ptagCurrEntry->pcText,fp);
    free(ptagCurrEntry);
  }
  fclose(fp);
  return(0);
}

INLINE_FCT_LOCAL void vLogc_FileQueue_Push_m(TagLog *ptagLog,
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

INLINE_FCT_LOCAL TagLogCEntry *ptagLogC_FileQueue_Pop_m(TagLog *ptagLog)
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
  return(pcLogC_StoragePop_m(ptagLog,pszEntryLength));
}

INLINE_FCT_LOCAL void vLogC_StoragePush_m(TagLog *ptagLog,
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

INLINE_FCT_LOCAL char *pcLogC_StoragePop_m(TagLog *ptagLog,
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
