/*
 *  See: process.h
 *
 *  (c) 2007-2014 - Jouke Witteveen
 */

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "process.h"
#ifndef SHELL_PATH
#define SHELL_PATH	"/bin/sh"	/* Path of the shell. */
#endif /* SHELL_PATH */
#ifndef SHELL_NAME
#define SHELL_NAME	"sh"	/* Name to give it. */
#endif /* SHELL_NAME */

typedef void* thread;
typedef void* thread_arg;

extern char* pipeFile;
struct threads_t {
  pthread_attr_t attr;
  unsigned int num;
  pthread_t* id;
} threads;


void execute( char* cmd ){
  pid_t pid = vfork();
  if( pid < 0 ) quit( FORK );
  else if( pid > 0 ) return;
  execl( SHELL_PATH, SHELL_NAME, "-c", cmd, NULL );
  quit( EXEC );
} //execute


thread threadEnd(){
  pthread_exit( NULL );
} //threadEnd


int threadsReserve( unsigned int num, unsigned int size ){
  if( pthread_attr_init( &threads.attr ) ) return -1;
  threads.num = num;
  threads.id = malloc( num * sizeof( pthread_t ) );
  if( !threads.id
      || pthread_attr_setdetachstate( &threads.attr, PTHREAD_CREATE_JOINABLE )
      || pthread_attr_setstacksize( &threads.attr, size ) ){
    pthread_attr_destroy( &threads.attr );
    return -1;
  }
  return 0;
} //threadsReserve


void threadStart( int id, thread ( *func )( thread_arg ), thread_arg arg ){
  if( pthread_create( &threads.id[id], &threads.attr, func, arg ) != 0 )
    quit( THREADC );
} //threadStart


void threadsWait(){
  unsigned int i;
  for( i = 0; i < threads.num; ++i ) pthread_join( threads.id[i], NULL );
} //threadsWait


/***   implementations of previously defined functions   ***/

void daemonize(){
  if( getppid() != 1 ){ //not already a daemon
    pid_t pid = fork();

    if( pid < 0 ) quit( FORK );
    else if( pid > 0 ) quit( DAEMONIZE ); //seperate the boys from the men
    if( setsid() < 0 ) quit( SETSID );

    freopen( "/dev/null", "r", stdin );
    freopen( "/dev/null", "w", stdout );
    freopen( "/dev/null", "w", stderr );

    // chdir remains needed (is done in dirLock())
  } //if
} //daemonize


int dirLock( char** filePath ){
  char *dir = *filePath, *pos = *filePath;
  int rv;
  while( *pos ) if( *pos++ == '/' ) *filePath = pos;

  if( dir == *filePath ) return 0; //filePath contains no directories

  *pos = **filePath;
  **filePath = '\0'; //terminates dir
  rv = chdir( dir );
  **filePath = *pos; //restore first letter of filename
  *pos = '\0';
  return rv;
} //dirLock


void setQuitHandlers(){
  if( signal( SIGINT, &quit ) == SIG_IGN ) signal( SIGINT, SIG_IGN );
  if( signal( SIGTERM, &quit ) == SIG_IGN ) signal( SIGTERM, SIG_IGN );
} //setQuitHandlers


void quit( const int cause ){
  int rv = 1;
  switch( cause ){
    case SYNTAX:
      fputs( "syntax error\n", stderr );
      // continue to display help
    case HELP:
      if( cause == HELP ) rv = EXIT_SUCCESS;
      puts( "Usage: respond -a actionscript [ -p named_pipe ]" );
      goto end;
    case OK:
    case SIGINT:
    case SIGTERM:
      rv = EXIT_SUCCESS;
      break;
    case DAEMONIZE:
      rv = EXIT_SUCCESS;
      goto end;
    case EXEC:
      fputs( "error executing another program\n", stderr );
      _exit( EXIT_FAILURE );
    case FILER:
      fputs( "error reading actionscript\n", stderr );
      break;
    case FORK:
      fputs( "error while forking\n", stderr );
      break;
    case NOACT:
      fputs( "error: no actions defined\n", stderr );
      break;
    case PIPEC:
      fputs( "error creating named pipe\n", stderr );
      goto end;
    case PIPER:
      fputs( "lost connection to pipe\n", stderr );
      break;
    case PIPET:
      fputs( "error: specified named pipe is not a pipe\n", stderr );
      break;
    case THREADC:
      fputs( "error creating thread\n", stderr );
      break;
    default:
      fprintf( stderr, "unknown error: %i\n", cause );
      break;
  } //switch

  threadsWait();
  if( pipeFile ) unlink( pipeFile );

  end:
  exit( rv );
} //quit
