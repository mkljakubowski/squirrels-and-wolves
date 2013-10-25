#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
/*
  CELL NUMBERING
  Cells are numbered as pixel on screen. Top left cell is (0,0):(x,y), x grows to the right, y grows down.
*/

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLF, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;

typedef unsigned int uint;

typedef struct cell_t {
  cell_habitant_t type;		//who lives in this cell
  uint starvation;		//starvation period if wolf
  uint breeding;		//breeding period of creature
  struct cell_t* updates[4];	//list of cells that wanted to update this one in previous subgen
  uint updateSize;
} cell_t;

//list of neighbours of some cell
typedef struct neighbours_t {
  cell_t** cells;
  uint size;
} neighbours_t;

/* GLOBALS */
cell_t* world = NULL;
uint worldSideLen = 0;
uint worldSize = 0;
uint wolfBreedingPeriod = 0;
uint squirrelBreedingPeriod = 0;
uint wolfStarvationPeriod = 0;

/* FUNCTIONS */
cell_t* getCell(uint x, uint y){
  /* printf ("X: %d; Y: %d\n", x, y); */
  assert(x < worldSideLen && x >= 0);
  assert(y < worldSideLen && y >= 0);
  return &world[y * worldSideLen + x];
}

cell_t* getCellAndCheckBoundries(uint x, uint y){
  if(x < 0 || x >= worldSideLen || y < 0 || y >= worldSideLen)
    return NULL;
  return getCell(x,y);
}

cell_habitant_t charToCellType(char c){
  switch(c){
    case 'w': return WOLF;
    case 's': return SQUIRREL;
    case 'i': return ICE;
    case 't': return TREE;
    case '$': return TREE_WITH_SQUIRREL;
    default : assert(0 == 1); return EMPTY;
  }
}

char cellTypeTochar(cell_habitant_t type)
{
  switch (type){
    case WOLF: 			return 'w';
    case SQUIRREL: 		return 's';
    case ICE: 			return 'i';
    case TREE: 			return 't';
    case TREE_WITH_SQUIRREL: 	return '$';
    default: assert(0 == 1);
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
  sscanf(buf, "%d", &worldSideLen);
  worldSize = worldSideLen * worldSideLen;
  world = (cell_t*)(malloc(worldSize * sizeof(cell_t)));

  //clear
  for(i = 0 ; i < worldSize ; i++){
    world[i].type = EMPTY;
    world[i].starvation = 0;
    world[i].breeding = 0;
    world[i].updateSize = 0;
  }

  //init cells
  while(getline(&buf, &len, file) != -1){
    sscanf(buf, "%d %d %c", &x, &y, &type);
    cell = getCell(x-1, y-1);
    cell->type = charToCellType(type);

    if(type == TREE_WITH_SQUIRREL || type == SQUIRREL){
      cell->breeding = 0;
    }else if(type == WOLF){
      cell->breeding = 0;
      cell->starvation = wolfStarvationPeriod;
    }
  }
}

uint isRed(uint x, uint y){
  if((x%2 == 0 && y%2==0) || (x%2 == 1 && y%2 == 1))
    return 1;
  return 0;
}

cell_t* checkIfCellHabitable(cell_t* cell, cell_habitant_t type){
  if(cell == NULL) return NULL;
  if(cell->type == ICE) return NULL;
  if(cell->type == TREE_WITH_SQUIRREL) return NULL;
  if(cell->type == TREE && type == WOLF) return NULL;
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
  if(who->type == WOLF && who->starvation == 0){
    who->type = EMPTY;
  }
}


//return who should stay on the current cell
cell_habitant_t checkIfShouldBreed(cell_t* who){
  if(who->type == WOLF && who->breeding >= wolfBreedingPeriod)
    return WOLF;
  if(who->type == SQUIRREL && who->breeding >= squirrelBreedingPeriod)
    return SQUIRREL;
  if(who->type == TREE_WITH_SQUIRREL && who->breeding >= squirrelBreedingPeriod)
    return TREE_WITH_SQUIRREL;
  if(who->type == TREE_WITH_SQUIRREL)
    return TREE;
  return EMPTY;
}

void copy(cell_t* from, cell_t* to){
  to->breeding = from->breeding;
  to->starvation = from->starvation;
  to->type = from->type;  
}

void eat(cell_t* wolf, cell_t* squirrel){
  cell_habitant_t stays = checkIfShouldBreed(wolf);
  wolf->starvation = wolfStarvationPeriod;
  copy(wolf, squirrel);;
  wolf->type = stays;
  wolf->breeding = 0;
  wolf->starvation = wolfStarvationPeriod;
}

//this gets complicated, it should handle every possibility(even conflicts) except when wolf eats a squirrel
void move(cell_t* from, cell_t* to){
  cell_habitant_t stays = checkIfShouldBreed(from); //this func checks what should stay on 'from' field
  if(to->type == TREE){ //but it doesnt check what happens on 'to' field
    copy(from, to);;
    to->type = TREE_WITH_SQUIRREL;
  }else if(to->type == WOLF){
    //conflict
    if(from->starvation == to->starvation){
      from->breeding = from->breeding > to->breeding ? from->breeding : to->breeding;
    }else{
      if(from->starvation > to->starvation){
	//do nothing from is a stronger wolf -> it will be the one to get copied
      }else{
	from->breeding = to->breeding;
	from->starvation = to->starvation;
      }
    }
    copy(from, to);;
  }else if(to->type == SQUIRREL || to->type == TREE_WITH_SQUIRREL){
    //conflict, pick stronger squirrel
    from->breeding = from->breeding > to->breeding ? from->breeding : to->breeding;
    copy(from, to);;
  }else{
    copy(from, to);;
  }
  from->type = stays;
  if(stays == TREE_WITH_SQUIRREL || stays == SQUIRREL){
    from->breeding = 0;
  }else if(stays == WOLF){
    from->breeding = 0;
    from->starvation = wolfStarvationPeriod;
  }
}


void doSquirrelStuff(uint x, uint y, cell_t* cell){
  neighbours_t* neighbours = getActiveCellsAroundFor(x, y, SQUIRREL);
  uint i = 0;
  cell_t* n = NULL;

  //look for empty place to move
  for(i = 0 ; i < neighbours->size ; i++){
    n = neighbours->cells[i];
    if(n->type == EMPTY || n->type == TREE){
      n->updates[n->updateSize] = cell;
      n->updateSize++;
      return;
    }
  }

  free(neighbours->cells);
}


void doWolfStuff(uint x, uint y, cell_t* cell){
  neighbours_t* neighbours = getActiveCellsAroundFor(x, y, WOLF);
  uint i = 0;
  cell_t* n = NULL;

  //check for squirrel
  for(i = 0 ; i < neighbours->size ; i++){
    n = neighbours->cells[i];
    if(n->type == SQUIRREL){
      n->updates[n->updateSize] = cell;
      n->updateSize++;
      return;
    }
  }

  //look for empty place to move
  for(i = 0 ; i < neighbours->size ; i++){
    n = neighbours->cells[i];
    if(n->type == EMPTY){
      n->updates[n->updateSize] = cell;
      n->updateSize++;
      return;
    }
  }

  //if cant do anything
  checkIfShouldDie(cell);
  free(neighbours->cells);
}

//updates current cell and cells that want to do sth with this cell
void update(cell_t* cell){
  uint i, updates = cell->updateSize;
  printf("%d\n",updates);
  //move squirrels
  for(i = 0 ; i < updates ; i++){
    if(cell->updates[i]->type == SQUIRREL || cell->updates[i]->type == TREE_WITH_SQUIRREL){
      printf("s\n");
      move(cell->updates[i], cell);
    }
  }
  
  //move wolves
  for(i = 0 ; i < updates ; i++){
    if(cell->updates[i]->type == WOLF){
      printf("w\n");
      if(cell->type == SQUIRREL){
	eat(cell->updates[i], cell);
      }else{
	move(cell->updates[i], cell);
      }
    }
  }
  
  cell->updateSize = 0;
}

/* =============================================== CELL BEHAVIOURS END =============================================== */

/* LOGIC LOOP */
void worldLoop(int noOfGenerations){
  uint x, y, i;
  cell_t* cell;

  for(i = 0 ; i < 2 * noOfGenerations ; i++){
    for(x = 0 ; x < worldSideLen ; x++){
      for(y = 0 ; y < worldSideLen ; y++){
	if((i%2 == 0 && isRed(x, y)) || (i%2 == 1 && !isRed(x,y))){
	  cell = getCell(x, y);
	  update(cell);
	  cell->starvation--;		//dont care whats inside
	  cell->breeding++;
	  switch(cell->type){
	  case EMPTY: break;
	  case ICE: break;
	  case TREE: break;
	  case SQUIRREL:
	    doSquirrelStuff(x, y, cell);
	    break;
	  case TREE_WITH_SQUIRREL:
	    doSquirrelStuff(x, y, cell);
	    break;
	  case WOLF:
	    doWolfStuff(x, y, cell);
	    break;
	  }
	}
      }
    }
  }
}


/* PRINT WORLD IN STDOUT */
void printWorld()
{
  uint x, y;
  cell_t* cell;

  fprintf(stdout, "%d\n", worldSideLen);
  fflush(stdout); /* force it to go out */
  for(x = 0 ; x < worldSideLen ; x++){
    for(y = 0 ; y < worldSideLen ; y++){
      cell = getCell(x, y);
      if (cell->type != EMPTY){
	fprintf(stdout, "%d %d %c\n", x, y, cellTypeTochar(cell->type));
      }
    }
  }
}


/* MAIN */
int main(int argc, char **argv){
  if(argc < 6){
    printf("ERROR: too few arguments.\n");
    fflush(stdout); /* force it to go out */
    exit(1);
  }
  FILE* input = fopen(argv[1], "r");
  if(input == NULL){
    printf("ERROR: file does not exist.\n");
    fflush(stdout); /* force it to go out */
    exit(1);
  }
  wolfBreedingPeriod = atoi(argv[2]);
  squirrelBreedingPeriod = atoi(argv[3]);
  wolfStarvationPeriod = atoi(argv[4]);
  uint noOfGenerations = atoi(argv[5]);

  loadWorld(input);
  worldLoop(noOfGenerations);
  printWorld();
  
  fclose(input);
  return 0;
}
