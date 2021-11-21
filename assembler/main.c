#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <dirent.h>

#include <sys/stat.h>

#define LINE_LENGTH 256

typedef struct Settings {
  const char *filepath;
  const char *outpath;
} Settings;

typedef struct Label {
  const char *name;
  uint16_t value;
} Label;

int64_t last_index_of(const char *parent, const char *child) {
  uint64_t parent_length = strlen(parent);
  uint64_t child_length = strlen(child);

  uint64_t child_index = child_length - 1;

  for (int64_t i = parent_length - 1; i >= 0; i--) {
    if (child[child_index] == parent[i]) {
      if (child_index > 0)
        child_index--;
      else
        return i;
    } else {
      child_index = child_length - 1;
      if (child[child_index] == parent[i]) {
        if (child_index > 0)
          child_index--;
        else
          return i;
      }
    }
  }

  return -1;
}

void read_args(uint32_t argc, char **argv, Settings *settings) {

  bool outpath_flag = false;

  for (uint32_t i = 1; i < argc; i++) {
    if (!outpath_flag && strcmp("-o", argv[i]) == 0) {
      outpath_flag = true;
      continue;
    }

    if (outpath_flag && settings->outpath == NULL) {
      int64_t slash_pos = last_index_of(argv[i], "/");
      outpath_flag = false;
      if (slash_pos == -1) {
        uint64_t length = strlen(argv[i]) + 1;
        char *outpath = (char *)malloc(length * sizeof(char));
        strcpy(outpath, argv[i]);
        outpath[length - 1] = '\0';
        settings->outpath = outpath;
        continue;
      }

      else {
        char buff[PATH_MAX];
        realpath(argv[i], buff);
        uint64_t length = strlen(buff) + 1;
        char *outpath = (char *)malloc(length * sizeof(char));
        strcpy(outpath, buff);
        outpath[length - 1] = '\0';
        settings->outpath = outpath;
        continue;
      }
    }

    if (!outpath_flag && settings->filepath == NULL) {
      int64_t slash_pos = last_index_of(argv[i], "/");

      if (slash_pos == -1) {
        uint64_t length = strlen(argv[i]) + 1;
        char *filepath = (char *)malloc(length * sizeof(char));
        strcpy(filepath, argv[i]);
        filepath[length - 1] = '\0';
        settings->outpath = filepath;
        continue;
      }

      else {
        char buff[PATH_MAX];
        realpath(argv[i], buff);
        uint64_t length = strlen(buff) + 1;
        char *filepath = (char *)malloc(length * sizeof(char));
        strcpy(filepath, buff);
        filepath[length - 1] = '\0';
        settings->filepath = filepath;
        continue;
      }
    }
  }

  if (settings->outpath == NULL) {
    char buff[PATH_MAX];
    realpath("out.hex", buff);
    uint64_t length = strlen(buff) + 1;
    char *outpath = (char *)malloc(length * sizeof(char));
    strcpy(outpath, buff);
    outpath[length - 1] = '\0';
    settings->outpath = outpath;
    fprintf(stderr, "no output file specified, defaulting to %s\n", outpath);
  }

  if (settings->filepath == NULL) {
    fprintf(stderr, "no input file specified\n");
  }
}

int main(int argc, char *argv[]) {

  Settings settings = {NULL, NULL};
  read_args(argc, argv, &settings);

  printf("outpath: %s\nfilepath: %s\n", settings.outpath, settings.filepath);

  FILE *input = fopen(settings.filepath, "r");
  FILE *output = fopen(settings.outpath, "w+");

  bool big_quit = false;
  bool small_quit = false;

  Label labels[LINE_LENGTH];
  uint8_t length = 0;

  while (!big_quit) {
    char line[LINE_LENGTH];
    uint8_t index = 0;

    small_quit = false;

    while (!small_quit && !big_quit) {
      char ch = tolower(fgetc(input));
      if (ch == '\t')
        continue;
      if (ch == '\n') {
        small_quit = true;
        continue;
      }
      if (ch == '\0' || ch == EOF) {
        big_quit = true;
        continue;
      }
      line[index++] = ch;
    }

    if (big_quit)
      break;

    line[index] = '\0';
    printf("%s\n", line);

    // THE MOV COMMAND
    if (last_index_of(line, "mov") != -1) {
      printf("mov detected\n");
      int16_t a_detected = last_index_of(line, "a,");
      int16_t data_detected = last_index_of(line, "#");
      printf("is A: %d\n", a_detected);
      printf("is data: %d\n", data_detected);

      // A and #data
      if (a_detected != -1 && data_detected != -1) {

        // mov A, #data
        if (a_detected < data_detected) {
          fprintf(output, "%c", 0x74);
          uint16_t length = strlen(line);
          bool hexadecimal = last_index_of(line, "h") == length - 1;
          bool binary = last_index_of(line, "b") == length - 1;
          bool decimal = !binary && !hexadecimal;
          uint8_t data_to_send;

          char ch_data[4];
          uint8_t index = 0;
          for (int16_t i = data_detected; i < length; i++) {
            if (isxdigit(line[i]) && !binary)
              ch_data[index++] = line[i];
            else if (binary && (line[i] == '1' || line[i] == '0'))
              ch_data[index++] = line[i];
          }

          ch_data[index] = '\0';

          printf("string data: %s\n", ch_data);

          if (decimal) {
            data_to_send = atoi(ch_data);
            printf("Decimal\n");
          } else if (hexadecimal) {
            data_to_send = strtol(ch_data, NULL, 16);
            printf("Hexadecimal\n");
          } else if (binary) {
            data_to_send = strtol(ch_data, NULL, 2);
            printf("Binary\n");
          }

          printf("actual data: %d\n", data_to_send);
          fprintf(output, "%c", data_to_send);
        }
      }
    }

    printf("\n");
  }

  fclose(input);
  fclose(output);
}
