/* NIST Secure Hash Algorithm */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../controller/ApplicationMonitor.h"
#include <unistd.h>
#include <iostream>
#include <signal.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "sha.h"
#include <fstream>

#define THR_REQ_CHANGE_TIME 150

int main(int argc, char **argv)
{
    data_t *data; //pointer to the shared memory to communicate with the monitor

    FILE *fin;
    SHA_INFO sha_info;
    int i, times;
    char* filename;
    double req_thr = 0;
    bool curr_gpu, prec_gpu = false;
    bool change_thr = false;

    if(argc <= 2 || argc > 6){
        printf("Usage: %s <IN_FILE_NAME> <NUM_OF_ITERS> [REQ_THR] [THR_CHANGE=Y] \n", argv[0]);
        return -1;
    }
    times = std::atoi(argv[2]);
    if (times < 0){
        std::cout << "negative times value " << times << std::endl;
        exit(0);
    }
    filename = argv[1];
    if (argc >= 4){
        req_thr = std::atof(argv[3]);
        if (req_thr < 0){
            std::cout << "negative throughput " << req_thr << std::endl;
            exit(0);
        }
        if (argc == 5)
            change_thr = true;
    }
    
    //attach the monitor. parameters are: application name, required throughput, a flag for 
    //specifying if the application is multithreaded and another one for the unavailability of the GPU implementation
    data = monitorAttach(argv[0], req_thr, false, false);

    for(i = 0; i < times; i++){
        //read GPU usage flag set by the controller. The application should use this flag to
        //decide which implementation of the computing kernel to execute
        curr_gpu = useGPU(data);
        if(curr_gpu!=prec_gpu){
            std::cout << "Flag useGPU changed by the controller: " << curr_gpu << std::endl; 
        }
        prec_gpu = curr_gpu;

        fin = fopen(filename, "rb");
        if (fin ) {
            sha_init(&sha_info, i);
            sha_stream(&sha_info, fin);
            //sha_print(&sha_info); //print output
            fclose(fin);
            
            //if there is a throughput requirement, do autosleep to avoid to run faster than needed
            if(req_thr > 0)
                autosleep(data, req_thr);
            //send heartbeat
            monitorTick(data);
            
        } else {
            printf("error opening %s for reading\n", filename);
        }
        // here an example of throughput changes signaled to the controller (if enabled). 
        // similarly it is possible to receive infos (e.g. run on gpu) from the controller 
        // (the code for input infos has to be put at the beginning of the loop)
        if(change_thr && i==THR_REQ_CHANGE_TIME){
          std::cout << "CHANGE THROUGHPUT (communicate to the controller)" << std::endl;
          req_thr = req_thr/2;
          setReqThroughput(data, req_thr);
        }
    }
    //detach the monitor
    monitorDetach(data);

    return 0;
}

