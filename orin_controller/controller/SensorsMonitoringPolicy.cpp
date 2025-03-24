#include "SensorsMonitoringPolicy.h"
#include <iostream>
#define CHANGEFREQ 5

SensorsMonitoringPolicy::SensorsMonitoringPolicy() {
  //instantiate all necessary monitors and controllers
  this->utilization = new Utilization();
  this->appl_monitor = monitorInit(this->utilization->getNumOfCores());
  this->sensors = new OrinHWLayer();

  //enable frequency control for both CPU and GPU
  this->sensors->enableFrequencyControl();
  
  //Get and print available CPU frequencies
  this->cpu_freqs = this->sensors->getAvailableCPUFreqs();
  std::cout << std::endl << "CPU available frequencies: ";
  for(int i = 0; i < this->cpu_freqs.size(); i++)
    std::cout << this->cpu_freqs[i] << " "; 
  std::cout << std::endl;

  //Get and print available GPU frequencies
  this->gpu_freqs = this->sensors->getAvailableGPUFreqs();
  std::cout << std::endl << "GPU available frequencies: ";
  for(int i = 0; i < this->gpu_freqs.size(); i++)
    std::cout << this->gpu_freqs[i] << " "; 
  std::cout << std::endl << std::endl;
}

SensorsMonitoringPolicy::~SensorsMonitoringPolicy(){  
  //kill all attached applications
  killAttachedApplications(this->appl_monitor);
  //destroy all monitors and controllers
  monitorDestroy(this->appl_monitor);
  delete this->utilization;
  delete this->sensors;
}

void SensorsMonitoringPolicy::run(int cycle){
  /* This function is called every cycle (1 sec). It has to be implemented as a 
     Observe-Decide-Act cycle. It means that all sensors/monitors have to be queried 
     ONLY ONCE (Observe phase), then based on these observation the Decide policy is 
     executed and after that all knobs are actuated (Act phase) again ONLY ONCE. 
     PAY ATTENTION: in order to perceive the effects of actuation it is necessary
     to wait for the subsequent cycle (or maybe a sequence of cycles based on the 
     controlled system!), thus the Decide policy has to be implemented as a finite
     state machine */

  //DO NOTE that these two functions have to be called only ones at the beginning
  std::vector<int> u = this->utilization->getCPUUtilization();  
  this->sensors->updateSensing();

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

  /* print CPU utilization infos*/
  std::cout << "CPU usage: ";
  for(int j=0; j < u.size(); j++){
    std::cout << u[j] << " ";
  }
  std::cout << std::endl;

  /* print sensors data (power consumption) */
  std::cout << "Power consumption: " << this->sensors->getVddInW() << std::endl;
  
  /* print CPU frequencies */
  int f;
  std::cout << "CPU frequencies: ";
  for(int j = 0; j < ORIN_NUM_OF_CORES; j++){
    f = this->sensors->getCPUCurFreq(j);
    std::cout << f << " "; 
  }
  std::cout << std::endl;

  /* print GPU frequency */
  std::cout << "GPU frequencies: ";
  f = this->sensors->getGPUCurFreq();
  std::cout << f << std::endl;

  /* example of policy changing the CPU frequency every CHANGEFREQ cycles */
  if(cycle%CHANGEFREQ==0){
    f = this->cpu_freqs[(cycle/CHANGEFREQ)%this->cpu_freqs.size()];
    std::cout << "CPU frequency change to " << f << std::endl;
    this->sensors->setCPUFreq(f);
  }
}
