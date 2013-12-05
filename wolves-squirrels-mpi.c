#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <mpi.h>
#include <stddef.h>
#include <string.h>

/* Type of messages */
#define NEW_BOARD_TAG 100
#define UPDATE_CELL_TAG 101
#define FINISHED_TAG 102
#define START_NEXT_GENERATION_TAG 103

/*
  CELL NUMBERING
  Cells are numbered as pixel on screen. Top left cell is (0,0):(x,y), x grows to the right, y grows down.
*/

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLF, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;
typedef enum node_category_t { MASTER, SERVANT } node_category_t;
typedef enum color_t { RED, BLACK } color_t;

typedef struct cell_t {
  cell_habitant_t type;		/* who lives in this cell */
  int starvation;		/* starvation period if wolf */
  int breeding;		 	/* breeding period of creature */
  struct cell_t* updates[4];	/* list of cells that wanted to update this one in previous subgen */
  int updateSize;
  int x, y;			/* Coordinates - to help debug - will be removed*/
} cell_t;

typedef struct update_cell_message_t {
  int x,y;
  cell_t cell;
} update_cell_message_t;

//list of neighbours of some cell
typedef struct neighbours_t {
  cell_t** cells;
  int size;
} neighbours_t;

/* GLOBALS */
cell_t* world = NULL;
int worldSideLen = 0;
int worldSize = 0;
int wolfBreedingPeriod = 0;
int squirrelBreedingPeriod = 0;
int wolfStarvationPeriod = 0;
int noOfGenerations = 0;
int msgSize = sizeof(update_cell_message_t);

/* FUNCTIONS */
cell_t* getCell(int x, int y){
  /* printf ("X: %d; Y: %d\n", x, y); */
  assert(x < worldSideLen && x >= 0);
  assert(y < worldSideLen && y >= 0);
  return &world[y * worldSideLen + x];
}

cell_t* getCellAndCheckBoundries(int x, int y){
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

char cellTypeTochar(cell_habitant_t type){
  switch (type){
  case WOLF: 			return 'w';
  case SQUIRREL: 		return 's';
  case ICE: 			return 'x';
  case TREE: 			return 't';
  case TREE_WITH_SQUIRREL: 	return '$';
  case EMPTY: 			return ' ';
  default: assert(0 == 1);
  }
}

void loadWorld(FILE* file){
  char* buf = NULL;
  char type;
  int x, y, i;
  size_t len;
  cell_t* cell;

  /* init world array */
  getline(&buf, &len, file);
  sscanf(buf, "%d", &worldSideLen);
  worldSize = worldSideLen * worldSideLen;
  world = (cell_t*)(malloc(worldSize * sizeof(cell_t)));

  /* clear */
  for(i = 0; i < worldSize; i++){
    world[i].type = EMPTY;
    world[i].starvation = 0;
    world[i].breeding = 0;
    world[i].updateSize = 0;
    world[i].x = i%worldSideLen;
    world[i].y = i/worldSideLen;
  }

  /* init cells */
  while(getline(&buf, &len, file) != -1){
    sscanf(buf, "%d %d %c", &y, &x, &type);
    cell = getCell(x, y);
    cell->type = charToCellType(type);
    cell->breeding = 0;
    cell->starvation = wolfStarvationPeriod;
  }

}

int isRed(int x, int y){
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
neighbours_t getActiveCellsAroundFor(int x, int y, cell_habitant_t type){
  cell_t* up, *right, *down, *left;
  up = checkIfCellHabitable(getCellAndCheckBoundries(x, y+1), type);
  right = checkIfCellHabitable(getCellAndCheckBoundries(x+1, y), type);
  down = checkIfCellHabitable(getCellAndCheckBoundries(x, y-1), type);
  left = checkIfCellHabitable(getCellAndCheckBoundries(x-1, y), type);

  int size = 0;
  if(up != NULL) size++;
  if(right != NULL) size++;
  if(down != NULL) size++;
  if(left != NULL) size++;

  neighbours_t neighbours;
  neighbours.cells = (cell_t**)(malloc(size * sizeof(cell_t*)));
  neighbours.size = size;

  int pos = 0;
  if(down != NULL) neighbours.cells[pos++] = down;
  if(right != NULL) neighbours.cells[pos++] = right;
  if(up != NULL) neighbours.cells[pos++] = up;
  if(left != NULL) neighbours.cells[pos++] = left;

  return neighbours;
}

/* ================================================= CELL BEHAVIOURS ================================================= */
void checkIfShouldDie(cell_t* who){
  if(who->type == WOLF && who->starvation <= 0){
    who->type = EMPTY;
  }
}

//return who should stay on the current cell
cell_habitant_t checkIfShouldBreed(cell_t* who){
  if(who->type == WOLF && who->breeding >= wolfBreedingPeriod){
    who->breeding = 0;
    return WOLF;
  }else if(who->type == SQUIRREL && who->breeding >= squirrelBreedingPeriod){
    who->breeding = 0;
    return SQUIRREL;
  }else if(who->type == TREE_WITH_SQUIRREL && who->breeding >= squirrelBreedingPeriod){
    who->breeding = 0;
    return TREE_WITH_SQUIRREL;
  }else if(who->type == TREE_WITH_SQUIRREL){
    return TREE;
  }
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
  copy(wolf, squirrel);
  wolf->type = stays;
  wolf->breeding = 0;
  wolf->starvation = wolfStarvationPeriod;
}

//this gets complicated, it should handle every possibility(even conflicts) except when wolf eats a squirrel
void move(cell_t* from, cell_t* to){
  cell_habitant_t stays = checkIfShouldBreed(from); //this func checks what should stay on 'from' field
  if(to->type == TREE){ //but it doesnt check what happens on 'to' field
    copy(from, to);
    to->type = TREE_WITH_SQUIRREL;
  }else if(from->type == TREE_WITH_SQUIRREL && to->type == EMPTY){
    copy(from, to);
    to->type = SQUIRREL;
  }else if(to->type == WOLF){
    if(from->type == WOLF){    //conflict
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
      copy(from, to);
    }
  }else if(to->type == SQUIRREL || to->type == TREE_WITH_SQUIRREL){
    //conflict, pick stronger squirrel
    from->breeding = from->breeding > to->breeding ? from->breeding : to->breeding;
    copy(from, to);
  }else{
    copy(from, to);
  }

  from->type = stays;
  if(stays == TREE_WITH_SQUIRREL || stays == SQUIRREL){
    from->breeding = 0;
  }else if(stays == WOLF){
    from->breeding = 0;
    from->starvation = wolfStarvationPeriod;
  }
}

void doSquirrelStuff(int x, int y, cell_t* cell){
  neighbours_t neighbours = getActiveCellsAroundFor(x, y, SQUIRREL);
  int i = 0;
  cell_t* n = NULL;

  //look for empty place to move
  for(i = 0 ; i < neighbours.size ; i++){
    n = neighbours.cells[i];
    if(n->type == EMPTY || n->type == TREE){
      cell->updates[cell->updateSize] = n;
      ++cell->updateSize;
      free(neighbours.cells);
      return;
    }
  }
  free(neighbours.cells);
}

void doWolfStuff(int x, int y, cell_t* cell){
  neighbours_t neighbours = getActiveCellsAroundFor(x, y, WOLF);
  int i = 0;
  cell_t* n = NULL;

  //check for squirrel
  for(i = 0 ; i < neighbours.size ; i++){
    n = neighbours.cells[i];
    if(n->type == SQUIRREL){
      cell->updates[cell->updateSize] = n;
      ++cell->updateSize;
      free(neighbours.cells);
      return;
    }
  }

  //look for empty place to move
  for(i = 0 ; i < neighbours.size ; i++){
    n = neighbours.cells[i];
    if(n->type == EMPTY){
      cell->updates[cell->updateSize] = n;
      ++cell->updateSize;
      free(neighbours.cells);
      return;
    }
  }

  //if can't do anything
  checkIfShouldDie(cell);
  free(neighbours.cells);
}

//updates current cell and cells that want to do something with this cell
void update(cell_t* cell){
  int i, updates = cell->updateSize;

  for(i = 0 ; i < updates ; i++){
    if(cell->updates[i]->type == SQUIRREL && cell->type == WOLF){
      eat(cell, cell->updates[i]); //if wolf->squirrel then eat
    }else{
      move(cell, cell->updates[i]); //else move
    }
  }

  cell->updateSize = 0;
}

/* =============================================== CELL BEHAVIOURS END =============================================== */

void fputcn(FILE *stream, int c, size_t n){
  size_t x;
  for(x = 0; x < n; x++)
    fputc(c, stream);
  fputc('\n', stream);
}

/* PRINT WORLD IN STDOUT IN 2D (FOR DEBUGGING PORPUSES) */
void printWorld2d(FILE *stream){
  int x, y, i, j = 0;
  cell_t *cell;
  fputcn(stream, '-', 3 * (1 + worldSideLen));
  fprintf(stream, "   ");
  fflush(stream); /* force it to go out */
  for (i = 0 ; i < worldSideLen ; i++){
    fprintf(stream, "%02d|", i);
    fflush(stream); /* force it to go out */
  }
  fprintf(stream, "\n");
  fflush(stream); /* force it to go out */
  for(y = 0 ; y < worldSideLen ; y++){
    fprintf(stream, "%02d:", j++);
    fflush(stream); /* force it to go out */
    for(x = 0 ; x < worldSideLen ; x++){
      cell = getCell(x, y);
      fprintf(stream, " %c|", (char)toupper((int)cellTypeTochar(cell->type)));
    }
    fprintf(stream, "\n");
    fflush(stream); /* force it to go out */
  }
  fputcn(stream, '-', 3 * (1 +worldSideLen));
  fprintf(stream, "\n\n");
  fflush(stream); /* force it to go out */
}

void pressEntertoContinue(){
  int c;
  printf("Press <enter> to continue: ");
  fflush(stdout);
  while ((c = getchar()) != '\n' && c != EOF) {
    /* nothing */;
  }
}

/* PRINT WORLD IN STDOUT */
void printWorld(){
  int x, y;
  cell_t* cell;

  fprintf(stdout, "%d\n", worldSideLen);
  fflush(stdout); /* force it to go out */
  for(x = 0 ; x < worldSideLen ; x++){
    for(y = 0 ; y < worldSideLen ; y++){
      cell = getCell(x, y);
      if (cell->type != EMPTY){
	fprintf(stdout, "%d %d %c\n", x, y, cellTypeTochar(cell->type));
	fflush(stdout);
      }
    }
  }
}

/* ACTIONS OF ONE SINGLE SERVANT */
void processServant(int rank) {
  MPI_Request req;
  MPI_Status status;
  int x, y, startX, startY, endX, endY, *buffer, flag;
  cell_t* cell;
  color_t color;
  update_cell_message_t updateMsg;

  buffer = (int *)(malloc(sizeof(int) * 128));
  MPI_Irecv(buffer, 4, MPI_INT, MASTER, NEW_BOARD_TAG, MPI_COMM_WORLD, &req);
  startX = buffer[0];
  endX = buffer[1];
  startY = buffer[2];
  endY = buffer[3];

  /* Servant loop */
  while (1){
    MPI_Irecv(buffer, 2, MPI_INT, MASTER, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
    MPI_Request_get_status(req, &flag, &status);
    
    if(status.MPI_TAG == FINISHED_TAG){
      printf("Slave with rank %d is processing tag 'FINISHED_TAG'.\n", rank);
      break;
    }else if(status.MPI_TAG == START_NEXT_GENERATION_TAG){
      printf("\nSlave with rank %d is processing tag 'START_NEXT_GENERATION_TAG'.\n", rank);
      color = buffer[0];

      for(y = startY ; y < endY ; y++){
	for(x = startX ; x < endX ; x++){
	  cell = getCell(x, y);
	  if(color == RED){
	    cell->starvation--;
	    cell->breeding++;
	  }
	  if ((color == RED && isRed(x, y)) || (color == BLACK && !isRed(x, y))) {
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

      for(y = 0 ; y < worldSideLen ; y++){
	for(x = 0 ; x < worldSideLen ; x++){
	    update(cell);

	    //send cells that are on the edge of my board
	    if(x == startX || x == startX-1 || x == endX || x == endX+1){
	      if(y == startY || y == startY-1 || y == endY || x == endY+1){
		updateMsg.x = x;
		updateMsg.y = y;
		updateMsg.cell = *cell;
		MPI_Isend(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, MASTER, FINISHED_TAG, MPI_COMM_WORLD, &req);
	      }
	    }
	}
      }

      /* send finished tag to master saying that all updates sent*/
      MPI_Isend(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, MASTER, FINISHED_TAG, MPI_COMM_WORLD, &req);

      /* listen for updates */
      while(1){
	MPI_Irecv(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, MASTER, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
	MPI_Request_get_status(req, &flag, &status);

	if(status.MPI_TAG == FINISHED_TAG){
	  printf("Slave with rank %d is received all updates\n", rank);
	  break;
	}else if(status.MPI_TAG == UPDATE_CELL_TAG){
	  cell = getCell(updateMsg.x, updateMsg.y);
	  *cell = updateMsg.cell;
	  cell->updateSize = 0; //just to make sure
	}
      }
    }

  }

  //send all cells belonging to servant to master
  for(y = startY ; y < endY ; y++){
    for(x = startX ; x < endX ; x++){
      updateMsg.x = x;
      updateMsg.y = y;
      updateMsg.cell = *cell;
      MPI_Isend(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, MASTER, FINISHED_TAG, MPI_COMM_WORLD, &req);
    }
  }

  //notify that all cells sent
  MPI_Isend(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, MASTER, FINISHED_TAG, MPI_COMM_WORLD, &req);
  free(buffer);
}

/* ACTIONS OF THE MASTER */
void processMaster(){
  MPI_Request req;
  MPI_Status status;
  int nTasks, rank, quotient, remainder, *buffer, nSlaves, subGenNo, finishedServants, flag;
  color_t color;
  update_cell_message_t updateMsg;
  cell_t *cell;

  /* Find out how many processes there are in the default communicator */
  MPI_Comm_size(MPI_COMM_WORLD, &nTasks);
  nSlaves = nTasks - 1;

  /* Splits the world by the number of slaves */
  quotient = worldSideLen/nSlaves;
  remainder = worldSideLen%nSlaves;
  buffer = (int *)(malloc(sizeof(int) * 128));

  /* Tell all the slaves position of theit board piece */
  for(rank = 1; rank < nTasks; rank++){
    buffer[0] = (rank-1)*quotient;
    buffer[1] = rank * quotient;
    buffer[2] = 0;
    buffer[3] = worldSideLen;

    if(rank == nTasks -1){
      buffer[1] += remainder;
    }

    MPI_Isend(buffer, 4, MPI_INT, rank, NEW_BOARD_TAG, MPI_COMM_WORLD, &req);
  }

  //logic loop of master
  for(subGenNo = 0 ; subGenNo < 2* noOfGenerations ; subGenNo++){
    color = (subGenNo % 2 == 1)?RED:BLACK;
    finishedServants = 0;

    //tell servants to start subgen
    for(rank = 1; rank < nTasks; rank++){
      MPI_Isend(&color, 2, MPI_INT, rank, START_NEXT_GENERATION_TAG, MPI_COMM_WORLD, &req);
    }    
    
    //if update -> propagate, if finished -> count until all finish
    while(1){
      MPI_Irecv(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
      MPI_Request_get_status(req, &flag, &status);

      if(status.MPI_TAG == UPDATE_CELL_TAG){
	for(rank = 1; rank < nTasks; rank++){
	  if(rank != status.MPI_SOURCE){
	    MPI_Isend(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, rank, UPDATE_CELL_TAG, MPI_COMM_WORLD, &req); //send updates to other servants
	  }
	}
      }else if(status.MPI_TAG == FINISHED_TAG){
	finishedServants++;
      }

      if(finishedServants == nSlaves){
	for(rank = 1; rank < nTasks; rank++){
	  MPI_Isend(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, rank, FINISHED_TAG, MPI_COMM_WORLD, &req); //finished sending updates
	}
	break; //all servants have finished
      }

    }

  }

  /* master tells slaves that all iterations are finished */
  for(rank = 1; rank < nTasks; rank++){
    MPI_Isend(buffer, 2, MPI_INT, rank, FINISHED_TAG, MPI_COMM_WORLD, &req); //finished all generations
  }

  /* master listens for cells after compytation */
  finishedServants = 0;
  while(1){
    MPI_Irecv(&updateMsg, sizeof(update_cell_message_t), MPI_CHAR, MASTER, MPI_ANY_TAG, MPI_COMM_WORLD, &req);
    MPI_Request_get_status(req, &flag, &status);

    if(status.MPI_TAG == FINISHED_TAG){
      finishedServants++;
      if(finishedServants == nSlaves){
	break;
      }
    }else if(status.MPI_TAG == UPDATE_CELL_TAG){
      cell = getCell(updateMsg.x, updateMsg.y);
      *cell = updateMsg.cell;
    }
  }

  printWorld();

  free(buffer);
}

/* MAIN */
int main(int argc, char **argv){
  FILE *input;	/* File descriptor */
  int rank;

  if(argc < 6){
    printf("ERROR: too few arguments.\n");
    fflush(stdout); /* force it to go out */
    exit(EXIT_FAILURE);
  }

  input = fopen(argv[1], "r");
  if(input == NULL){
    printf("ERROR: file does not exist.\n");
    fflush(stdout); /* force it to go out */
    exit(EXIT_FAILURE);
  }

  /* Initialize global variables with values passed by the command line */
  wolfBreedingPeriod = atoi(argv[2]);
  squirrelBreedingPeriod = atoi(argv[3]);
  wolfStarvationPeriod = atoi(argv[4]);
  noOfGenerations = atoi(argv[5]);

  /* Initialize MPI */
  if(MPI_Init(&argc, &argv) != MPI_SUCCESS){
    perror("Error initializing MPI");
    exit(EXIT_FAILURE);
  }

  /* Load initial world from file */
  loadWorld(input);

  /* Find out my identity in the default communicator */
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  if(rank == MASTER){
    processMaster();
  }else{
    processServant(rank);
  }

  MPI_Finalize();
  fclose(input);
  return EXIT_SUCCESS;
}
