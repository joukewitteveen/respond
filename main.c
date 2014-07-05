/*
 * Respond is an easy way to act upon situations that are reported by a logging
 * system (such as Syslog).
 *
 * (c) 2007-2014 - Jouke Witteveen
 */

#include "process.h"
#include "respond.h"

char* pipeFile = NULL;


int main( int argc, char* const argv[] ) {

  /***   process commandline arguments   ***/
  char *actionPath = NULL, *pipePath = NULL;
  int i;

  for( i = 1; i < argc; ++i ){
    if( argv[i][0] != '-' ) quit( SYNTAX );
    switch( argv[i][1] ){
      case 'h':
      case '?':
        quit( HELP );
      case 'a':
        if( argv[i][2] ) actionPath = argv[i] + 2;
        else if( ++i < argc ) actionPath = argv[i];
        else quit( SYNTAX );
        break;
      case 'p':
        if( argv[i][2] ) pipePath = argv[i] + 2;
        else if( ++i < argc ) pipePath = argv[i];
        else quit( SYNTAX );
        break;
    } //switch
  } //for
  if( !actionPath ) quit( SYNTAX );
  setQuitHandlers();


  /***   preperations (actionscript and named pipe) and daemonize   ***/
  action_t* actions = NULL;
  const int actioncount = readActionFile( actionPath, &actions );

  if( actioncount < 0 ) quit( FILER );
  else if( actioncount == 0 ) quit( NOACT );
  if( pipePath ){
    if( preparePipe( pipePath ) < 0 ) quit( pipeFile ? PIPEC : PIPET );
    daemonize();
    if( dirLock( &pipePath ) < 0 ) quit( DIRLOCK );
    if( pipeFile ) pipeFile = pipePath;
  } //if

  /***   listen on pipe   ***/
  quit( listenPipe( pipePath, &actions, actioncount ) == 0 ? OK : PIPER );
  return 1; //unreachable
} //main
