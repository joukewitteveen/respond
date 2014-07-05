/*
 *  See: respond.h
 *
 *  (c) 2007-2014 - Jouke Witteveen
 */

#define _WITH_GETLINE
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "respond.h"
// More than enough for commands of _POSIX_ARG_MAX characters.
#define EXEC_STACK	65536

typedef void* thread;
typedef void* thread_arg;

extern void execute( char* );
extern thread threadEnd();
extern int threadsReserve( unsigned int, unsigned int );
extern void threadStart( int, thread (*)( thread_arg ), thread_arg );
extern void threadsWait();
extern char* pipeFile;

char* buffer = NULL;
size_t bufferSize = 0;


/*
 * matchThread matches 'buffer' to the expression of the action pointed to by
 * 'vactionp'. In case of a match the corresponding command is formed and
 * executed.
 */
thread matchThread( thread_arg vactionp ){
  action_t* actionp = (action_t*) vactionp;
  regmatch_t subex[ actionp->re->re_nsub + 1 ];

  if( regexec( actionp->re, buffer, actionp->re->re_nsub + 1, subex ) == 0 ){
    char* cmd;
    {
      int length = actionp->cmd_cl, c, p, q, s;
      for( c = 0, p = 0; c < actionp->cmd_cl; ++c, ++p )
        if( actionp->cmd[p] == '\0' ){
          --length, ++p;
          length += subex[(int) actionp->cmd[p]].rm_eo
                    - subex[(int) actionp->cmd[p]].rm_so;
        } //if
      cmd = malloc( (length + 1) * sizeof( char ) );
      if( !cmd ) threadEnd(); //unlikely because of the reserved stack size

      for( c = 0, p = 0, q = 0;
           c < actionp->cmd_cl || ( cmd[q] = '\0' );
           ++c, ++p, ++q )
        if( ( cmd[q] = actionp->cmd[p] ) == '\0' ){
          ++p, --q;
          for( s = subex[(int) actionp->cmd[p]].rm_so;
               s < subex[(int) actionp->cmd[p]].rm_eo;
               ++s )
            cmd[++q] = buffer[s];
        } //if
    }
    execute( cmd );
    free( cmd );
  } //if

  return threadEnd();
} //matchThread


/***   implementations of previously defined functions   ***/

int listenPipe( char* pipePath, action_t** actions, const int actioncount ){
  int i;
  ssize_t n;
  FILE* pipe;

  if( threadsReserve( actioncount, EXEC_STACK ) < 0 ) return -1;

  pipe = pipePath ? fopen( pipePath, "r" ) : stdin;
  if( !pipe ) return -1;
  while( ( n = getline( &buffer, &bufferSize, pipe ) ) >= 0 ){
    if( n > 0 && buffer[n - 1] == '\n' ) buffer[n - 1] = '\0';
    for( i = 0; i < actioncount; ++i )
      threadStart( i, &matchThread, (thread_arg) &( *actions )[i] );
    threadsWait();
    if( n > LINE_MAX ) {
      free( buffer );
      buffer = NULL;
      bufferSize = 0;
    }
  } //while

  i = feof( pipe );
  if( pipePath ) fclose( pipe );
  return i ? 0 : -1;
} //listenPipe


int preparePipe( char* pipePath ){
  struct stat pipeStat;

  if( stat( pipePath, &pipeStat ) < 0 ){
    pipeFile = pipePath;
    return mkfifo( pipePath, 0600 );
  } else return S_ISFIFO( pipeStat.st_mode ) ? 0 : -1;
} //preparePipe


int readActionFile( char* actionPath, action_t** actions ){
  int count = 0, i = -1, j, dj, k;
  regex_t entry;
  regmatch_t subex[5]; //the match itself plus 4 subexpressions
  FILE* actionStream = fopen( actionPath, "r" );
  if( !actionStream ) return -1;

  regcomp( &entry, "^[[:space:]]*[^#[:space:]]" );
  while( getline( &buffer, &bufferSize, actionStream ) >= 0 )
    if( regexec( &entry, buffer, 0, NULL ) == 0 ) ++count;

  if( count == 0 || !feof( actionStream ) ) goto end;
  rewind( actionStream );
  *actions = malloc( count * sizeof( action_t ) );
  if( !*actions ) goto end;
  regcomp( &entry,
           "^[[:space:]]*"
           "(([^\"#\\\\[:space:]]|\\\\.)+|\"([^\"\\\\]|\\\\.)+\")"
           "[[:space:]]+"
           "(([^\"#\\\\[:space:]]|\\\\.)+|\"([^\"\\\\]|\\\\.)+\")"
           "[[:space:]]*(#|$)" );
  while( getline( &buffer, &bufferSize, actionStream ) >= 0 )
    if( regexec( &entry, buffer, 5, subex ) == 0 ){
      if( ++i >= count ) goto end;
      for( j = 0, dj = subex[1].rm_so;
           j + dj < subex[1].rm_eo || ( buffer[j] = '\0' );
           ++j )
        switch( buffer[j] = buffer[j + dj] ){
          case '"':
            --j, ++dj;
            break;
          case '\\':
            buffer[j] = buffer[j + ++dj];
            break;
        } //switch
      ( *actions )[i].re = malloc( sizeof( regex_t ) );
      if( !( *actions )[i].re ){
        count = 0;
        goto end;
      }
      if( regcomp( ( *actions )[i].re, buffer ) != 0
          || ( *actions )[i].re->re_nsub > CHAR_MAX ){
        free( ( *actions )[i].re );
        count = 0;
        goto end;
      } //if
      ( *actions )[i].cmd_cl = subex[4].rm_eo - subex[4].rm_so;
      ( *actions )[i].cmd = malloc( ( *actions )[i].cmd_cl * sizeof( char ) );
      if( !( *actions )[i].cmd ){
        free( ( *actions )[i].re );
        count = 0;
        goto end;
      }
      for( j = subex[4].rm_so, k = 0; j < subex[4].rm_eo; ++j, ++k )
        switch( ( *actions )[i].cmd[k] = buffer[j] ){
          case '"':
            --k, --( *actions )[i].cmd_cl;
            break;
          case '\\':
            ++j, --( *actions )[i].cmd_cl;
            ( *actions )[i].cmd[k] = buffer[j];
            break;
          case '$':
            for( ( *actions )[i].cmd[k + 1] = 0, dj = 1;
                 buffer[j + dj] >= '0' && buffer[j + dj] <= '9';
                 ++dj )
              if( ( *actions )[i].cmd[k + 1]
                  > ( (int) ( *actions )[i].re->re_nsub
                      - ( buffer[j + dj] - '0' ) ) / 10 ){
                dj = 1;
                break;
              } else
                ( *actions )[i].cmd[k + 1] =
                  10 * ( *actions )[i].cmd[k + 1] + ( buffer[j + dj] - '0' );
            if( --dj )
              j += dj,
              ( *actions )[i].cmd_cl -= dj,
              ( *actions )[i].cmd[k++] = '\0';
            break;
        } //switch
    } //if

  end:
  fclose( actionStream );
  free( buffer );
  buffer = NULL;
  bufferSize = 0;
  if( i + 1 != count ){
    while( --i >= 0 ){
      free( ( *actions )[i].re );
      free( ( *actions )[i].cmd );
    }
    free( *actions );
    return -1;
  } else return count;
} //readActionsFile
