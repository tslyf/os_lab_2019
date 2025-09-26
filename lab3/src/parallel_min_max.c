#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) {
  int seed = -1;
  int array_size = -1;
  int pnum = -1;
  bool with_files = false;

  while (true) {
    int current_optind = optind ? optind : 1;

    static struct option options[] = {{"seed", required_argument, 0, 0},
                                      {"array_size", required_argument, 0, 0},
                                      {"pnum", required_argument, 0, 0},
                                      {"by_files", no_argument, 0, 'f'},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "f", options, &option_index);

    if (c == -1) break;

    switch (c) {
      case 0:
        switch (option_index) {
          case 0:
            seed = atoi(optarg);
            if (seed <= 0) {
              printf("seed is a positive number\n");
              return 1;
            }
            break;
          case 1:
            array_size = atoi(optarg);
            if (array_size <= 0) {
              printf("array_size is a positive number\n");
              return 1;
            }
            break;
          case 2:
            pnum = atoi(optarg);
            if (pnum <= 0) {
              printf("pnum is a positive number\n");
              return 1;
            }
            break;
          case 3:
            with_files = true;
            break;
          default:
            printf("Index %d is out of options\n", option_index);
        }
        break;
      case 'f':
        with_files = true;
        break;

      case '?':
        break;

      default:
        printf("getopt returned character code 0%o?\n", c);
    }
  }

  if (optind < argc) {
    printf("Has at least one no option argument\n");
    return 1;
  }

  if (seed == -1 || array_size == -1 || pnum == -1) {
    printf("Usage: %s --seed \"num\" --array_size \"num\" --pnum \"num\" [--by_files]\n", argv[0]);
    return 1;
  }

  int *array = malloc(sizeof(int) * array_size);
  GenerateArray(array, array_size, seed);
  int active_child_processes = 0;
  int pipefd[pnum][2]; // Массив pipe для каждого процесса

  struct timeval start_time;
  gettimeofday(&start_time, NULL);

  for (int i = 0; i < pnum; i++) {
    if (!with_files) {
        if (pipe(pipefd[i]) == -1) {
            perror("pipe failed");
            return 1;
        }
    }

    pid_t child_pid = fork();
    if (child_pid >= 0) {
      // successful fork
      active_child_processes += 1;
      if (child_pid == 0) {
        int begin = i * (array_size / pnum);
        int end = (i == pnum - 1) ? array_size : (i + 1) * (array_size / pnum);
        struct MinMax local_min_max = GetMinMax(array, begin, end);

        if (with_files) {
          char filename[32];
          sprintf(filename, "temp_file_%d.txt", getpid());
          FILE *fp = fopen(filename, "w");
          fprintf(fp, "%d %d", local_min_max.min, local_min_max.max);
          fclose(fp);
        } else {
          close(pipefd[i][0]); // закрываем чтение в доч.
          write(pipefd[i][1], &local_min_max, sizeof(struct MinMax));
          close(pipefd[i][1]);
        }
        exit(0); // выходим из доч.
      }
      // Родитель
      if (!with_files) {
          close(pipefd[i][1]); // закрываем запись в родителе
      }

    } else {
      printf("Fork failed!\n");
      return 1;
    }
  }

  pid_t child_pids[pnum];
  int status;
  for(int i = 0; i < pnum; i++){
      child_pids[i] = wait(&status); // ждем завершения доч.процессов
  }
  
  struct MinMax min_max;
  min_max.min = INT_MAX;
  min_max.max = INT_MIN;

  for (int i = 0; i < pnum; i++) {
    int min = INT_MAX;
    int max = INT_MIN;

    if (with_files) {
      // read from files
      char filename[32];
      sprintf(filename, "temp_file_%d.txt", child_pids[i]);
      FILE *fp = fopen(filename, "r");
      fscanf(fp, "%d %d", &min, &max);
      fclose(fp);
      remove(filename);
    } else {
      // read from pipes
      struct MinMax local_min_max;
      read(pipefd[i][0], &local_min_max, sizeof(struct MinMax));
      min = local_min_max.min;
      max = local_min_max.max;
      close(pipefd[i][0]);
    }

    if (min < min_max.min) min_max.min = min;
    if (max > min_max.max) min_max.max = max;
  }

  struct timeval finish_time;
  gettimeofday(&finish_time, NULL);

  double elapsed_time = (finish_time.tv_sec - start_time.tv_sec) * 1000.0;
  elapsed_time += (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

  free(array);

  printf("Min: %d\n", min_max.min);
  printf("Max: %d\n", min_max.max);
  printf("Elapsed time: %fms\n", elapsed_time);
  fflush(NULL);
  return 0;
}
