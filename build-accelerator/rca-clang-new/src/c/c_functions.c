#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

int convert_path(char *const argv[], char* args[]) {
    // get the time stamp
    struct timeval tv;
    gettimeofday(&tv,NULL);
    unsigned long time_in_micros = 1000000 * tv.tv_sec + tv.tv_usec;

    // NB: argv[0] is a duplicate of pathname, so we start at index 1
    int arg_count = 10;

    // print the clang invocation command
    for (int i = 0; argv[i] != NULL; i++) {
      arg_count++;
      fprintf(stderr, " %s", argv[i]);
    }
    fprintf(stderr,"\n");

    if (args==NULL){
      args = malloc(sizeof(char*)*arg_count);
    }

    args[0] = "/usr/bin/perf";
    args[1] = "record";
    args[2] = "-e";
    args[3] = "cycles:u";
    args[4] = "-j";
    args[5] = "any,u";
    args[6] = "-o";
    args[7] = malloc(sizeof(char)*64);
    sprintf(args[7], "/home.local/zyuxuan/tmp_rust/perf%ld.data", time_in_micros);
    args[8] = "--";
    args[9] = malloc(sizeof(char)*(strlen(argv[0])+1));
    bzero(args[9], sizeof(char)*(strlen(argv[0])+1));
    strncpy(args[9], argv[0], strlen(argv[0]));
    args[9][strlen(argv[0])] = '\0';

    char* perf = "/usr/bin/perf";

    int last_idx = 9;

    for (int i = 1; argv[i] != NULL; i++) {
      args[i+9] = malloc(sizeof(char)*(strlen(argv[i])+1));
      bzero(args[i+9], sizeof(char)*(strlen(argv[i])+1));
      strncpy(args[i+9], argv[i], strlen(argv[i]));
      args[i+9][strlen(argv[i])] = '\0';
      last_idx++;
    }

    args[last_idx+1] = NULL;
    // print the clang invocation command
    for (int i = 0; args[i] != NULL; i++) {
      fprintf(stderr, " %s", args[i]);
    }
    fprintf(stderr,"\n");
    return 0;
}

int convert_envp(char *const envp[], char* envp_new[]) {
  int arg_count = 0;
  // print the clang invocation command
  for (int i = 0; envp[i] != NULL; i++) {
    arg_count++;
  }

  int j = 0;
  for (int i = 0; envp[i] != NULL; i++) {
    if (strncmp(envp[i],"LD_PRELOAD", 10)){
      envp_new[j] = malloc(sizeof(char)*strlen(envp[i]));
      strncpy(envp_new[j], envp[i], strlen(envp[i]));
      j++;    
    }
  }
  envp_new[j] = NULL;
  fprintf(stderr,"\n");



  return 0;
}
