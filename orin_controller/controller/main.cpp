#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <thread>
#include <set>
#include <iomanip>
#include <getopt.h>

#include "ApplicationMonitor.h"
#include "CGroupUtils.h"
#include "Policy.h"
#include "ApplMonitoringPolicy.h"
#include "SensorsMonitoringPolicy.h"

#define WAKEUP_PERIOD 1 //seconds

enum {
  APPL_MONITORING = 0,
  APPL_UTIL_MONITORING = 1,
  SENSORS_MONITORING = 2,
  CPU_DVFS_POLICY = 3,
  MAX_POLICY = 4
};

//interrupt handler tp enable the usage of ctrl+c
void sig_handler (int sig, siginfo_t *info, void *extra);
bool end_loop = false; //states if the controller has to be terminated

int main (int argc, char* argv[]) {
  unsigned int policy_id = APPL_UTIL_MONITORING; 
  Policy* policy = NULL;

  ////////////////////////////////////////////////////////////////////////////////
  //parsing input arguments
  ////////////////////////////////////////////////////////////////////////////////
  int next_option;
  //a string listing valid short options letters
  const char* const short_options = "hp:";
  //an array describing valid long options
  const struct option long_options[] = { { "help", no_argument, NULL, 'h' }, //help
          { "policy", required_argument, NULL, 'p' }, //select policy
          { NULL, 0, NULL, 0 } /* Required at end of array.  */
  };

  do {
    next_option = getopt_long(argc, argv, short_options, long_options, NULL);
    switch (next_option) {
      case 'p':
        policy_id = atoi(optarg);
        break;
      case -1: /* Done with options.  */
        break;
      case '?':
      case 'h':
      default: /* Something else: unexpected.  */
        std::cout << std::endl << "USAGE: " << argv[0] << " [-p POLICY_ID]" << std::endl;
        std::cout << std::endl << "Supported modes:" << std::endl;
        std::cout << "0 - Simple policy to monitor the throughput of attached applications" << std::endl;
        std::cout << "    The policy also demonstrates how to change mapping and application's parameters" << std::endl;
        std::cout << "1 - Policy 0 + CPU utilization monitoring" << std::endl;
        std::cout << "2 - Policy 1 + board power and CPU/GPU frequency monitoring" << std::endl;
        std::cout << "    The policy also demonstrates how to change the frequency of the CPU" << std::endl;
        std::cout << "3 - Policy to control DVFS based on applications' throughput requirements" << std::endl;
        exit(EXIT_FAILURE);
    }
  } while (next_option != -1);

  if(policy_id >= MAX_POLICY || policy_id<0){
    std::cout << std::endl << "USAGE: " << argv[0] << " [-p POLICY_ID]" << std::endl;
    std::cout << std::endl << "Supported modes:" << std::endl;
    std::cout << "0 - Simple policy to monitor the throughput of attached applications" << std::endl;
    std::cout << "    The policy also demonstrates how to change mapping and application's parameters" << std::endl;
    std::cout << "1 - Policy 0 + CPU utilization monitoring" << std::endl;
    std::cout << "2 - Policy 1 + board power and CPU/GPU frequency monitoring" << std::endl;
    std::cout << "    The policy also demonstrates how to change the frequency of the CPU" << std::endl;
    std::cout << "3 - Policy to control DVFS based on applications' throughput requirements" << std::endl;
    exit(EXIT_FAILURE);
  }

  //setup interrupt handler
  struct sigaction action;
  action.sa_flags = SA_SIGINFO;
  action.sa_sigaction = &sig_handler;
  if (sigaction(SIGINT, &action, NULL) == -1) {
    std::cout << "Error registering interrupt handler\n" << std::endl;
    exit(EXIT_FAILURE);
  }

  //set output precision
  std::cout << std::setprecision(10);
  std::cerr << std::setprecision(10);

  //setup monitors and policies
  if(policy_id == APPL_MONITORING){
    policy = new ApplMonitoringPolicy(!ApplMonitoringPolicy::enableUtil);
  } else if(policy_id == APPL_UTIL_MONITORING){
    policy = new ApplMonitoringPolicy(ApplMonitoringPolicy::enableUtil);
  } else if(policy_id == SENSORS_MONITORING){
    policy = new SensorsMonitoringPolicy();
  } else if(policy_id == CPU_DVFS_POLICY){
    //TODO !!!  
  } else {
    //unreachable else
    std::cout << "unreachable else"<< std::endl;
    exit(EXIT_FAILURE);
  }

  std::cout << "press Ctrl+C in order to stop monitoring" << std::endl;
  std::cout << "monitoring start..." << std::endl;
  int i = 0;

  //control loop
  while (!end_loop) {
    std::cout << std::endl << "iteration " << i << std::endl;

    //run policy
    policy->run(i);

    //sleep for the specified period
    i++;
    std::this_thread::sleep_for(std::chrono::seconds(WAKEUP_PERIOD));
  }

  delete policy;
  return 0;
}

//interrupt handler that finalizes the execution
void sig_handler (int sig, siginfo_t *info, void *extra) {
  if (sig == SIGINT) {
    std::cout << std::endl << "monitoring stop." << std::endl;
    end_loop = true;
  }
}
