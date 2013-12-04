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
/* #define WORLD_SIDE_LEN_TAG 104 */

/* Type of nodes */
#define MASTER_ID 0

/* #define SERVANT_ID 1 */

/* ATTENTION!! SERVANT_ID and RED cannot have the same value = 1  */

/* Colors */
#define RED 1
#define BLACK 2

/*
  CELL NUMBERING
  Cells are numbered as pixel on screen. Top left cell is (0,0):(x,y), x grows to the right, y grows down.
*/

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLF, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;

typedef struct cell_t {
  cell_habitant_t type;		/* who lives in this cell */
  int starvation;		/* starvation period if wolf */
  int breeding;		 	/* breeding period of creature */
  struct cell_t* updates[4];	/* list of cells that wanted to update this one in previous subgen */
  int updateSize;
  int x, y;			/* Coordinates - to help debug - will be removed*/
} cell_t;

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

/* LOGIC LOOP */
void worldLoop(int noOfGenerations){
  int x, y, i;
  cell_t* cell;

  for(i = 0 ; i < 4 * noOfGenerations ; i++){
    //     if(i % 4 == 1)
    //       fprintf(stdout, "Iteration %d Red\n", (i/4) + 1);
    // //     if(i % 4 == 3)
    //       fprintf(stdout, "Iteration %d Black\n", (i/4) + 1);
    for(y = 0 ; y < worldSideLen ; y++){
      for(x = 0 ; x < worldSideLen ; x++){
	cell = getCell(x, y);
	if(i % 4 == 0){
	  cell->starvation--;
	  cell->breeding++;
	}
	if (((i % 4 == 0) && isRed(x, y)) || ((i % 4 == 2) && !isRed(x, y))) {
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
	} else if(i%4==3 || i%4==1){
	  update(cell);
	}
      }
    }
    //if (i%4==1 || i%4==3)
    //printWorld2d(stdout);
    /* pressEntertoContinue(); */
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
  MPI_Status status;
  int slaveWorldSize, x, y, startX, startY, endX, endY, color, *buffer;
  cell_t* slaveWorld = NULL;
  cell_t* cell;

  /// No need of all of this because we can load the input for each servants for the initialisation
  ///* starts listening for NEW_BOARD message -> allocates memory for its board part */
  /// /MPI_Recv(&side, 1, MPI_INT, MASTER_ID, NEW_BOARD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

  buffer = (int *)(malloc(sizeof(int) * 2));

  /* Servant loop */
  while (1){

    /* Receive a message from the master */
    MPI_Recv(buffer, 2, MPI_INT, 0, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

    /* Check the tag of the received message. */
    /* If it is FINISHED - all generations are finished - break the loop */
    if(status.MPI_TAG == FINISHED_TAG){
      printf("Slave with rank %d is processing tag 'FINISHED_TAG'.\n", rank);
      printf("Master told me to break the loop.\n\n");
      /* free resources */
      free(slaveWorld);
      return;
    }
    /* else if(status.MPI_TAG == WORLD_SIDE_LEN_TAG) { */
    /*   printf("Slave is processing tag 'WORLD_SIDE_LEN_TAG'.\n"); */
    /*   printf("Master told me that the 'worldSideLen' is %d.\n\n", buffer); */
    /*   fflush(stdout); /\* force it to go out *\/ */
    /*   worldSideLen = buffer; */
    /* } */
    else if(status.MPI_TAG == NEW_BOARD_TAG){
      printf("\nSlave with rank %d is processing tag 'NEW_BOARD_TAG'.\n", rank);
      printf("Master told me to allocate memory for a matrix with %d by %d.\n", worldSideLen, *(buffer+1));
      printf("Master told me to copy the 'world' array starting in index: %d.\n", *(buffer+0));
      slaveWorldSize = worldSideLen * *(buffer+1);
      slaveWorld = (cell_t*)(malloc(slaveWorldSize * sizeof(cell_t)));
      printf("The allocated matrix will have %d cells.\n\n", slaveWorldSize);
      fflush(stdout); /* force it to go out */

      memcpy(slaveWorld, world+*(buffer+0), slaveWorldSize * sizeof(cell_t));
      /* To the slave the 'world' array is of no use anymore; reclaim memory */
      /* free(world); */

    }
    /* Listens for UPDATE_CELL messages, saves messages to board */

    /* Listens for FINISHED meaning all cells are in place */

  }


  while(0){

    /* START_NEXT_GENERATION(genInfo) - start next generation */

    int genInfo[5]; // genInfo : startX, startY, endX, endY, color
    MPI_Recv(&genInfo, 1, MPI_INT, MASTER_ID, START_NEXT_GENERATION_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    startX = genInfo[0];
    startY = genInfo[1];
    endX = genInfo[2];
    endY = genInfo[3];
    color = genInfo[4];

    /* Process steps :
       Do the computation of its part of the board; OK
       Send cells that were changed on a border to master with message UPDATE_CELL; OK
       When done sends FINISHED to master; OK
       Listens for UPDATE_CELL messages from master;
       If cells are in its part of the board updates them (potentially resolves conflicts); OK
       listens for FINISHED messages from master. OK
    */
    if(color == RED || color == BLACK) { // Useless condition. With this loop and the inc of +2 we stay on the right color.

      for(y = startY; y < endY; y += 2) {
	for(x = startX ; x < endX; x += 2) {
	  cell = getCell(x, y);
	  cell->starvation--;
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
      /* Send the cells to the master */
      int buffSize = (endY-startY)*(endX-startX);
      cell_t* cellBuff[buffSize];
      for(y = startY; y < endY; y += 2) {
	for(x = startX ; x < endX; x += 2) {
	  cellBuff[x + 10*y] = getCell(x, y);
	  MPI_Send(&cellBuff, buffSize , MPI_INT /* STRUCTURE CELL */, MASTER_ID, UPDATE_CELL_TAG, MPI_COMM_WORLD);
	}
      }

      /* Send Finished to Master */
      int itFinished = 1;
      MPI_Send(&itFinished, 1, MPI_INT, MASTER_ID, FINISHED_TAG, MPI_COMM_WORLD);

      /* Listens for UPDATE_CELL messages from master; */
      cell_t* rcvCellBuffer[buffSize];
      // MPI_Recv(SOMETHING);
      MPI_Recv(&rcvCellBuffer, buffSize, MPI_INT /* MPI_STRUCT */, MASTER_ID, UPDATE_CELL_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE); // TO DO : Define the structure
      // Now that we have the info for the others cells, we can update

      for(y = startY; y < endY; y += 2) {
	for(x = startX ; x < endX; x += 2) {
	  //cell = getCell(x, y);
	  update(rcvCellBuffer[x + 10*y]);
	}
      }

    }

    else if(color == BLACK) {
      /* Black Process (Same than Red ?)*/
    }

    /* sends all cells from board to master with UPDATE_CELL */

  }
  return;
}


/* ACTIONS OF THE MASTER */
void processMaster(){
  int nTasks, rank, quotient, remainder, *buffer, *slaveSideLen, index;
  /* MPI_Status status; */

  /* Find out how many processes there are in the default communicator */
  MPI_Comm_size(MPI_COMM_WORLD, &nTasks);

  /* Tell all the slaves the size of the worldSideLen sending an message with the WORLD_SIDE_LEN_TAG. */
  /* for(rank = 1; rank < nTasks; ++rank){ */

  /*   /\* Send it to each rank *\/ */
  /*   MPI_Send(&worldSideLen, 1, MPI_INT, rank, WORLD_SIDE_LEN_TAG, MPI_COMM_WORLD); */
  /* } */


  /* Splits the world by the number of slaves */
  /* Consider doing this more correctly using function MPI_DIMS_CREATE */
  /* Handles ghost lines */
  /* Ghost lines - Memory locations used to store redundant copies of data held by neighboring processes*/
  /* Allocating ghost points as extra columns simplifies parallel algorithm by allowing same loop to update all cells */
  /* See slides 6 to 10 in 'jacobi-iteration.pdf' */
  quotient = worldSideLen/(nTasks-1);
  remainder = worldSideLen%(nTasks-1);
  slaveSideLen = (int *)(malloc(nTasks * sizeof(int)));
  *slaveSideLen = 0;
  for(rank = 1; rank < nTasks; rank++)
    *(slaveSideLen+rank) = quotient;
  if(remainder > 0)
    for(rank = 1; rank < remainder+1; rank++)
      (*(slaveSideLen+rank))++;

  buffer = (int *)(malloc(sizeof(int) * 2));

  /* Tell all the slaves to create new board sending an message with the NEW_BOARD_TAG. */
  index = 0;
  for(rank = 1; rank < nTasks; ++rank){
    /* buffer[0] - Inital index from which the slave will start copying its 'slaveWorld' from the 'world' loaded from file. */
    *buffer = index;
    /* buffer[1] - Number of cells in the 'slaveWorld' array */
    *(buffer+1) =  *(slaveSideLen+rank);

    /* Ghost lines */
    if(((rank == 1) && (nTasks > 2)) || ((rank == (nTasks-1)) && (nTasks > 2))){
      /* If it is the first slave of the last just add one redundant line */
      (*(buffer+1))++;
    } else if (nTasks > 2){
      /* Add two redundant lines - one from the previous task and one from the next task */
      (*(buffer+1))++;
      (*(buffer+1))++;
    }

    printf("Inital index from which the slave will start copying: %d for rank: %d.\n", *buffer, rank);
    printf("Number of cells in the 'slaveWorld' array %d for rank: %d.\n", *(buffer+1), rank);
    fflush(stdout); /* force it to go out */

    /* Send it to each rank */
    MPI_Send(buffer, 2, MPI_INT, rank, NEW_BOARD_TAG, MPI_COMM_WORLD);

    if((rank+1) < nTasks)
      index = index + (*(slaveSideLen+rank) * worldSideLen) - (2 * worldSideLen);
  }



  /* Everything is done. Tell all the slaves to break their loops and exit sending an message with the FINISHED_TAG. */
  for(rank = 1; rank < nTasks; ++rank){

    /* Send it to each rank */
    MPI_Send(buffer, 2, MPI_INT, rank, FINISHED_TAG, MPI_COMM_WORLD);
  }

  /* Release resources */
  free(buffer);
  return;
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
  if(rank == MASTER_ID){
    processMaster();
  }else{
    processServant(rank);
  }

  /* Shut down MPI */
  MPI_Finalize();

  /* There is no point in freeing blocks at the end of a program, because all of the program's space is given back to the system when the process terminates. */
  /* Release resources */
  free(world);

  /* Close file descriptor */
  fclose(input);

  /* Exit with sucess */
  return EXIT_SUCCESS;
}
