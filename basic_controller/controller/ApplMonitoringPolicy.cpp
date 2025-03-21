#include "ApplMonitoringPolicy.h"
#include <iostream>
#include "CGroupUtils.h"
#include "MappingConsts.h"

#define CHANGEPERIOD 10

ApplMonitoringPolicy::ApplMonitoringPolicy(bool sensorsEnabled) {
  this->appl_monitor = monitorInit(NUM_OF_CORES);
  this->sensorsEnabled = sensorsEnabled;
  if(sensorsEnabled){
    this->utilization = new Utilization(NUM_OF_CORES);
  }
}

ApplMonitoringPolicy::~ApplMonitoringPolicy(){  
  killAttachedApplications(this->appl_monitor);
  monitorDestroy(this->appl_monitor);
  if(sensorsEnabled){
    delete utilization;
  }
}

void ApplMonitoringPolicy::run(int cycle){
  std::vector<pid_t> newAppls = updateAttachedApplications(this->appl_monitor);
  
  if(newAppls.size()>0){    
    std::cout << "New applications: ";
    for(int i=0; i < newAppls.size(); i++)
      std::cout << newAppls[i] << " ";

    std::cout << std::endl;

  }
  
  //run resource management policy
  printAttachedApplications(this->appl_monitor);

  if(sensorsEnabled){
    std::vector<int> u = utilization->getCPUUtilization();  
    
    std::cout << "CPU usage: ";
    for(int j=0; j < u.size(); j++){
      std::cout << u[j] << " ";
    }
    std::cout << std::endl;
  }
  
  if(cycle%CHANGEPERIOD == 0){
    std::vector<int> cores;
    std::cout << "CAMBIO MAPPING!" << std::endl;
    if(cycle/CHANGEPERIOD%2==0){
      cores.push_back(0);    
    }else{
      cores.push_back(1);    
    }
    for(int i=0; i < appl_monitor->nAttached; i++){
      UpdateCpuSet(this->appl_monitor, appl_monitor->appls[i].pid, cores);
    }
 
  }
}
