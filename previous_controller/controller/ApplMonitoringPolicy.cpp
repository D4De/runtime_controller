#include "ApplMonitoringPolicy.h"
#include <iostream>
#include "CGroupUtils.h"

ApplMonitoringPolicy::ApplMonitoringPolicy(bool sensorsEnabled) {
  this->appl_monitor = monitorInit(ODROID_NUM_OF_CPU_CORES);
  this->sensorsEnabled = sensorsEnabled;
  if(sensorsEnabled){
    this->sensors = new GetNode();
    this->utilization = new Utilization(ODROID_NUM_OF_CPU_CORES);
  }
}

ApplMonitoringPolicy::~ApplMonitoringPolicy(){  
  killAttachedApplications(this->appl_monitor);
  monitorDestroy(this->appl_monitor);
  if(sensorsEnabled){
    delete sensors;
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
    this->sensors->updateSensing(u);
    
    std::cout << "Temperatures (bigs and GPU): ";
    for(int j=0; j < 5; j++){
      std::cout << sensors->getCPUTemp(j) << " ";
    }
    std::cout << std::endl;
    std::cout << "Power consumption: " << sensors->getBigW() << " (big) " << sensors->getLittleW() << " (LITTLE)" << std::endl;        
    std::cout << "- last 5 sec mean: " << sensors->getBigWavg() << " (big) " << sensors->getLittleWavg() << " (LITTLE)" << std::endl;        
    
    std::cout << "CPU usage: ";
    for(int j=0; j < u.size(); j++){
      std::cout << u[j] << " ";
    }
    std::cout << std::endl;
    std::cout << "Freq: " << sensors->getCPUCurFreq(0) << " - " << sensors->getCPUCurFreq(4) << " - " << sensors->getGPUCurFreq() << std::endl;        
  }
}
