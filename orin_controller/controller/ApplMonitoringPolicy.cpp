#include "ApplMonitoringPolicy.h"
#include <iostream>
#include "OrinHWLayer.h"

#define CHANGEPERIOD 10
#define CHANGECPUGPU 15

ApplMonitoringPolicy::ApplMonitoringPolicy(bool utilEnabled) {
  //instantiate all necessary monitors and controllers
  this->appl_monitor = monitorInit(Utilization::readNumOfCores());
  this->utilEnabled = utilEnabled;
  if(this->utilEnabled){
    this->utilization = new Utilization();
  }
}

ApplMonitoringPolicy::~ApplMonitoringPolicy(){  
  //kill all attached applications
  killAttachedApplications(this->appl_monitor);
  //destroy all monitors and controllers
  monitorDestroy(this->appl_monitor);
  if(this->utilEnabled){
    delete this->utilization;
  }
}

void ApplMonitoringPolicy::run(int cycle){
  /* This function is called every cycle (1 sec). It has to be implemented as a 
     Observe-Decide-Act cycle. It means that all sensors/monitors have to be queried 
     ONLY ONCE (Observe phase), then based on these observation the Decide policy is 
     executed and after that all knobs are actuated (Act phase) again ONLY ONCE. 
     PAY ATTENTION: in order to perceive the effects of actuation it is necessary
     to wait for the subsequent cycle (or maybe a sequence of cycles based on the 
     controlled system!), thus the Decide policy has to be implemented as a finite
     state machine */

  //get the list of newly attached applications and print them on the screen
  std::vector<pid_t> newAppls = updateAttachedApplications(this->appl_monitor);
  
  if(newAppls.size()>0){    
    std::cout << "New applications: ";
    for(int i=0; i < newAppls.size(); i++)
      std::cout << newAppls[i] << " ";
    std::cout << std::endl;
  }
  
  /* print all running applications */ 
  printAttachedApplications(this->appl_monitor);

  /* if enable, get and print CPU utilization infos*/
  if(this->utilEnabled){
    //DO NOTE that this function has to be called only once
    std::vector<int> u = this->utilization->getCPUUtilization();  
    
    std::cout << "CPU usage: ";
    for(int j=0; j < u.size(); j++){
      std::cout << u[j] << " ";
    }
    std::cout << std::endl;
  }

  /* this is an example of decision strategy to change the mapping of all 
     the connected applications every CHANGEPERIOD loops. Applications
     are moved from core 0 to 1 and viceversa */
  if(cycle%CHANGEPERIOD == 0){
    std::vector<int> cores;
    std::cout << "CHANGE MAPPING MAPPING!" << std::endl;
    if(cycle/CHANGEPERIOD%2 == 0){
      cores.push_back(0);    
    }else{
      cores.push_back(1);    
    }
    for(int i=0; i < appl_monitor->nAttached; i++){
      UpdateCpuSet(this->appl_monitor, appl_monitor->appls[i].pid, cores);
    } 
  }
  /* this is an example of decision strategy that checks the throughput 
     requirement received by the application and sends a parameter to all the 
     connected applications every CHANGECPUGPU loops. Check in the application
     loop how the throughput requirement is set and this parameter is read */
  if(cycle%CHANGECPUGPU == 0){
    for(int i=0; i < appl_monitor->nAttached; i++){
      data_t* appldataRef = monitorPtrRead(appl_monitor->appls[i].segmentId);
      double req_thr = getReqThroughput(appldataRef);
      bool gpu = getUseGPU(appldataRef);
      gpu = !gpu;
      setUseGPU(appldataRef, gpu);
      std::cout << "Application " << appl_monitor->appls[i].name << " required throughput: " 
                << req_thr << " gpu value set to " << gpu << std::endl;
    } 
  }
}
