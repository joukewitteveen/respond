/*
 *  See: process.h
 *
 *  (c) 2007-2014 - Jouke Witteveen
 */

#include <csignal>
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "process.h"
#define	SHELL_PATH	"/bin/sh"	/* Path of the shell.  */
#define	SHELL_NAME	"sh"		/* Name to give it.  */

using namespace std;
typedef void* thread;
typedef void* thread_arg;

extern char* pipeFile;
istream* cinp = &cin;
struct threads_t {
  threads_t() : num( 0 ){}
  unsigned int num;
  pthread_attr_t* attr;
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


void threadsReserve( unsigned int num, unsigned int size ){
  threads.num = num;
  threads.id = new pthread_t[num];
  threads.attr = new pthread_attr_t;
  pthread_attr_init( threads.attr );
  pthread_attr_setdetachstate( threads.attr, PTHREAD_CREATE_JOINABLE );
  pthread_attr_setstacksize( threads.attr, size );
} //threadsReserve


void threadStart( int id, thread ( *func )( thread_arg ), thread_arg arg ){
  if( pthread_create( &threads.id[id], threads.attr, func, arg ) != 0 )
    quit( THREADC );
} //threadStart


void threadsWait(){
  for( unsigned int i = 0; i < threads.num; ++i )
    pthread_join( threads.id[i], NULL );
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


int dirLock( char*& filePath ){
  char* dir = filePath;
  for( char* i = filePath; *i; ++i ) if( *i == '/' ) filePath = i + 1;

  if( dir == filePath ) return 0; //filePath contains no directories

  char c = *filePath;
  *filePath = '\0'; //terminates dir
  int rv = chdir( dir );
  *filePath = c; //restore first letter of filename
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
      cerr << "syntax error" << endl;
      // continue to display help
    case HELP:
      if( cause == HELP ) rv = 0;
      cout << "Usage: respond -a actionscript [ -p named_pipe ]" << endl;
      goto end;
    case 0:
    case SIGINT:
    case SIGTERM:
      rv = 0;
      break;
    case DAEMONIZE:
      rv = 0;
      goto end;
    case EXEC:
      cerr << "error executing another program" << endl;
      _exit( 1 );
    case FILER:
      cerr << "error reading file" << endl;
      break;
    case FORK:
      cerr << "error while forking" << endl;
      break;
    case NOACT:
      cerr << "error: no actions defined" << endl;
      break;
    case PIPEC:
      cerr << "error creating named pipe" << endl;
      goto end;
    case PIPER:
      cerr << "lost connection to pipe" << endl;
      break;
    case PIPET:
      cerr << "error: specified named pipe is not a pipe" << endl;
      break;
    case THREADC:
      cerr << "error creating thread" << endl;
      break;
    default:
      cerr << "unknown error: " << cause << endl;
      break;
  } //switch

  threadsWait();
  if( pipeFile ) unlink( pipeFile );

  end:
  exit( rv );
} //quit
