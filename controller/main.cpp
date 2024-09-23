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
#include "getNode.h"
#include "Policy.h"
#include "ApplMonitoringPolicy.h"
#include "OdroidPolicy.h"
#include "OdroidProfPolicy.h"
#include "TegraPolicy.h"
#include "TegraNanoPolicy.h"
#include "TegraProfPolicy.h"
#include "TegraNanoProfPolicy.h"
#include "RPI3Policy.h"


//#define PROFILING_CALLS

#define WAKEUP_PERIOD 1 //seconds

enum {
  NO_POLICY = 0,
  ONLY_SENSORS = 1,
  ODROID_POLICY = 2,
  ODROID_LOG_POLICY = 3,
  ODROID_PROF_POLICY = 4,
  TEGRA_POLICY = 5,
  TEGRA_LOG_POLICY = 6,
  TEGRA_PROF_POLICY = 7,
  TEGRANANO_POLICY = 8,
  TEGRANANO_LOG_POLICY = 9,
  TEGRANANO_PROF_POLICY = 10,
  RPI3_POLICY = 11,
  MAX_POLICY = 12
};


void sig_handler (int sig, siginfo_t *info, void *extra);
bool end_loop = false; //states if the controller has to be terminated

int main (int argc, char* argv[]) {
#ifdef PROFILING_CALLS
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
 
  unsigned int policy_id = NO_POLICY; 
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
        std::cerr << std::endl << "USAGE: " << argv[0] << " [-p POLICY_ID]" << std::endl;
        std::cerr << std::endl << "Supported modes:" << std::endl;
        std::cerr << "0 - Only application monitor on Odroid (DEFAULT)" << std::endl;
        std::cerr << "1 - Odroid HW sensors" << std::endl;
        std::cerr << "2 - Odroid policy for fake multiboard exps" << std::endl;
        std::cerr << "3 - Odroid policy for fake multiboard exps + logs in CSV" << std::endl;
        std::cerr << "4 - Odroid Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
        std::cerr << "5 - Tegra policy for fake multiboard exps" << std::endl;
        std::cerr << "6 - Tegra policy for fake multiboard exps + logs in CSV" << std::endl;
        std::cerr << "7 - Tegra Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
        std::cerr << "8 - Tegra Nano policy for fake multiboard exps" << std::endl;
        std::cerr << "9 - Tegra Nano policy for fake multiboard exps + logs in CSV" << std::endl;
        std::cerr << "10- Tegra Nano Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
        std::cerr << "11- RPI3 policy == RPI3 Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
        exit(EXIT_FAILURE);
    }
  } while (next_option != -1);

  if(policy_id >= MAX_POLICY || policy_id<0){
    std::cerr << std::endl << "USAGE: " << argv[0] << " [-p POLICY_ID]" << std::endl;
    std::cerr << std::endl << "Supported modes:" << std::endl;
    std::cerr << "0 - Only application monitor on Odroid (DEFAULT)" << std::endl;
    std::cerr << "1 - Odroid HW sensors" << std::endl;
    std::cerr << "2 - Odroid policy for fake multiboard exps" << std::endl;
    std::cerr << "3 - Odroid policy for fake multiboard exps + logs in CSV" << std::endl;
    std::cerr << "4 - Odroid Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
    std::cerr << "5 - Tegra policy for fake multiboard exps" << std::endl;
    std::cerr << "6 - Tegra policy for fake multiboard exps + logs in CSV" << std::endl;
    std::cerr << "7 - Tegra Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
    std::cerr << "8 - Tegra Nano policy for fake multiboard exps" << std::endl;
    std::cerr << "9 - Tegra Nano policy for fake multiboard exps + logs in CSV" << std::endl;
    std::cerr << "10- Tegra Nano Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
    std::cerr << "11- RPI3 policy == RPI3 Profiling policy for fake multiboard exps + logs in CSV" << std::endl;
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
  if(policy_id == NO_POLICY){
    policy = new ApplMonitoringPolicy();
  } else if(policy_id == ONLY_SENSORS){
    policy = new ApplMonitoringPolicy(true);
  } else if(policy_id == ODROID_POLICY){
    policy = new OdroidPolicy();
  } else if(policy_id == ODROID_LOG_POLICY){
    policy = new OdroidPolicy("sys.csv", "appls.csv");
  } else if(policy_id == ODROID_PROF_POLICY){
    policy = new OdroidProfPolicy("sys.csv", "appls.csv");
  } else if(policy_id == TEGRA_POLICY){
    policy = new TegraPolicy();
  } else if(policy_id == TEGRA_LOG_POLICY){
    policy = new TegraPolicy("sys.csv", "appls.csv");
  } else if(policy_id == TEGRA_PROF_POLICY){
    policy = new TegraProfPolicy("sys.csv", "appls.csv");
  } else if(policy_id == TEGRANANO_POLICY){
    policy = new TegraNanoPolicy();
  } else if(policy_id == TEGRANANO_LOG_POLICY){
    policy = new TegraNanoPolicy("sys.csv", "appls.csv");
  } else if(policy_id == TEGRANANO_PROF_POLICY){
    policy = new TegraNanoProfPolicy("sys.csv", "appls.csv");
  } else if(policy_id == RPI3_POLICY){
    policy = new RPI3Policy("sys.csv", "appls.csv");
  } else {
    //unreachable else
    std::cout << "unreachable else"<< std::endl;
    exit(EXIT_FAILURE);
  }

#ifdef PROFILING_CALLS
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time_span1 = std::chrono::duration_cast<std::chrono::duration<double> >(t2 - t1);
    std::cout << "[PROFILING] Init controller: " << time_span1.count() << " s." << std::endl;
#endif

  std::cout << "press Ctrl+C in order to stop monitoring" << std::endl;
  std::cout << "monitoring start..." << std::endl;
  int i = 0;

  while (!end_loop) {
    std::cout << std::endl << "iteration " << i << std::endl;

#ifdef PROFILING_CALLS
    std::chrono::high_resolution_clock::time_point t3 = std::chrono::high_resolution_clock::now();
#endif
    //run policy
    policy->run(i);
#ifdef PROFILING_CALLS
    std::chrono::high_resolution_clock::time_point t4 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> time_span2 = std::chrono::duration_cast<std::chrono::duration<double> >(t4 - t3);
    std::cout << "[PROFILING] Run policy: " << time_span2.count() << " s." << std::endl;
#endif

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
    std::cout << "\nmonitoring stop." << std::endl;
    end_loop = true;
  }
}
