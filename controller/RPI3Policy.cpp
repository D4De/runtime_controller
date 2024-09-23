#include "RPI3Policy.h"
#include <iostream>
#include <chrono>
#include <set>
#include <stdexcept>
#include "CGroupUtils.h"


RPI3Policy::RPI3Policy() {
  this->appl_monitor = monitorInit(RPI3_NUM_OF_CPU_CORES);
  this->utilization = new Utilization(RPI3_NUM_OF_CPU_CORES);

  this->smartGauge = new SmartGauge();
  if(!this->smartGauge->initDevice()){
    exit(1);
  }
  //dummy read. first read returns always -1...
  this->smartGauge->getWatt();

  this->csvSysFilename = ""; 
  this->csvSysFile = NULL;
  this->csvApplFilename = ""; 
  this->csvApplFile = NULL;
}

RPI3Policy::RPI3Policy(std::string csvSysFilename, std::string csvApplFilename) : RPI3Policy() {
  this->csvSysFilename = csvSysFilename; 
  this->csvSysFile = new std::ofstream(csvSysFilename.c_str());
  if (! this->csvSysFile->is_open()) {
    std::cout << "Cannot open System CSV file" << std::endl;
    exit(0);
  }
  *this->csvSysFile << "TIME;ITERATION;POWER;";
  for(int i=0; i<RPI3_NUM_OF_CPU_CORES; i++)
    *this->csvSysFile << "UTIL" << i << ";";
  *this->csvSysFile << std::endl;

  this->csvApplFilename = csvApplFilename; 
  this->csvApplFile = new std::ofstream(csvApplFilename.c_str());
  if (! this->csvApplFile->is_open()) {
    std::cout << "Cannot open Applications CSV file" << std::endl;
    exit(0);
  }
}

RPI3Policy::~RPI3Policy(){  
  killAttachedApplications(this->appl_monitor);
  monitorDestroy(this->appl_monitor);
  delete utilization;
  delete this->smartGauge;
    
  if(this->csvSysFile){
    this->csvSysFile->close();
    delete this->csvSysFile;
  }
  if(this->csvApplFile){
    this->csvApplFile->close();
    delete this->csvApplFile;
  }
}

void RPI3Policy::run(int cycle){
  //update status
  std::vector<pid_t> newAppls = updateAttachedApplications(this->appl_monitor);
  std::vector<int> u = utilization->getCPUUtilization();  
 
  //report current status
  if(newAppls.size()>0){    
    std::cout << "New applications: ";
    for(int i=0; i < newAppls.size(); i++)
      std::cout << newAppls[i] << " ";
    std::cout << std::endl;
  }  
  printAttachedApplications(this->appl_monitor);
  
  float curr_power = -1;
  
  try {
    curr_power = this->smartGauge->getWatt();
  } catch(std::runtime_error e1 ) {
    std::cout << "ERROR ACCESSING THE SMART POWER" << std::endl;
  }
  
  std::cout << "Power consumption: " << curr_power << std::endl;        
  
  std::cout << "CPU usage: ";
  for(int j=0; j < u.size(); j++){
    std::cout << u[j] << " ";
  }
  std::cout << std::endl;
    
  if(this->csvSysFile){
    std::string currtime;
    //get current time
    std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
    std::time_t curr_time_c = std::chrono::system_clock::to_time_t(curr_time);
    currtime = std::string(std::ctime(&curr_time_c));
    currtime = currtime.substr(0, currtime.size()-1);

    *this->csvSysFile << currtime << ";" << cycle << ";" << curr_power << ";";
    for(int i=0; i<RPI3_NUM_OF_CPU_CORES; i++)
      *this->csvSysFile << u[i] << ";";
    *this->csvSysFile << std::endl;
  }
  
  //run resource management policy
  std::set<pid_t> alreadyAnalyzedPid;
  //events
  bool new_appl = false;
  bool ended_appl = false;

  //remove deatched applications info. DO NOTE; do it before updating with new appls 
  //since in the meanwhile I perform some other analysis on the currently running applications 
  
  //get the list of running applications to compute ended once by subtraction of sets
  for(int i=0; i < appl_monitor->nAttached; i++){
    pid_t curr_pid = appl_monitor->appls[i].pid;
    alreadyAnalyzedPid.insert(curr_pid);
  }
  //the iterator is not invalidated by the deletion!
  std::map<pid_t, RPI3Policy::appl_state>::iterator erase_it = appl_info.begin(); 
  while(erase_it != appl_info.end()){
    if (alreadyAnalyzedPid.find(erase_it->first) == alreadyAnalyzedPid.end()) {
      //actually erase data from the map
      erase_it = appl_info.erase(erase_it);      
      ended_appl = true;
    } else {
      ++erase_it;
    }   
  }

  //analyze running applications to update the local data structure and start analyzing 
  //the current situation
  for(int i=0; i < appl_monitor->nAttached; i++){
    //get data from shared memory
    pid_t curr_pid = appl_monitor->appls[i].pid;
    data_t temp_data = monitorRead(appl_monitor->appls[i].segmentId);
    
    //initialization of newly arrived applications
    if(appl_info.find(curr_pid) == appl_info.end()) {   
      //copy settings from the shared memory 
      appl_info[curr_pid].mapping = appl_monitor->appls[i].mapping;
      if(appl_info[curr_pid].mapping != MAP_LITTLE_ID){
        std::cout << "There is a single CPU cluster" << std::endl;
        exit(0);      
      }
      appl_info[curr_pid].mt = (appl_monitor->appls[i].maxThreads >1);
      new_appl = true;
    } 

    //update curr throughput and corresponding requirement for ALL applications
    appl_info[curr_pid].thr = getCurrThroughput(&temp_data);
    appl_info[curr_pid].thr_ref = getReqThroughput(&temp_data);    
  }
  
  //In a second csv file we save the data about the applications. they have to be postprocessed to be attached to the other system file
  if(this->csvApplFile){
    *this->csvApplFile << cycle << ";";
    for(std::map<pid_t, RPI3Policy::appl_state>::iterator mapIt = appl_info.begin(); mapIt != appl_info.end(); mapIt++) {
      *this->csvApplFile << mapIt->first << ";" << mapIt->second.thr << ";" << mapIt->second.mapping << ";";  
    }
    *this->csvApplFile << std::endl;
  }
}
