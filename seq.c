#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLVE, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;

typedef unsigned int uint;

typedef struct cell_t {
  cell_habitant_t type;
  uint starvation;
  uint breeding;
} cell_t;

/* GLOBALS */
cell_t* world = NULL;
uint worldX = 0, worldY = 0;
uint worldSize = 0;

/* FUNCTIONS */
cell_t* getCell(uint x, uint y){
  assert(x < worldX);
  assert(y < worldY);
  return &world[x * worldX + y];
}

void setCell(uint x, uint y, cell_habitant_t type){
  getCell(x, y)->type = type;
}

cell_habitant_t charToCellType(char c){
  switch(c){
    case 'w':
       return WOLVE;
    case 's':
       return SQUIRREL;
    case 'i':
       return ICE;
    case 't':
       return TREE;
    default :
       assert(0 == 1);
       return EMPTY;
  }
}

void loadWorld(FILE* file){
  char* buf = NULL;
  char type;
  uint x, y, i;
  size_t len;
  
  //init world array
  getline(&buf, &len, file);
  sscanf(buf, "%d %d", &worldX, &worldY);
  worldSize = worldX * worldY;
  world = (cell_t*)(malloc(worldSize * sizeof(cell_t)));
  
  //clear
  for(i = 0 ; i < worldSize ; i++){
    world[i].type = EMPTY;
    world[i].starvation = 0;
    world[i].breeding = 0;
  }
  
  //init cells
  while(getline(&buf, &len, file) != -1){
    sscanf(buf, "%d %d %c", &x, &y, &type);
    setCell(x, y, charToCellType(type));
  }
}

/* CELL BEHAVIOURS */

/* LOGIC LOOP */

/* MAIN */
int main(int argc, char **argv){
  if(argc < 6){
    printf("ERROR: too few arguments.\n");
    exit(1);
  }
  FILE* input = fopen(argv[1], "r");
  if(input == NULL){
    printf("ERROR: file does not exist.\n");
    exit(1);
  }
  uint wolveBreedingPeriod = atoi(argv[2]);
  uint squirrelBreedingPeriod = atoi(argv[3]);
  uint wolveStarvationPeriod = atoi(argv[4]);
  uint noOfGenerations = atoi(argv[5]);

  loadWorld(input);
  
  fclose(input);
  return 0;
}