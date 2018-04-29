#ifndef LOGC_H_INCLUDED
  #define LOGC_H_INCLUDED

#include <stdarg.h>
#include <limits.h>

#define LOGC_FEATURE_ENABLE_LOGFILE     /* Enable this option if you want to log to a file */
#define LOGC_FEATURE_ENABLE_LOG_STORAGE /* Enable this option if you want to store logs */

enum ELogType
{
  LOGC_ALL        =INT_MIN,
  LOGC_DEBUG_MORE =100,
  LOGC_DEBUG      =200,
  LOGC_INFO       =300,
  LOGC_WARNING    =1000,
  LOGC_ERROR      =1100,
  LOGC_FATAL      =1200,
  LOGC_NONE       =INT_MAX
};

enum ELogPrefix
{
  LOGC_PREFIX_FILEINFO   =0x1,
  LOGC_PREFIX_TIMESTAMP  =0x2
};

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  #define LOGC_STORAGE_MAX SIZE_MAX
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

/* Check for optional variadic macro extension (##__VA_ARGS__ in GCC) */
#if defined(_doxygen) || defined(__GNUC__) || defined(__CC_ARM) /* additional ## modifier is used to supress the trailing , if no additional arg is passed. */
  #define LOGC_OPTVARARG 1
#elif defined(__MSC_VER) /* In MSC, __VA_ARGS__ will supress the , if no arg is passed */
  #define LOGC_OPTVARARG 2
#else
  #define LOGC_OPTVARARG 0
#endif

/* Check for >= C99, or extension for __func__ macro */
#if (defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901L)) || (defined(_doxygen) || defined(__GNUC__) || defined(__CC_ARM))
  #define LOGC_FUNCTIONNAME __func__
#elif defined(__MSC_VER)          /* MS-Specific __FUNCTION__ macro */
  #define LOGC_FUNCTIONNAME __FUNCTION__
#else                            /* Fallback, if no macro for functionname is available */
  #define LOGC_FUNCTIONNAME NULL
#endif /* __STDC_VERSION__ */

typedef struct TagLog_t TagLog;

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
/**
 * Struct for Logfile options
 */
typedef struct
{
  /**
   * The Path for logging.
   */
  const char *pcFilePath;
  /**
   * The Entries won't be written directly to the file,
   * specify here how many Entriess can be queued before writing them.
   * A value of 0 means no Entries will be written automaticly.
   * @see iLogC_WriteEntriesToDisk_g() in this case
   */
  size_t szMaxFileQueueSize;
}TagLogFile;
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

/**
 * Creates a new Log-Object
 *
 * @param iLogLevel Define from which level on the logs should be shown, @see enum ELogType.
 * @param szMaxEntryLength
 *                  The maximum Textlength for a single Entry, including prefix. Longer Text will be truncated.
 *                  A newline will be added at the end of the text automatically, not counting to this size.
 * @param uiPrefixOptions
 *                  Options for Prefixing each entry, @see enum ELogPrefix
 * @param ptagLogFile
 *                  Just available if LOGC_FEATURE_ENABLE_LOGFILE is defined, contains info for logging to a file.
 *                  If no file is required, pass NULL.
 * @param szMaxStorageCount
 *                  Just available if LOGC_FEATURE_ENABLE_LOG_STORAGE is defined.
 *                  Pass the ammount of maximum entries to be stored, LOGC_STORAGE_MAX for maximum.
 *                  If no storage is required, pass 0.
 *
 * @return Pointer to new Log-Object, NULL if an Error occured.
 */
extern TagLog *ptagLogC_New_g(int iLogLevel,
                              size_t szMaxEntryLength,
                              unsigned int uiPrefixOptions
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
                              ,TagLogFile *ptagLogFile
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
                              ,size_t szMaxStorageCount
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
                              );

#ifdef __GNUC__
  #define ADDENTRY_TEXT_FORMAT_CHECK __attribute__ ((format (printf, 6, 7)))
#else
  #define ADDENTRY_TEXT_FORMAT_CHECK
#endif

/**
 * Adds a new Logtext to the current log.
 * It's recommended not to use this function directly, use the LOG_TEXT() Macro instead.
 *
 * @param ptagLog    The current Log-Object.
 * @param iLogType   The Type for this entry, @see enum ELogType.
 * @param pcFileName Used for Prefixing the entry.
 * @param iLineNr    Used for Prefixing the entry.
 * @param pcFunction Used for Prefixing the entry.
 * @param pcLogText  The Logtext including format specifiers, @see printf().
 *
 * @return 0 on success, negative value on Error.
 */
extern int iLogC_AddEntry_Text_g(TagLog *ptagLog,
                                 int iLogType,
                                 const char *pcFileName,
                                 int iLineNr,
                                 const char *pcFunction,
                                 const char *pcLogText,
                                 ...)ADDENTRY_TEXT_FORMAT_CHECK;

#if LOGC_OPTVARARG == 1 /* GNUC optional Variadic macro (##__VA_ARGS__) */
  #define LOG_TEXT(log,logtype,txt,...) iLogC_AddEntry_Text_g(log,logtype,__FILE__,__LINE__,LOGC_FUNCTIONNAME,txt,##__VA_ARGS__)
#elif LOGC_OPTVARARG == 2 /* MS-Specific optional Variadic macro (Just __VA_ARGS__) */
  #define LOG_TEXT(log,logtype,txt,...) iLogC_AddEntry_Text_g(log,logtype,__FILE__,__LINE__,LOGC_FUNCTIONNAME,txt,__VA_ARGS__)
#else /* No optional VA-Args available */
  #define LOG_TEXT(log,logtype,...) iLogC_AddEntry_Text_g(log,logtype,__FILE__,__LINE__,LOGC_FUNCTIONNAME,__VA_ARGS__)
#endif /* LOGC_OPTVARARG */

/**
 * Writes pending logs to File if needed and cleans up the Log-Object.
 *
 * @param ptagLog The Log-Object
 *
 * @return 0 on success, negative value on Error.
 */
extern int iLogC_End_g(TagLog *ptagLog);

/**
 * Changes prefix options of the Log-Object after creating it.
 *
 * @param ptagLog The Log-Object
 * @param uiNewPrefixOptions
 *                New Options for the Prefix, @see enum ELogPrefix.
 */
extern void vLogC_SetPrefixOptions_g(TagLog *ptagLog,
                                     unsigned int uiNewPrefixOptions);

#ifdef LOGC_FEATURE_ENABLE_LOGFILE
/**
 * This Function is just available if LOGC_FEATURE_ENABLE_LOGFILE is defined.
 * Force Writing pending Entries to the LogFile,
 * just use this function if you passed 0 as szMaxFileQueueSize.
 * This function will be called on iLogC_End_g() anyway.
 *
 * @param ptagLog The Log-Object
 *
 * @return 0 on success, negative value on Error.
 */
extern int iLogC_WriteEntriesToDisk_g(TagLog *ptagLog);

/**
 * This Function is just available if LOGC_FEATURE_ENABLE_LOGFILE is defined.
 * Change the Logfilepath after creating of the Log-Object.
 * If you pass NULL, no logfile will be written anymore.
 *
 * @param ptagLog   The Log-Object.
 * @param pcNewPath Path for the new Logfile, Length should not exceed 260 Characters.
 *                  NULL means no logfile will be written anymore.
 *
 * @return 0 on success, negative value on Error.
 */
extern int iLogC_SetFilePath_g(TagLog *ptagLog,
                               const char *pcNewPath);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
/**
 * This Function is just available if LOGC_FEATURE_ENABLE_ENTRIES_STORAGE is defined.
 * Returns a Pointer to the next Log-Entry, free after use.
 *
 * @param ptagLog The Log-Object
 * @param pszEntryLength Returns the size of the Text,
 * is Equivalent to strlen()+1. This parameter might be important later.
 * Pass NULL if you don't need the size.
 *
 * @return The Next Log-Entry, or NULL if not available.
 */
extern char *pcLogC_StorageGetNextLog_g(TagLog *ptagLog,
                                        size_t *pszEntryLength);
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

#endif /* LOGC_H_INCLUDED */
