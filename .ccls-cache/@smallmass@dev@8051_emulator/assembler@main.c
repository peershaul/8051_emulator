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

#define MOV_COMMAND 0x1
#define CPL_COMMAND 0x2
#define NOP_COMMAND 0x3
#define AJMP_COMMAND 0x4
#define LJMP_COMMAND 0x5
#define RR_COMMAND 0x6
#define INC_COMMAND 0x7
#define JBC_COMMAND 0x8
#define ACALL_COMMAND 0x9
#define LCALL_COAMMAND 0xa
#define RRC_COMMAND 0xb
#define DEC_COMMAND 0xc
#define JB_COMMAND 0xd
#define RET_COMMAND 0xe
#define RL_COMMAND 0xf
#define ADD_COMMAND 0x10
// TODO Continue this shit 

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

int64_t index_of_ch(const char *parent, char child, uint64_t offset) {
  int64_t index = offset;
  while (parent[index] != '\0') {
    if (parent[index] == child)
      return index;
    index++;
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

uint8_t find_command(const char *command_string) {

  if (last_index_of(command_string, "mov") != -1)
    return MOV_COMMAND;

  if (last_index_of(command_string, "cpl") != -1)
    return CPL_COMMAND;

  return 0;
}

void get_command_arguments(char **arg_outs, uint8_t n_of_commas,
                           uint8_t space_place, char *line, int8_t *comma_pos) {
  uint8_t length = strlen(line);

  switch (n_of_commas) {
  case 0: {
    char *arg = (char *)malloc((length - space_place) * sizeof(char));
    for (uint8_t i = space_place + 1; i < length; i++)
      arg[i - space_place - 1] = line[i];

    arg[length - space_place - 1] = '\0';
    arg_outs[0] = arg;
    return;
  }
  case 1: {
    char *arg = (char *)malloc((comma_pos[0] - space_place) * sizeof(char));
    for (uint8_t i = space_place + 1; i < comma_pos[0]; i++)
      arg[i - space_place - 1] = line[i];
    arg[comma_pos[0] - space_place] = '\0';
    arg_outs[0] = arg;

    arg = (char *)malloc((length - comma_pos[0] - 1) * sizeof(char));
    for (uint8_t i = comma_pos[0] + 1; i < length; i++)
      arg[i - comma_pos[0] - 2] = line[i];

    arg[length - comma_pos[0] - 1] = '\0';
    arg_outs[1] = arg;
    return;
  }
  case 2: {
    char *arg = (char *)malloc((comma_pos[0] - space_place) * sizeof(char));
    for (uint8_t i = space_place + 1; i < comma_pos[0]; i++)
      arg[i - space_place - 1] = line[i];
    arg[comma_pos[0] - space_place] = '\0';
    arg_outs[0] = arg;

    arg = (char *)malloc((comma_pos[1] - comma_pos[0] - 1) * sizeof(char));
    for (uint8_t i = comma_pos[0] + 1; i < comma_pos[1]; i++)
      arg[i - comma_pos[0] - 2] = line[i];

    arg[length - comma_pos[0] - 1] = '\0';
    arg_outs[1] = arg;

    arg = (char *)malloc((length - comma_pos[1] - 1) * sizeof(char));
    for (uint8_t i = comma_pos[1] + 1; i < length; i++)
      arg[i - comma_pos[1] - 2] = line[i];

    arg[length - comma_pos[1] - 1] = '\0';
    arg_outs[2] = arg;
    return;
  }
  }
}

uint16_t number_from_immideate(const char *arg, uint8_t arg_length,
                               bool is_immideate) {
  char data[arg_length - is_immideate];
  bool hexadecimal = arg[arg_length - 1] == 'h';
  bool binary = arg[arg_length - 1] == 'b';

  if (!hexadecimal && !binary) {
    for (uint i = is_immideate; i < arg_length; i++)
      data[i - is_immideate] = arg[i];

    return atoi(data);
  } else {
    for (uint i = is_immideate; i < arg_length - 1; i++)
      data[i - is_immideate] = arg[i];

    return strtol(data, NULL, binary ? 2 : 16);
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
  uint16_t line_number = 0;

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

    int8_t first_space = index_of_ch(line, ' ', 0);
    char command_ch[first_space + 1];

    for (uint8_t i = 0; i < first_space; i++)
      command_ch[i] = line[i];

    command_ch[first_space] = '\0';

    printf("command: \"%s\"\n", command_ch);
    uint8_t command = find_command(command_ch);

    printf("command found: %x\n", command);

    int8_t comma_pos[2] = {
        index_of_ch(line, ',', first_space),
        comma_pos[0] == -1 ? -1 : index_of_ch(line, ',', comma_pos[0] + 1)};

    uint8_t n_of_commas = 0;
    if (comma_pos[0] != -1) {
      n_of_commas = 1;
      if (comma_pos[1] != -1)
        n_of_commas = 2;
    }

    printf("number of commas: %d\n", n_of_commas);

    char *args[3] = {};
    uint8_t arg_lengths[3] = {};

    get_command_arguments(args, n_of_commas, first_space, line, comma_pos);
    printf("args:\n");
    for (uint8_t i = 0; i <= n_of_commas; i++) {
      printf("\t%d: \"%s\"\n", i + 1, args[i]);
      arg_lengths[i] = strlen(args[i]);
    }

    switch (command) {
    case MOV_COMMAND: {

      // mov A, #immediate
      if (arg_lengths[0] == 1 && args[0][0] == 'a' && args[1][0] == '#') {
        uint8_t data = number_from_immideate(args[1], arg_lengths[1], true);
        printf("\nmov A, #%x\n", data);
        printf("74, %2x\n", data);
        fprintf(output, "%c%c", 0x74, data);
        line_number += 2;
      }

      // mov A, Ri
      else if (arg_lengths[0] == 1 && args[0][0] == 'a' && args[1][0] == 'r') {
        uint8_t rindex = atoi(&args[1][1]);
        if (rindex > 7)
          return -1;
        printf("\nmov A, R%d\n", rindex);
        uint8_t opcode = 0xe8 + rindex;
        printf("%x\n", opcode);
        fprintf(output, "%c", opcode);
        line_number++;
      }

      // mov A
      else if (arg_lengths[0] == 1 && args[0][0] == 'a' && args[1][0] == '@') {
        uint8_t rindex = atoi(&args[1][2]);
        if (rindex > 1)
          return -1;
        uint8_t opcode = 0xf6 + rindex;

        printf("\nmov @R%d, A\n", rindex);
        printf("%x\n", opcode);
        fprintf(output, "%c", opcode);
        line_number++;
      }

      // mov A, ram_addr
      else if (arg_lengths[0] == 1 && args[0][0] == 'a') {
        uint8_t data = number_from_immideate(args[1], arg_lengths[1], false);
        printf("\nmov A, %x\n", data);
        printf("e5, %2x\n", data);
        fprintf(output, "%c%c", 0xe5, data);
        line_number += 2;
      }

      // mov A, @Ri
      else if (arg_lengths[1] == 1 && args[0][0] == 'r' && args[1][0] == 'a') {
        uint8_t rindex = atoi(&args[0][1]);
        if (rindex > 7)
          return -1;
        printf("\nmov A, @R%d\n", rindex);
        uint8_t opcode = 0xe6 + rindex;
        printf("%x\n", opcode);
        fprintf(output, "%c", opcode);
        line_number++;
      }

      // mov Ri, #immideate
      else if (args[0][0] == 'r' && args[1][0] == '#') {
        uint8_t rindex = atoi(&args[0][1]);
        if (rindex > 7)
          return -1;
        uint8_t data = number_from_immideate(args[1], arg_lengths[1], true);
        uint8_t opcode = 0x78 + rindex;
        printf("\nmov R%d, #%xh\n", rindex, data);
        printf("%x, %2x\n", opcode, data);
        fprintf(output, "%c%c", opcode, data);
        line_number += 2;
      }

      // mov Ri, ram_addr
      else if (args[0][0] == 'r') {
        uint8_t rindex = atoi(&args[0][1]);
        if (rindex > 7)
          return -1;
        uint8_t data = number_from_immideate(args[1], arg_lengths[1], false);
        uint8_t opcode = 0xa8 + rindex;
        printf("\nmov R%d, %xh\n", rindex, data);
        printf("%x, %2x\n", opcode, data);
        fprintf(output, "%c%c", opcode, data);
        line_number += 2;
      }

      // mov @Ri, A
      else if (args[0][0] == '@' && args[1][0] == 'a' && arg_lengths[1] == 1) {
        uint8_t rindex = atoi(&args[0][2]);
        if (rindex > 1)
          return -1;
        uint8_t opcode = 0xa6 + rindex;

        printf("\nmov @R%d, A\n", rindex);
        printf("%x\n", opcode);
        fprintf(output, "%c", opcode);
        line_number++;
      }

      // mov @Ri, #immediate
      else if (args[0][0] == '@' && args[1][0] == '#') {
        uint8_t rindex = atoi(&args[0][2]);
        if (rindex > 1)
          return -1;
        uint8_t opcode = 0x76 + rindex;
        uint8_t data = number_from_immideate(args[1], arg_lengths[1], true);

        printf("\nmov @R%d, #%XH\n", rindex, data);
        printf("%x, %2x\n", opcode, data);
        fprintf(output, "%c%c", opcode, data);
        line_number += 2;
      }

      // mov @Ri, ram_addr
      else if (args[0][0] == '@') {
        uint8_t rindex = atoi(&args[0][2]);
        if (rindex > 1)
          return -1;
        uint8_t opcode = 0xa6 + rindex;
        uint8_t data = number_from_immideate(args[1], arg_lengths[1], false);

        printf("\nmov @R%d, %XH\n", rindex, data);
        printf("%x, %2x\n", opcode, data);
        fprintf(output, "%c%c", opcode, data);
        line_number += 2;
      }

      // mov ram_addr, A
      else if (args[1][0] == 'a' && arg_lengths[1] == 1) {
        uint8_t location =
            number_from_immideate(args[0], arg_lengths[0], false);
        printf("\nmov %XH, A\n", location);
        printf("f5, %2x\n", location);
        fprintf(output, "%c%c", 0xf5, location);
        line_number += 2;
      }

      // mov ram_addr, @Ri 
      else if (args[1][0] == '@') {
	uint8_t rindex = atoi(&args[1][2]);
	if(rindex > 1) return -1;
	uint8_t location = number_from_immideate(args[0], arg_lengths[0], false);
	uint8_t opcode = 0x86 + rindex;

	printf("\nmov %XH, @R%d\n", location, rindex);
	printf("%x, %2x\n", opcode, location);
	fprintf(output, "%c%c", opcode, location);
	line_number += 2;
      }

      // mov ram_addr, Ri
      else if (args[1][0] == 'r') {
        uint8_t rindex = atoi(&args[1][1]);
	if(rindex > 7) return -1;
	uint8_t location = number_from_immideate(args[0], arg_lengths[0], false);
	uint8_t opcode = 0x88 + rindex;

	printf("\nmov %XH, R%d\n", location, rindex);
	printf("%x, %2x\n", opcode, location);
	fprintf(output, "%c%c", opcode, location);
	line_number += 2;
      }

      // mov ram_addr, #immediate
      else if (args[1][0] == '#') {
	uint8_t location = number_from_immideate(args[0], arg_lengths[0], false);
	uint8_t data = number_from_immideate(args[1], arg_lengths[1], true);

	printf("\nmov %XH, #%XH\n", location, data);
	printf("75, %2x, %2x\n", location, data);
	fprintf(output, "%c%c%c", 0x75, location, data);
	line_number += 3;
      }

      // mov C, bit_addr
      else if (args[0][0] == 'c' && arg_lengths[0] == 1) {
	uint8_t location = number_from_immideate(args[1], arg_lengths[1], false);

	printf("\nmov C, %XH\n", location);
	printf("a2, %2x\n", location);
	fprintf(output, "%c%c", 0xa2, location);
	line_number += 2;
      }

      // mov bit_addr, C
      else if (args[1][0] == 'c' && arg_lengths[1] == 1) {
	uint8_t location = number_from_immideate(args[0], arg_lengths[0], false);

	printf("\nmov %XH, C\n", location);
	printf("92, %2x\n", location);
	fprintf(output, "%c%c", 0x92, location);
	line_number += 2;
      }

      // mov DPTR, #data16
      else if (strcmp(args[0], "dptr") == 0) {
	uint16_t location = number_from_immideate(args[1], arg_lengths[1], true);
	uint8_t msb = location >> 8;
	uint8_t lsb = location & 0xff;
	
	printf("\nmov DPTR, #%XH", location);
	printf("90, %2x, %2x\n", lsb, msb);
	fprintf(output, "%c%c%c", 0x90, lsb, msb);
	line_number += 3;
      }

      // mov ram_addr, ram_addr
      else {
        uint8_t location_0 = number_from_immideate(args[0], arg_lengths[0], false);
	uint8_t location_1 = number_from_immideate(args[1], arg_lengths[1], false);

	printf("\nmov %XH, %XH\n", location_0, location_1);
	printf("85, %2x, %2x\n", location_0, location_1);
	fprintf(output, "%c%c%c", 0x85, location_0, location_1);
	line_number += 3;
      }

      break;
    }
    }

    printf("\n");
  }

  printf("this program has %X lines\n", line_number);

  fclose(input);
  fclose(output);
}
