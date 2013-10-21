#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLF, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;

typedef unsigned int uint;


/* /\* Pointer to a structure  *\/ */
/* typedef struct cell_t *Worldcellptr; */

typedef struct cell_t {
  cell_habitant_t type;
  uint starvation;
  uint breeding;
} WorldCell;

WorldCell **world=NULL;

/* GLOBALS */
uint worldSideLen = 0;

/* FUNCTIONS */
WorldCell *getCell(uint x, uint y)
{
  assert(x < worldSideLen);
  assert(y < worldSideLen);
  printf("Chegou a getCell -> X: %d; Y: %d\n" ,x ,y);

  /* returns a pointer to a structure */
  return &world[x][y];
}

void setCell(uint x, uint y, cell_habitant_t type)
{
  printf("Chegou a setCell -> X: %d; Y: %d\n" ,x ,y);
  getCell(x, y)->type = type;
}

cell_habitant_t charToCellType(char c)
{
  switch(c){
  case 'w':
    return WOLF;
  case 's':
    return SQUIRREL;
  case 'i':
    return ICE;
  case 't':
    return TREE;
  default:
    assert(0 == 1);
    return EMPTY;
  }
}

char cellTypeTochar(cell_habitant_t type)
{
  switch (type){
  case WOLF:
    return 'w';
  case SQUIRREL:
    return 's';
  case ICE:
    return 'i';
  case TREE:
    return 't';
  case TREE_WITH_SQUIRREL:
    return '$';
  default:
    assert(0 == 1);
  }
}

void loadWorld(FILE* file)
{
  char* buf = NULL;
  char type;
  uint x, y, i, j;
  size_t len;

  printf("Chegou aqui0!!\n");
  /* init world two-dimensional array */
  getline(&buf, &len, file);
  sscanf(buf, "%d", &worldSideLen);

  /* dynamically allocate a multidimensional array */
  /* http://c-faq.com/aryptr/dynmuldimary.html */
  world = malloc(worldSideLen * sizeof(WorldCell *));
  for(i = 0; i < worldSideLen; i++)
    world[i] = malloc(worldSideLen * sizeof(WorldCell));

  printf("Chegou aqui1!!\n");

  /* clear */
  for(i = 0 ; i < worldSideLen ; i++)
    for (j = 0 ; j < worldSideLen ; j++){
    world[i][j].type = EMPTY;
    world[i][j].starvation = 0;
    world[i][j].breeding = 0;
  }
  printf("Chegou aqui2!!\n");

  /* init cells */
  while(getline(&buf, &len, file) != -1){
    sscanf(buf, "%d %d %c", &x, &y, &type);
    setCell(x, y, charToCellType(type));
  }

  printf("Chegou aqui3!!\n");
}

void printWorld(FILE* file)
{
  printf("Chegou a printWorld!!\n");
}

/* CELL BEHAVIOURS */

/* LOGIC LOOP */

/* MAIN */
/* char *argv[] is a pointer to an array of pointers */
/* char **argv is a pointer to a pointer to char */
int main(int argc, char *argv[])
{
  if(argc < 6){
    printf("ERROR: too few arguments.\n");
    exit(1);
  }

  if(argc > 6){
    printf("ERROR: too much arguments.\n");
    exit(1);
  }

  FILE *input = fopen(argv[1], "r");
  if(input == NULL){
    printf("ERROR: file does not exist.\n");
    exit(1);
  }

  uint wolfBreedingPeriod = atoi(argv[2]);
  uint squirrelBreedingPeriod = atoi(argv[3]);
  uint wolfStarvationPeriod = atoi(argv[4]);
  uint numOfGenerations = atoi(argv[5]);

  
  loadWorld(input);
  fclose(input);
  
  FILE *output = fopen("/cygdrive/c/Users/Paulo/squirrels-and-wolves/output", "w");
  if(output == NULL){ /* Always Check Return Value */
    printf("ERROR: failed to open file for writing.\n");
    exit(1);
  }

  printWorld(output);
  fclose(input);

  uint i;
  /* free the array since it is no longer needed */
  /* malloc knows nothing about structure declarations or about the contents of allocated memory; it especially does not know whether allocated memory contains pointers to other allocated memory. */
  for(i = 0; i < worldSideLen; i++)
    free((void *)world[i]);
  free((void *)world);

  return 0;
}
