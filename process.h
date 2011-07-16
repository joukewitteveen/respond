/*
 *  fork/thread/execute functionality, terminate routines and exit codes
 */

#ifndef _PROCESS_H
#define _PROCESS_H
#include <cstddef>

// Negatives make sure the codes for SIGINT and SIGTERM remain usable!
#define OK			0
#define DAEMONIZE	-1
#define DIRLOCK		-2
#define EXEC		-3
#define FILER		-4
#define FORK		-5
#define HELP		-6
#define NOACT		-7
#define PIPEC		-8
#define PIPER		-9
#define PIPET		-10
#define SETSID		-11
#define SYNTAX		-12
#define THREADC		-13

void daemonize();
// dirLock moves into the directory of a given file and strips it's path.
int dirLock( char*& );
void setQuitHandlers();
void quit( const int = 0 );

#endif /* _PROCESS_H */
