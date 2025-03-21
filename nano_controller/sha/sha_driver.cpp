/* NIST Secure Hash Algorithm */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../controller/ApplicationMonitor.h"
#include "../controller/MappingConsts.h"
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "sha.h"
#include <fstream>


int main(int argc, char **argv)
{
    data_t *data; //pointer to the shared memory to communicate with the monitor

    FILE *fin;
    SHA_INFO sha_info;
    int i, times;
    char* filename;
    double req_thr=1.0;
    int mapping=0;

    if(argc <= 2 || argc > 5){
        printf("Usage: %s <IN_FILE_NAME> <NUM_OF_ITERS> [REQ_THR] [MAPPING] \n", argv[0]);
        return -1;
    }
    times = std::atoi(argv[2]);
    if (times <0){
      std::cout << "negative times value " << times << std::endl;
      exit(0);
    }
    filename = argv[1];
    if (argc>=4){
        req_thr=std::atof(argv[3]);
        if (req_thr <0){
          std::cout << "negative throughput " << req_thr << std::endl;
          exit(0);
        }
    }
    if (argc==5) {
        mapping=std::atoi(argv[4]);
        if (mapping != MAP_BIG_ID && mapping != MAP_LITTLE_ID){
          std::cout << "not supported mapping " << mapping << std::endl;
          exit(0);
        }
    }
    
    //attach the monitor. parameters are: application name,
    //minimum and throughout requirements, and the unavailability of the GPU implementation
    data = monitorAttach(argv[0], req_thr, mapping, 1, false);

    for(i = 0; i < times; i++){
        fin = fopen(filename, "rb");
        if (fin ) {
            sha_init(&sha_info, i);
            sha_stream(&sha_info, fin);
            //sha_print(&sha_info); //print output
            fclose(fin);
            
            //sleep for a while
            if(req_thr > 0)
                autosleep(data, req_thr);
            //send heartbeat
            monitorTick(data);
            
        } else {
            printf("error opening %s for reading\n", filename);
        }
        if(i==300){
          printf("CHANGE THROUGHPUT\n");
          setReqThroughput(data, req_thr/2);
        }
    }
    //detach the monitor
    monitorDetach(data);

    return 0;
}

