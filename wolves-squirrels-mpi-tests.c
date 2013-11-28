#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>
#include <mpi.h>
#include <stddef.h>

/* TYPES */
typedef enum cell_habitant_t { EMPTY, SQUIRREL, WOLF, ICE, TREE, TREE_WITH_SQUIRREL } cell_habitant_t;

typedef struct cell_t {
  cell_habitant_t type;		/* who lives in this cell */
  int starvation;		/* starvation period if wolf */
  int breeding;		/* breeding period of creature */
  /* struct cell_t* updates[4];	list of cells that wanted to update this one in previous subgen */
  int updateSize;
} cell_t;

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


typedef struct {
  int ne, n, u, v, process, min, strip, mincost, b;
} info;


int main(int argc, char **argv){
  FILE* input;	/* File descriptor */
  int p;	/* Number of processes */
  int id;	/* Process rank */
  int i;	/* Iterations */
  int *sendbuff;

  if(argc < 6){
    printf("ERROR: too few arguments.\n");
    fflush(stdout); /* force it to go out */
    exit(1);
  }

  input = fopen(argv[1], "r");
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
  MPI_Barrier(MPI_COMM_WORLD);

  /* Get the number of MPI tasks and the id of this task. */
  MPI_Comm_size(MPI_COMM_WORLD, &p); /* Get number of processes */
  MPI_Comm_rank(MPI_COMM_WORLD, &id); /* Get own ID */
  MPI_Request *sendRequests;
  MPI_Status *sendStatuses;

  if(id == 0){
    int numServ, quotient, remainder;
    /* Only master process loads initial world from file */
    loadWorld(input);
    /* http://stackoverflow.com/questions/13782694/understanding-dimensions-in-mpi-cart-coords-mpi-dims-create-ordering-of-proce */
    /* Splits the world in p-1 parts */
    /* MPI_DIMS_CREATE */
    numServ = p-1;
    quotient = worldSideLen/numServ;
    remainder = worldSideLen%numServ;
    /* http://stackoverflow.com/questions/13543723/mpi-isend-segmentation-fault */
    /* Memory allocation.                                          */
    sendbuff=(int *)malloc(numServ * sizeof(int));
    for(i = 0; i < numServ; i++)
      *(sendbuff+i) = quotient;
    if(remainder != 0)
      for(i = 0; i < remainder; i++)
	*(sendbuff+i) = *(sendbuff+i) + 1;
    /* sends NEW_BOARD(K) to every node */

    sendRequests = malloc(numServ * sizeof(MPI_Request));
    for (i = 0; i < numServ; i++) {
      MPI_Isend(sendbuff[i], 1, MPI_INT, i+1, 0, MPI_COMM_WORLD, &sendRequests[numServ]);
    }
    MPI_Waitall(numServ, sendRequests, sendStatuses);
    /* sends correct part of board to every node + 1 cell border around it by sending UPDATE_CELL messages (now the nodes have data to work on) */
    /* sends FINISHED to confirm sending all cells */
  }

  /* printf("Processor %d has var 'wolfBreedingPeriod' value: %d\n", id, wolfBreedingPeriod); */
  /* printf("Processor %d has var 'squirrelBreedingPeriod' value: %d\n", id, squirrelBreedingPeriod); */
  /* printf("Processor %d has var 'wolfStarvationPeriod' value: %d\n", id, wolfStarvationPeriod); */
  /* printf("Processor %d has var 'noOfGenerations' value: %d\n", id, noOfGenerations); */
  /* printf("Processor %d has var 'World' value: %p\n", id, world); */
  /* printf("Processor %d has var 'worldSize' value: %d\n", id, worldSize); */

  /* MPI_Scatter: http://www.mpi-forum.org/docs/mpi-1.1/mpi-11-html/node71.html#Node71 */
  /* http://stackoverflow.com/questions/20031250/mpi-scatter-of-2d-array-and-malloc */
  /* http://stackoverflow.com/questions/9269399/sending-blocks-of-2d-array-in-c-using-mpi/9271753#9271753 */
  /* https://gist.github.com/ehamberg/1263868 - Scatterv example*/
  /* http://stackoverflow.com/questions/9864510/struct-serialization-in-c-and-sending-over-mpi */
  /* http://stackoverflow.com/questions/18453387/sending-c-struct-via-mpi-fails-partially */
  /* http://stackoverflow.com/questions/10419990/creating-an-mpi-datatype-for-a-structure-containing-pointers */
  /* http://stackoverflow.com/questions/13039283/sending-typedef-struct-containing-void-by-creating-mpi-drived-datatype */
  /* http://stackoverflow.com/questions/18165277/how-to-send-a-variable-of-type-struct-in-mpi-send */
  /* info _info; */
  /* int count; //Says how many kinds of data your structure has */
  /* count = 1; //1, 'cause you just have int */

  /* //Says the type of every block */
  /* MPI_Datatype array_of_types[count]; */
  /* //You just have int */
  /* array_of_types[0] = MPI_INT; */

  /* //Says how many elements for block */
  /* int array_of_blocklengths[count]; */
  /* //You have 8 int */
  /* array_of_blocklengths[0] = 8; */

  /* /\*Says where every block starts in memory, counting from the beginning of the struct.*\/ */
  /* MPI_Aint array_of_displaysments[count]; */
  /* MPI_Aint address1, address2; */
  /* MPI_Get_address(&_info, &address1); */
  /* MPI_Get_address(&_info.ne, &address2); */
  /* array_of_displaysments[0] = address2 - address1; */

  /* /\* Create MPI Datatype and commit *\/ */
  /* MPI_Datatype stat_type; */
  /* MPI_Type_create_struct(count, array_of_blocklengths, array_of_displaysments, array_of_types, &stat_type); */
  /* MPI_Type_commit(&stat_type); */

  /* /\* MPI_COMM_WORLD  //Now we are ready to send *\/ */
  /* /\* MPI_Send(&_info, 1, stat_type, dest, tag, MPI_COMM_WORLD); *\/ */

  /* /\* . . . *\/ */


  /* Free datatype */
  /* MPI_Type_free(&stat_type); */

  MPI_Barrier(MPI_COMM_WORLD);
  /* Close down the MPI environment */
  MPI_Finalize();

  /* Close file descriptor  */
  fclose(input);
  return EXIT_SUCCESS;
}
