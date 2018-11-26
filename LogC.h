#ifndef LOGC_H_INCLUDED
  #define LOGC_H_INCLUDED

#include <stdarg.h>
#include <limits.h>

#define LOGC_FEATURE_ENABLE_LOGFILE       /* Enable this option if you want to log to a file */
/* #define LOGC_FEATURE_ENABLE_LOG_STORAGE   /* Enable this option if you want to store logs */
#define LOGC_FEATURE_ENABLE_THREADSAFETY  /* Enable this for making safe for use within multithreaded Applications */

#define LOGC_LIBRARY_DEBUG

/**
 * This are the available logtypes for each entry.
 * Edit as you need, also in LogC.c!
 */
enum LogCType
{
  LOGC_ALL        =0,
  LOGC_DEBUG_MORE =100,
  LOGC_DEBUG      =200,
  LOGC_INFO       =300,
  LOGC_WARNING    =1000,
  LOGC_ERROR      =1100,
  LOGC_FATAL      =1200,
  LOGC_NONE       =INT_MAX
};

/**
 * Use these Options to configure the log as you need it.
 */
enum LogCOptions
{
  /* Timestamp, with year, month and day, e.g. 2018-01-02 */
  LOGC_OPTION_PREFIX_TIMESTAMP_DATE           =0x1,
  /* Timestamp with the current time, e.g. 12:34:56 */
  LOGC_OPTION_PREFIX_TIMESTAMP_TIME           =0x2,
  /* Current Milliseconds, e.g. .0123 */
  LOGC_OPTION_PREFIX_TIMESTAMP_TIME_MILLISECS =0x4,
  /* Add the logtype as text prefix, e.g. [Error] */
  LOGC_OPTION_PREFIX_LOGTYPETEXT              =0x10,
  /* Adds Fileinfo to the Prefix of each entry, e.g. myfile.c@line 11 */
  LOGC_OPTION_PREFIX_FILEINFO                 =0x20,
  /* Add Functionname for each entry using the __func__ macro. */
  LOGC_OPTION_PREFIX_FUNCTIONNAME             =0x40,

  /* Keeps the logs timestamp in UTC (this is the default behaviour if nothing is specified) */
  LOGC_OPTION_TIMESTAMP_UTC                   =0x100,
  /* Sets the timestamp to localtime */
  LOGC_OPTION_TIMESTAMP_LOCALTIME             =0x200,

  /* Don't print logtypes which are linked to stdout, see TagLogTypes in LogC.h */
  LOGC_OPTION_IGNORE_STDOUT                   =0x1000,
  /* Don't print logtypes which are linked to stderr, see TagLogTypes in LogC.h */
  LOGC_OPTION_IGNORE_STDERR                   =0x2000,

#ifdef LOGC_FEATURE_ENABLE_THREADSAFETY
  /**
   * Makes the Interface Threadsafe for the choosen Log-Object.
   * Just needed if you want to access the same Log-Object from diffrent Threads.
   */
  LOGC_OPTION_THREADSAFE                      =0x8000,
#endif /* LOGC_FEATURE_ENABLE_THREADSAFETY */
};

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
  #define LOGC_STORAGE_MAX SIZE_MAX
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

/* Check for optional variadic macro extension (##__VA_ARGS__ in GCC) */
#if defined(_doxygen) || defined(__GNUC__) || defined(__CC_ARM) /* additional ## modifier is used to supress the ',' if no additional arg is passed. */
  #define LOGC_OPTVARARG 1
#elif defined(__MSC_VER) /* In MSC, __VA_ARGS__ will supress the ',' automatically if no arg is passed */
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

typedef struct TagLog_t* LogC;

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
}LogCFile;
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

/**
 * Creates a new Log-Object
 *
 * @param logLevel Define from which level on the logs should be added, @see enum LogCType.
 * @param maxEntryLength
 *                  The maximum Textlength for a single Entry, including prefix. Longer Text will be truncated.
 *                  A newline will be added at the end of the text automatically, not counting to this size.
 *                  The real size of one entry can be this value +2 (+'\n'+'\0').
 *                  If this value is too small for the choosen prefix, LogC_AddEntry_Text() will fail.
 * @param logOptions
 *                  Options for logging, @see enum LogCOptions
 * @param logFile
 *                  Just available if LOGC_FEATURE_ENABLE_LOGFILE is defined, contains info for logging to a file.
 *                  If no file is required, pass NULL.
 * @param maxStorageCount
 *                  Just available if LOGC_FEATURE_ENABLE_LOG_STORAGE is defined.
 *                  Pass the ammount of maximum entries to be stored, LOGC_STORAGE_MAX for maximum.
 *                  If no storage is required, pass 0.
 *
 * @return New Log-Object, NULL if an Error occured.
 */
extern LogC LogC_New(int logLevel,
                     size_t maxEntryLength,
                     unsigned int logOptions
#ifdef LOGC_FEATURE_ENABLE_LOGFILE
                     ,LogCFile *logFile
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */
#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
                     ,size_t maxStorageCount
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */
                     );

#ifdef __GNUC__
  #define PRINTF_FORMAT_CHECK __attribute__ ((format (printf, 6, 7)))
#else
  #define PRINTF_FORMAT_CHECK
#endif

/**
 * Adds a new Logtext to the current log.
 * It's recommended not to use this function directly, use the LOG_TEXT() Macro instead.
 *
 * @param log      The current Log-Object.
 * @param logType  The Type for this entry, @see enum ELogType.
 * @param fileName Used for Prefixing the entry.
 * @param lineNr   Used for Prefixing the entry.
 * @param functionName
 *                 Used for Prefixing the entry.
 * @param logText  The Logtext including format specifiers, @see printf().
 *
 * @return 0 on success, negative value on Error.
 */
extern int LogC_AddEntry_Text(LogC log,
                              int logType,
                              const char *fileName,
                              int lineNr,
                              const char *functionName,
                              const char *logText,
                              ...)PRINTF_FORMAT_CHECK;

#if LOGC_OPTVARARG == 1 /* GNUC optional Variadic macro (##__VA_ARGS__) */
  #define LOG_TEXT(log,logtype,txt,...) LogC_AddEntry_Text(log,logtype,__FILE__,__LINE__,LOGC_FUNCTIONNAME,txt,##__VA_ARGS__)
#elif LOGC_OPTVARARG == 2 /* MS-Specific optional Variadic macro (Just __VA_ARGS__) */
  #define LOG_TEXT(log,logtype,txt,...) LogC_AddEntry_Text(log,logtype,__FILE__,__LINE__,LOGC_FUNCTIONNAME,txt,__VA_ARGS__)
#else /* No optional varArgs available */
  #define LOG_TEXT(log,logtype,...) LogC_AddEntry_Text(log,logtype,__FILE__,__LINE__,LOGC_FUNCTIONNAME,__VA_ARGS__)
#endif /* LOGC_OPTVARARG */

/**
 * Writes pending logs to File if needed and cleans up the Log-Object.
 *
 * @param log The Log-Object
 *
 * @return 0 on success, negative value on Error.
 */
extern int LogC_End(LogC log);

/**
 * Changes prefix options of the Log-Object after creating it.
 *
 * @param log The Log-Object
 * @param newPrefixFormat
 *                New Options for the Prefix, @see enum LogCOptions,
 *                Use the LOGC_OPTION_PREFIX_XXX Options.
 *                This will reset the current format options and apply these ones.
 */
extern int LogC_SetPrefixFormat(LogC log,
                                unsigned int newPrefixFormat);


/**
 * Set other log options than prefix format.
 * It's not possible to set LOGC_OPTION_THREADSAFE here, only initially while creating a new log.
 *
 * @param log
 * @param newOptions
 *
 * @return
 */
extern int LogC_SetLogOptions(LogC log,
                              unsigned int newOptions);


#ifdef LOGC_FEATURE_ENABLE_LOGFILE
/**
 * This Function is just available if LOGC_FEATURE_ENABLE_LOGFILE is defined.
 * Force Writing pending Entries to the LogFile,
 * just use this function if you passed 0 as szMaxFileQueueSize.
 * This function will be called on iLogC_End_g() anyway.
 *
 * @param log The Log-Object
 *
 * @return 0 on success, negative value on Error.
 */
extern int LogC_WriteEntriesToDisk(LogC log);

/**
 * This Function is just available if LOGC_FEATURE_ENABLE_LOGFILE is defined.
 * Change the Logfilepath after creating of the Log-Object.
 * If you pass NULL, no logfile will be written anymore.
 *
 * @param log       The Log-Object.
 * @param newPath Path for the new Logfile, Length should not exceed 260 Characters.
 *                  NULL means no logfile will be written anymore.
 *
 * @return 0 on success, negative value on Error.
 */
extern int LogC_SetFilePath(LogC log,
                            const char *newPath);
#endif /* LOGC_FEATURE_ENABLE_LOGFILE */

#ifdef LOGC_FEATURE_ENABLE_LOG_STORAGE
/**
 * This Function is just available if LOGC_FEATURE_ENABLE_ENTRIES_STORAGE is defined.
 * Returns a Pointer to the next Log-Entry, free after use.
 *
 * @param log            The Log-Object
 * @param entryLength Returns the size of the Text,
 * is Equivalent to strlen()+1. This parameter might be important later.
 * Pass NULL if you don't need the size.
 *
 * @return The Next Log-Entry, or NULL if not available.
 */
extern char *LogC_StorageGetNextLog(LogC log,
                                    size_t *entryLength);
#endif /* LOGC_FEATURE_ENABLE_LOG_STORAGE */

#endif /* LOGC_H_INCLUDED */



