#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 CELL NUMBERING
 Cells are numbered as pixel on screen. Top left cell is (0,0), x grows to the right, y grows down.
*/

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLVE, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;
typedef enum color_t { TRANSPARENT, RED, BLACK } color_t;

typedef unsigned int uint;

typedef struct cell_t {
  cell_habitant_t type;	//who lives in this cell
  color_t color; 	//when the cell was changed -> for resolving the confilcts
  uint starvation;	//starvation period if wolve
  uint breeding;	//breeding period of creature
} cell_t;

//list of neighbours of some cell
typedef struct neighbours_t {
  cell_t** cells;
  uint size;
} neighbours_t;

/* GLOBALS */
cell_t* world = NULL;
uint worldX = 0, worldY = 0;
uint worldSize = 0;
uint wolveBreedingPeriod = 0;
uint squirrelBreedingPeriod = 0;
uint wolveStarvationPeriod = 0;

/* FUNCTIONS */
cell_t* getCell(uint x, uint y){
  assert(x < worldX && x > 0);
  assert(y < worldY && y > 0);
  return &world[x * worldX + y];
}

cell_t* getCellAndCheckBoundries(uint x, uint y){
  if(x < 0 || x >= worldSize || y < 0 || y >= worldSize)
    return NULL;
  return getCell(x,y);
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
  cell_t* cell;
  
  //init world array
  getline(&buf, &len, file);
  sscanf(buf, "%d %d", &worldX, &worldY);
  worldSize = worldX * worldY;
  world = (cell_t*)(malloc(worldSize * sizeof(cell_t)));
  
  //clear
  for(i = 0 ; i < worldSize ; i++){
    world[i].type = EMPTY;
    world[i].color = TRANSPARENT;
    world[i].starvation = 0;
    world[i].breeding = 0;
  }
  
  //init cells
  while(getline(&buf, &len, file) != -1){
    sscanf(buf, "%d %d %c", &x, &y, &type);
    cell = getCell(x, y);
    cell->type = type;
    
    if(type == TREE_WITH_SQUIRREL || type == SQUIRREL){
      cell->breeding = 0;	//TODO
    }else if(type == WOLVE){
      cell->breeding = 0;	//TODO
      cell->starvation = 0;	//TODO
    }
  }
}

uint isRed(uint x, uint y){
  if((x%2 == 0 && y%2==0) || (x%2 == 1 && y%2 == 1))
    return 1;
  return 0;
}

cell_t* checkIfCellHabitable(cell_t* cell, cell_habitant_t type){
  if(cell->type == ICE) return NULL;
  if((cell->type == TREE || cell->type == TREE_WITH_SQUIRREL) && type == WOLVE) return NULL;
  return cell;
}

//returns in correct order up -> right -> down -> left
neighbours_t* getActiveCellsAroundFor(uint x, uint y, cell_habitant_t type){
  cell_t* up, *right, *down, *left;
  up = checkIfCellHabitable(getCellAndCheckBoundries(x, y-1), type);
  right = checkIfCellHabitable(getCellAndCheckBoundries(x+1, y), type);
  down = checkIfCellHabitable(getCellAndCheckBoundries(x, y+1), type);
  left = checkIfCellHabitable(getCellAndCheckBoundries(x-1, y), type);
  
  uint size = 0;
  if(up != NULL) size++;
  if(right != NULL) size++;
  if(down != NULL) size++;
  if(left != NULL) size++;
  
  neighbours_t neighbours;
  neighbours.cells = (cell_t**)(malloc(size * sizeof(cell_t*)));
  neighbours.size = size;
  
  uint pos = 0;
  if(up != NULL) neighbours.cells[pos++] = up;
  if(right != NULL) neighbours.cells[pos++] = right;
  if(down != NULL) neighbours.cells[pos++] = down;
  if(left != NULL) neighbours.cells[pos++] = left;
  
  return &neighbours;
}

/* ================================================= CELL BEHAVIOURS ================================================= */
void checkIfShouldDie(cell_t* who){
  if(who->type == WOLVE && who->starvation == 0){
    who->type = EMPTY;
  }
}

//return who should stay on the current cell
cell_habitant_t checkIfShouldBreed(cell_t* who){
  if(who->type == WOLVE && who->breeding >= wolveBreedingPeriod)
    return WOLVE;
  if(who->type == SQUIRREL && who->breeding >= squirrelBreedingPeriod)
    return SQUIRREL;
  if(who->type == TREE_WITH_SQUIRREL && who->breeding >= squirrelBreedingPeriod)
    return TREE_WITH_SQUIRREL;
  if(who->type == TREE_WITH_SQUIRREL)
    return TREE;
  return EMPTY;
}

void eat(cell_t* wolve, cell_t* squirrel){
  cell_habitant_t stays = checkIfShouldBreed(wolve);
  *squirrel = *wolve; //wolve moves erasing squirrel
  wolve->type = stays;
  wolve->breeding = 0;		//TODO
  wolve->starvation = 0;	//TODO
}

void move(cell_t* from, cell_t* to){
  cell_habitant_t stays = checkIfShouldBreed(from); //this func checks what should stay on 'from' field
  if(to->type == TREE){ //but it doesnt check what happens on 'to' field
    *to = *from;
    to->type = TREE_WITH_SQUIRREL;
  }else{
    *to = *from;
  }
  from->type = stays;
  if(stays == TREE_WITH_SQUIRREL || stays == SQUIRREL){
    from->breeding = 0;		//TODO
  }else if(stays == WOLVE){
    from->breeding = 0;		//TODO
    from->starvation = 0;	//TODO
  }
}

void doSquirrelStuff(uint x, uint y, cell_t* cell, color_t color){
  neighbours_t* neighbours = getActiveCellsAroundFor(x, y, SQUIRREL);
  uint i = 0;
  cell_t* n = NULL;
  
  //check for squirrel
  for(i = 0 ; i < neighbours->size ; i++){
    n = neighbours->cells[i];
    if(n->type == SQUIRREL){
      eat(cell, n);
      n->color = color;
      return;
    }
  }
  
  //look for empty place to move
  for(i = 0 ; i < neighbours->size ; i++){
    n = neighbours->cells[i];
    if(n->type == EMPTY){
      move(cell, n);
      n->color = color;
      return;
    }
  }
  
  //TODO: check for conflicts
  
  //if cant do anything
  checkIfShouldDie(cell);
  checkIfShouldBreed(cell);
}

void doWolveStuff(uint x, uint y, cell_t* cell, color_t color){
  neighbours_t* neighbours = getActiveCellsAroundFor(x, y, WOLVE);
  uint i = 0;
  cell_t* n = NULL;
  
  //look for empty place to move
  for(i = 0 ; i < neighbours->size ; i++){
    n = neighbours->cells[i];
    if(n->type == EMPTY || n->type == TREE){
      move(cell, n);
      n->color = color;
      return;
    }
  }
  
  //TODO: check for conflicts

  //if cant do anything
  checkIfShouldBreed(cell);
}
/* =============================================== CELL BEHAVIOURS END =============================================== */

/* LOGIC LOOP */
void worldLoop(int noOfGenerations){
  color_t currentColor = RED;
  uint x, y, i;
  cell_t* cell;
  
  for(i = 0 ; i < 2* noOfGenerations ; i++){
    for(x = 0 ; x < worldSize ; x++){
      for(y = 0 ; y < worldSize ; y++){
	cell = getCell(x, y);
	cell->color = currentColor;	//clear color for next sub-generation
	cell->starvation--;		//dont care whats inside
	cell->breeding++;
	
 	switch(cell->type){
	  case EMPTY: break;
	  case ICE: break;
	  case TREE: break;
	  case SQUIRREL:
	    doSquirrelStuff(x, y, cell, currentColor);
	    break;
	  case TREE_WITH_SQUIRREL:
	    doSquirrelStuff(x, y, cell, currentColor);
	    break;
	  case WOLVE:
	    doWolveStuff(x, y, cell, currentColor);
	    break;
	}
      }
    }
    currentColor = currentColor == RED ? BLACK : RED; //switch color of loop
  }
}

void printWorld(){
  
}

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
  wolveBreedingPeriod = atoi(argv[2]);
  squirrelBreedingPeriod = atoi(argv[3]);
  wolveStarvationPeriod = atoi(argv[4]);
  uint noOfGenerations = atoi(argv[5]);

  loadWorld(input);
  worldLoop(noOfGenerations);
  printWorld();
  
  fclose(input);
  return 0;
}