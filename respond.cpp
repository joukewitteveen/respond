/*
 *  See: respond.h
 *
 *  (c) 2007-2014 - Jouke Witteveen
 */

#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include "respond.h"
#define GETLINE_MAX	256

using namespace std;
typedef void* thread;
typedef void* thread_arg;

extern void execute( char* );
extern thread threadEnd();
extern void threadsReserve( unsigned int, unsigned int );
extern void threadStart( int, thread (*)( thread_arg ), thread_arg );
extern void threadsWait();
extern char* pipeFile;
extern istream* cinp;
char message[GETLINE_MAX];


/*
 * getline reads a line of at most 'size' - 1 characters from 'stream' into
 * 'buffer' and returns the number of characters read or -1 on failure.
 * The stream pointer is set beyond the end of the line.
 */
int getline( istream& stream, char* buffer, const streamsize size ){
  if( stream.eof() ) return 0;
  else if( stream.bad() ) return -1;

  stream.clear();
  stream.getline( buffer, size );
  if( stream.bad() ) return -1;
  else if( stream.fail() && !stream.eof() ){
    stream.clear( stream.rdstate() & ~istream::failbit );
    do{ if( stream.get() == '\n' ) break; } while( stream.good() );
    return size;
  } else return stream.gcount();
} //getline


/*
 * matchThread matches 'message' to the expression of the action pointed to by
 * 'vactionp'. In case of a match the corresponding command is formed and
 * executed.
 */
thread matchThread( thread_arg vactionp ){
  action_t* actionp = (action_t*) vactionp;
  regmatch_t subex[ actionp->re->re_nsub + 1 ];

  if( regexec( actionp->re, message, actionp->re->re_nsub + 1, subex ) == 0 ){
    char* cmd;
    {
      int length = actionp->cmd_cl, c, p, q;
      for( c = 0, p = 0; c < actionp->cmd_cl; ++c, ++p )
        if( actionp->cmd[p] == '\0' ){
          --length, ++p;
          length += subex[(int) actionp->cmd[p]].rm_eo
                    - subex[(int) actionp->cmd[p]].rm_so;
        } //if
      cmd = new char[length + 1];

      for( c = 0, p = 0, q = 0;
           c < actionp->cmd_cl || ( cmd[q] = '\0');
           ++c, ++p, ++q )
        if( ( cmd[q] = actionp->cmd[p] ) == '\0' ){
          ++p, --q;
          for( int s = subex[(int) actionp->cmd[p]].rm_so;
            s < subex[(int) actionp->cmd[p]].rm_eo;
            ++s )
            cmd[++q] = message[s];
        } //if
    }
    execute( cmd );
  } //if

  return threadEnd();
} //matchThread


/***   implementations of previously defined functions   ***/

int listenPipe( char* pipePath, action_t*& actions, const int& actioncount ){
  istream* pipe = ( pipePath ? new ifstream( pipePath, ios::in ) : cinp );
  threadsReserve( actioncount,
                  sizeof( char ) * GETLINE_MAX * GETLINE_MAX / 2
                  + sizeof( int ) * 5
                  + sizeof( regmatch_t ) * ( CHAR_MAX + 1 ) );

  while( pipe->good() )
    while( getline( *pipe, message, GETLINE_MAX ) > 0 ){
      for( int i = 0; i < actioncount; ++i )
        threadStart( i, &matchThread, (thread_arg) &actions[i] );
      threadsWait();
    } //while

  if( pipePath ) ( (ifstream*) pipe )->close();
  return pipe->eof() ? 0 : -1;
} //listenPipe


int preparePipe( char* pipePath ){
  struct stat pipeStat;

  if( stat( pipePath, &pipeStat ) < 0 ){
    pipeFile = pipePath;
    return mkfifo( pipePath, 0600 );
  } else return S_ISFIFO( pipeStat.st_mode ) ? 0 : -1;
} //preparePipe


int readActionFile( char* actionPath, action_t*& actions ){
  char line[GETLINE_MAX];
  int count = 0, i = -1;
  ifstream actionStream( actionPath, ios::in );
  regex_t entry;
  regmatch_t subex[5]; //the match itself plus 4 subexpressions

  if( actionStream.fail() ) return -1;
  regcomp( &entry, "^[[:space:]]*[^#[:space:]]" );
  while( getline( actionStream, line, GETLINE_MAX ) > 0 )
    if( regexec( &entry, line, 0, NULL ) == 0 ) ++count;

  if( count == 0 || actionStream.bad() ) goto end;
  actionStream.clear();
  actionStream.seekg( 0, ios_base::beg );
  actions = new action_t[count];
  regcomp( &entry,
           "^[[:space:]]*"
           "(([^\"#\\\\[:space:]]|\\\\.)+|\"([^\"\\\\]|\\\\.)+\")"
           "[[:space:]]+"
           "(([^\"#\\\\[:space:]]|\\\\.)+|\"([^\"\\\\]|\\\\.)+\")"
           "[[:space:]]*(#|$)" );
  while( getline( actionStream, line, GETLINE_MAX ) > 0 )
    if( regexec( &entry, line, 5, subex ) == 0 ){
      if( ++i >= count ) goto end;
      for( int j = 0, dj = subex[1].rm_so;
           j + dj < subex[1].rm_eo || ( line[j] = '\0' );
           ++j )
        switch( line[j] = line[j + dj] ){
          case '"':
            --j, ++dj;
            break;
          case '\\':
            line[j] = line[j + ++dj];
            break;
        } //switch
      actions[i].re = new regex_t;
      if( regcomp( actions[i].re, line ) != 0
          || actions[i].re->re_nsub > CHAR_MAX ){
        count = -1;
        goto end;
      } //if
      actions[i].cmd_cl = subex[4].rm_eo - subex[4].rm_so;
      actions[i].cmd = new char[actions[i].cmd_cl];
      for( int j = subex[4].rm_so, k = 0, dj; j < subex[4].rm_eo; ++j, ++k )
        switch( actions[i].cmd[k] = line[j] ){
          case '"':
            --k, --actions[i].cmd_cl;
            break;
          case '\\':
            ++j, --actions[i].cmd_cl;
            actions[i].cmd[k] = line[j];
            break;
          case '$':
            for( actions[i].cmd[k + 1] = 0, dj = 1;
                 line[j + dj] >= '0' && line[j + dj] <= '9';
                 ++dj )
              if( actions[i].cmd[k + 1] > ( (int) actions[i].re->re_nsub
                                            - ( line[j + dj] - '0' ) ) / 10 ){
                dj = 1;
                break;
              } else actions[i].cmd[k + 1] = 10 * actions[i].cmd[k + 1]
                                             + ( line[j + dj] - '0' );
            if( --dj )
              j += dj, actions[i].cmd_cl -= dj, actions[i].cmd[k++] = '\0';
            break;
        } //switch
    } //if

  end:
  actionStream.close();
  return i + 1 == count ? count : -1;
} //readActionsFile
