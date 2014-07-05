/*
 * respond core functionality
 */

#ifndef _RESPOND_H
#define _RESPOND_H
#include <regex.h>
#define regcomp( c, p )	regcomp( c, p, REG_EXTENDED )
#define regexec( c, s, n, m )	regexec( c, s, n, m, 0 )

typedef struct{
  regex_t* re;	//regular expression
  char* cmd;	//command
  int cmd_cl;	//command content length
} action_t;

// listenPipe keeps listening until an error occurs.
int listenPipe( char*, action_t**, const int );
// preparePipe checks or creates a given path to a pipe.
int preparePipe( char* );
// readActionFile reads the relevant data from a file into memory.
int readActionFile( char*, action_t** );

#endif /* _RESPOND_H */
