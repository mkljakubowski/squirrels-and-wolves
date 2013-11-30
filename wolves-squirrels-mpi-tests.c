#define _GNU_SOURCE
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


/* typedef struct { */
/*   int ne, n, u, v, process, min, strip, mincost, b; */
/* } info; */


int main(int argc, char **argv) {
  FILE* input;	/* File descriptor */
  int nprocs;	/* Number of processes */
  int rank;	/* Process rank */
  int tag;
  /* int i;	/\* Iterations *\/ */
  MPI_Request req;
  MPI_Status status;
  float sendbuf, recvbuf;

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
    exit(EXIT_FAILURE);
  }
  /* MPI_Barrier(MPI_COMM_WORLD); */

  /* Get the number of MPI tasks and the rank of this task. */
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs); /* Get number of processes */
  MPI_Comm_rank(MPI_COMM_WORLD, &rank); /* Get own RANK */

  if (nprocs < 2) {
    fprintf(stderr,"%s: Require at least two processors.\n", argv[0]);
    MPI_Finalize();
    exit(EXIT_FAILURE);
  }
  tag = 1;
  if(rank == 0){
    /* Only master process loads initial world from file */
    /* loadWorld(input); */
    /* Splits the world in p-1 parts */
    /* MPI_DIMS_CREATE */
    /* numServ = nprocs-1; */
    /* quotient = worldSideLen/numServ; */
    /* remainder = worldSideLen%numServ; */
    /* Memory allocation.                                          */
    /* sendbuff=(int *)malloc(numServ * sizeof(int)); */
    /* for(i = 0; i < numServ; i++) */
    /*   *(sendbuff+i) = quotient; */
    /* if(remainder != 0) */
    /*   for(i = 0; i < remainder; i++) */
    /* 	(*(sendbuff+i))++; */
    /* sends NEW_BOARD(K) to every node */
    /* sendRequests = malloc(numServ * sizeof(MPI_Request)); */
    /* sendStatuses = malloc(numServ * sizeof(MPI_Status)); */
    /* for (i = 0; i < numServ; i++) { */
    /*   MPI_Isend(sendbuff, 1, MPI_INT, i+1, 0, MPI_COMM_WORLD, &sendRequests[numServ]); */
    /* } */
    /* MPI_Waitall(numServ, sendRequests, sendStatuses); */
    /* sends correct part of board to every node + 1 cell border around it by sending UPDATE_CELL messages (now the nodes have data to work on) */
    /* sends FINISHED to confirm sending all cells */
    sendbuf = 9.0;
    MPI_Send(&sendbuf, 1, MPI_FLOAT, 1, tag, MPI_COMM_WORLD);
  } else {
    MPI_Irecv(&recvbuf, 1, MPI_FLOAT, 0, tag, MPI_COMM_WORLD, &req);
    MPI_Wait(&req, &status);
    printf("recvbuf = %f\n", recvbuf);
  }

  /* printf("Processor %d has var 'wolfBreedingPeriod' value: %d\n", rank, wolfBreedingPeriod); */
  /* printf("Processor %d has var 'squirrelBreedingPeriod' value: %d\n", rank, squirrelBreedingPeriod); */
  /* printf("Processor %d has var 'wolfStarvationPeriod' value: %d\n", rank, wolfStarvationPeriod); */
  /* printf("Processor %d has var 'noOfGenerations' value: %d\n", rank, noOfGenerations); */
  /* printf("Processor %d has var 'World' value: %p\n", rank, world); */
  /* printf("Processor %d has var 'worldSize' value: %d\n", rank, worldSize); */


  MPI_Barrier(MPI_COMM_WORLD);
  /* Close down the MPI environment */
  MPI_Finalize();

  /* Close file descriptor  */
  /* fclose(input); */
  return EXIT_SUCCESS;
}

/* MPI_Scatter: http://www.mpi-forum.org/docs/mpi-1.1/mpi-11-html/node71.html#Node71 */
/* http://stackoverflow.com/questions/20031250/mpi-scatter-of-2d-array-and-malloc */
/* http://stackoverflow.com/questions/9269399/sending-blocks-of-2d-array-in-c-using-mpi/9271753#9271753 */
/* https://gist.github.com/ehamberg/1263868 - Scatterv example*/
/* http://stackoverflow.com/questions/9864510/struct-serialization-in-c-and-sending-over-mpi */
/* http://stackoverflow.com/questions/18453387/sending-c-struct-via-mpi-fails-partially */
/* http://stackoverflow.com/questions/10419990/creating-an-mpi-datatype-for-a-structure-containing-pointers */
/* http://stackoverflow.com/questions/13039283/sending-typedef-struct-containing-void-by-creating-mpi-drived-datatype */
/* http://stackoverflow.com/questions/18165277/how-to-send-a-variable-of-type-struct-in-mpi-send */
/* http://stackoverflow.com/questions/13782694/understanding-dimensions-in-mpi-cart-coords-mpi-dims-create-ordering-of-proce */
/* http://stackoverflow.com/questions/13543723/mpi-isend-segmentation-fault */
