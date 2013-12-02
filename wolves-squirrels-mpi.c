#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <mpi.h>
#include <stddef.h>

/* Type of messages */
#define NEW_BOARD_TAG 100
#define UPDATE_CELL_TAG 101
#define FINISHED_TAG 102
#define START_NEXT_GENERATION_TAG 103

/* Type of nodes */
#define MASTER_ID 0
#define SERVANT_ID 1

/* Colors */
#define RED 1
#define BLACK 2
/*
 */

/*
  CELL NUMBERING
  Cells are numbered as pixel on screen. Top left cell is (0,0):(x,y), x grows to the right, y grows down.
*/

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLF, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;

typedef struct cell_t {
  cell_habitant_t type;		//who lives in this cell
  int starvation;		//starvation period if wolf
  int breeding;		//breeding period of creature
  struct cell_t* updates[4];	//list of cells that wanted to update this one in previous subgen
  int updateSize;
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
void processServant() {
  int side;

  /// No need of all of this because we can load the input for each servants for the initialisation
  ///* starts listening for NEW_BOARD message -> allocates memory for its board part */
  /// /MPI_Recv(&side, 1, MPI_INT, MASTER_ID, NEW_BOARD_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);


  ///* listens for UPDATE_CELL messages, saves messages to board */

  ///* listens for FINISHED meaning all cells are in place */

  ///* Servant loop */
  while(1) {

    /* if it is FINISHED - all generations are finished - break the loop */
    int isFinished = 0;
    int startX, startY, endX, endY;
    int x, y;
    cell_t* cell;
    // Go into a blocking-receive waiting
    MPI_Recv(&isFinished, 1, MPI_INT, MASTER_ID, FINISHED_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    if(isFinished) {
      break;
    }

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
				//  MPI_Send(&cellBuff, buffSize , STRUCTURE CELL, MASTER_ID, UPDATE_CELL_TAG, MPI_COMM_WORLD);
		
		/* Send Finished to Master */
		int itFinished = 1
		MPI_send(&itFinished, 1, MPI_INT, MASTER_ID, FINISHED_TAG); 
		 
		/* Listens for UPDATE_CELL messages from master; */
		cell_t* rcvCellBuffer[buffSize]
		// MPI_Recv(SOMETHING);
		
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

    /* exits */
  }
  return;
}


/* ACTIONS OF THE MASTER */
/* int processMaster(FILE* input, int numservants){ */

/*   /\* */
/*     Splits the world in 'numservants' parts. */
/*     A first approach is to split the rows of the world evenly according the supplied 'numservants', */
/*     so that each servent process receives, within the possibilities, the same number of rows. */
/*     Later, it could be implemented a more 'clever' division of work. One that balances better the */
/*     workload among servent processes so that these finish its portion of work, roughly, at the same */
/*     time. */

/*     All MPI data copy routines expect that source and destination memory are "flat" linear arrays. */
/*     Multidimensional C arrays should be stored in row-major order. In row-major storage, a */
/*     multidimensional array in linear memory is organized such that rows are stored one after the other. */
/*   *\/ */

/*   return 0; */
/* } */

/* int main(int argc, char **argv) { */
/*   int size, rank; */

/*   MPI_Init(&argc, &argv); */
/*   MPI_Comm_size(MPI_COMM_WORLD, &size); */
/*   MPI_Comm_rank(MPI_COMM_WORLD, &rank); */

/*   int *globaldata=NULL; */
/*   int localdata; */
/*   int i; */

/*   if (rank == 0) { */
/*     globaldata = malloc(size * sizeof(int)); */
/*     for (i=0; i<size; i++) */
/*       globaldata[i] = 2*i+1; */

/*     printf("Processor %d has data: ", rank); */
/*     for (i=0; i<size; i++) */
/*       printf("%d ", globaldata[i]); */
/*     printf("\n"); */
/*   } */

/*   MPI_Scatter(globaldata, 1, MPI_INT, &localdata, 1, MPI_INT, 0, MPI_COMM_WORLD); */

/*   printf("Processor %d has data %d\n", rank, localdata); */
/*   localdata *= 2; */
/*   printf("Processor %d doubling the data, now has %d\n", rank, localdata); */

/*   MPI_Gather(&localdata, 1, MPI_INT, globaldata, 1, MPI_INT, 0, MPI_COMM_WORLD); */

/*   if (rank == 0) { */
/*     printf("Processor %d has data: ", rank); */
/*     for (i=0; i<size; i++) */
/*       printf("%d ", globaldata[i]); */
/*     printf("\n"); */
/*   } */

/*   if (rank == 0) */
/*     free(globaldata); */

/*   MPI_Finalize(); */
/*   return 0; */
/* } */

/* MAIN */
int main(int argc, char **argv){
  int p;	/* Number of processes */
  int id;	/* Process rank */

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

  /*
    INITIALIZE GLOBAL VARIABLES WITH VALUES PASSED BY THE COMMAND LINE
    Both master and servants will have access to these variables
  */
  wolfBreedingPeriod = atoi(argv[2]);
  squirrelBreedingPeriod = atoi(argv[3]);
  wolfStarvationPeriod = atoi(argv[4]);
  noOfGenerations = atoi(argv[5]);

  /* MPI initialisation. */
  if(MPI_Init(&argc, &argv) != MPI_SUCCESS){
    perror("Error initializing MPI");
    exit(1);
  }

  /*
    Get the number of MPI tasks and the id of this task.
  */
  MPI_Comm_size(MPI_COMM_WORLD, &p); // Get number of processes
  MPI_Comm_rank(MPI_COMM_WORLD, &id); // Get own ID

  if(id == MASTER_ID){
    /* Only master process loads initial World */
    loadWorld(input);
  }

  /* create a type for struct cell_t */
  const int nitems=5;
  int block_lengths[5] = {1, 1, 1, 4*sizeof(cell_t *), 1};
  MPI_Datatype mpi_types[5] = {MPI_INT, MPI_INT, MPI_INT, MPI_INT};
  MPI_Datatype mpi_cell_type;
  /* MPI_Aint type used to idetify byte displacement of each block (array)*/
  MPI_Aint     offsets[5];
  offsets[0] = offsetof(cell_t, type);
  offsets[1] = offsetof(cell_t, starvation);
  offsets[2] = offsetof(cell_t, breeding);
  offsets[3] = offsetof(cell_t, updates);
  offsets[4] = offsetof(cell_t, updateSize);
  MPI_Type_create_struct(nitems, block_lengths, offsets, mpi_types, &mpi_cell_type);
  MPI_Type_commit(&mpi_cell_type);
  /* local = (cell_t*)(malloc((worldSize/p)*sizeof(cell_t)); */

  /* if(id == MASTER_ID){ */
  /*   MPI_Scatter(world, worldSize/p, MPI_CHAR, */
  /* 		local, worldSize/p, MPI_CHAR,	/\* each proc receives  into local *\/ */
  /* 		MASTER_ID ,MPI_COMM_WORLD);	/\* sending process is root, all procs in MPI_COMM_WORLD participate *\/ */
  /* } */

  /* else { // Servant process */
  /*	    loadWorld(input); */
  /*   processServant(); // Actions of what a single servant must do */
  /* } */

  /* Print the final World */
  if(id == 0){
    printWorld();
    free(world);
  }

  MPI_Type_free(&mpi_cell_type);
  /* MPI finalization. */
  MPI_Finalize();

  /* Close file descriptor */
  fclose(input);
  return 0;
}
