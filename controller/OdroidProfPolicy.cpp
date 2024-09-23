#include "OdroidProfPolicy.h"
#include <iostream>
#include <chrono>
#include <set>
#include "CGroupUtils.h"

#define BIG_FREQ 1600
#define LITTLE_FREQ 200

OdroidProfPolicy::OdroidProfPolicy() {
  this->appl_monitor = monitorInit(ODROID_NUM_OF_CPU_CORES);
  this->sensors = new GetNode(! GetNode::NO_POWER_SENSORS, ! GetNode::NO_SMART_POWER);
  this->utilization = new Utilization(ODROID_NUM_OF_CPU_CORES);

  this->sensors->setCPUFreq(ODROID_LITTLE_CORE_ID, LITTLE_FREQ);
  this->sensors->setCPUFreq(ODROID_BIG_CORE_ID, BIG_FREQ);

  this->csvSysFilename = ""; 
  this->csvSysFile = NULL;
  this->csvApplFilename = ""; 
  this->csvApplFile = NULL;
}

OdroidProfPolicy::OdroidProfPolicy(std::string csvSysFilename, std::string csvApplFilename) : OdroidProfPolicy() {
  this->csvSysFilename = csvSysFilename; 
  this->csvSysFile = new std::ofstream(csvSysFilename.c_str());
  if (! this->csvSysFile->is_open()) {
    std::cout << "Cannot open System CSV file" << std::endl;
    exit(0);
  }
  *this->csvSysFile << "TIME;ITERATION;POWER;FREQ_B;FREQ_L;";
  for(int i=0; i<ODROID_NUM_OF_CPU_CORES; i++)
    *this->csvSysFile << "UTIL" << i << ";";
  *this->csvSysFile << std::endl;

  this->csvApplFilename = csvApplFilename; 
  this->csvApplFile = new std::ofstream(csvApplFilename.c_str());
  if (! this->csvApplFile->is_open()) {
    std::cout << "Cannot open Applications CSV file" << std::endl;
    exit(0);
  }
}

OdroidProfPolicy::~OdroidProfPolicy(){  
  killAttachedApplications(this->appl_monitor);
  monitorDestroy(this->appl_monitor);
  delete sensors;
  delete utilization;
  sensors->setCPUFreq(ODROID_LITTLE_CORE_ID, ODROID_LITTLE_REF_FREQ);    
  sensors->setCPUFreq(ODROID_BIG_CORE_ID, ODROID_BIG_REF_FREQ);
  if(this->csvSysFile){
    this->csvSysFile->close();
    delete this->csvSysFile;
  }
  if(this->csvApplFile){
    this->csvApplFile->close();
    delete this->csvApplFile;
  }
}

void OdroidProfPolicy::run(int cycle){
  //update status
  std::vector<pid_t> newAppls = updateAttachedApplications(this->appl_monitor);
  std::vector<int> u = utilization->getCPUUtilization();  
  this->sensors->updateSensing(u);

  int curr_freqs[2]; //DO NOTE 2 clusters 0=LITTLE and 1=big. GPU is not used.
  curr_freqs[MAP_LITTLE_ID] = sensors->getCPUCurFreq(ODROID_LITTLE_CORE_ID);
  curr_freqs[MAP_BIG_ID] = sensors->getCPUCurFreq(ODROID_BIG_CORE_ID);
  
  //report current status
  if(newAppls.size()>0){    
    std::cout << "New applications: ";
    for(int i=0; i < newAppls.size(); i++)
      std::cout << newAppls[i] << " ";
    std::cout << std::endl;
  }  
  printAttachedApplications(this->appl_monitor);
  
  std::cout << "Power consumption: " << sensors->getBigW() << " (big) " << sensors->getLittleW() << " (LITTLE)" << std::endl;        
  
  std::cout << "CPU usage: ";
  for(int j=0; j < u.size(); j++){
    std::cout << u[j] << " ";
  }
  std::cout << std::endl;
  
  std::cout << "Freqs: " << " - " << curr_freqs[MAP_LITTLE_ID] << " - " << curr_freqs[MAP_BIG_ID] << std::endl;        
  
  if(this->csvSysFile){
    std::string currtime;
    //get current time
    std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
    std::time_t curr_time_c = std::chrono::system_clock::to_time_t(curr_time);
    currtime = std::string(std::ctime(&curr_time_c));
    currtime = currtime.substr(0, currtime.size()-1);

    float currpower = this->sensors->getBoardW();    

    *this->csvSysFile << currtime << ";" << cycle << ";" << currpower << ";" << curr_freqs[MAP_BIG_ID] << ";" << curr_freqs[MAP_LITTLE_ID] << ";";
    for(int i=0; i<ODROID_NUM_OF_CPU_CORES; i++)
      *this->csvSysFile << u[i] << ";";
    *this->csvSysFile << std::endl;
  }
  
  //run resource management policy
  std::set<pid_t> alreadyAnalyzedPid;
  int target_freqs[2];
  target_freqs[MAP_LITTLE_ID] = ODROID_LITTLE_MIN_FREQ; 
  target_freqs[MAP_BIG_ID] = ODROID_BIG_MIN_FREQ; 
  //number of sequential and multithreaded applications per cluster
  std::vector<pid_t> st_appls_count[2];
  std::vector<pid_t> mt_appls_count[2];
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
  std::map<pid_t, OdroidProfPolicy::appl_state>::iterator erase_it = appl_info.begin(); 
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
      if(appl_info[curr_pid].mapping != MAP_LITTLE_ID && appl_info[curr_pid].mapping != MAP_BIG_ID){
        std::cout << "GPU mapping not implemented yet" << std::endl;
        exit(0);      
      }
      appl_info[curr_pid].mt = (appl_monitor->appls[i].maxThreads >1);
      new_appl = true;
    } 
    
    //update curr throughput and corresponding requirement for ALL applications
    appl_info[curr_pid].thr = getCurrThroughput(&temp_data);
    appl_info[curr_pid].thr_ref = getReqThroughput(&temp_data);
    //count how many sequential/multithreaded appls on big and  on LITTLE
    if(appl_info[curr_pid].mt)
      mt_appls_count[appl_info[curr_pid].mapping].push_back(curr_pid);
    else
      st_appls_count[appl_info[curr_pid].mapping].push_back(curr_pid);        
  }
  
  //In a second csv file we save the data about the applications. they have to be postprocessed to be attached to the other system file
  if(this->csvApplFile){
    *this->csvApplFile << cycle << ";";
    for(std::map<pid_t, OdroidProfPolicy::appl_state>::iterator mapIt = appl_info.begin(); mapIt != appl_info.end(); mapIt++) {
      *this->csvApplFile << mapIt->first << ";" << mapIt->second.thr << ";" << mapIt->second.mapping << ";";  
    }
    *this->csvApplFile << std::endl;
  }
  
  //intra-cluster mapping (allocation of cores for applications running on the same cluster)
  //since there are only 4 cores per cluster I have identified a set of possible situations 
  //and for each of them a fixed mapping solution is proposed   
  if(new_appl || ended_appl) {
    std::cout << "LITTLE " << st_appls_count[MAP_LITTLE_ID].size() << " " << mt_appls_count[MAP_LITTLE_ID].size() << std::endl;
    std::cout << "big    " << st_appls_count[MAP_BIG_ID].size() << " " << mt_appls_count[MAP_BIG_ID].size() << std::endl;
    //analyze LITTLE workload
    if(st_appls_count[MAP_LITTLE_ID].size()>0 && mt_appls_count[MAP_LITTLE_ID].size()==0) { //N sequential appls
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      for(int i=0; i<st_appls_count[MAP_LITTLE_ID].size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[MAP_LITTLE_ID][i],core);
    } else if(st_appls_count[MAP_LITTLE_ID].size()==0 && mt_appls_count[MAP_LITTLE_ID].size()==1) { //1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_LITTLE_ID][0],core);    
    } else if(st_appls_count[MAP_LITTLE_ID].size()==1 && mt_appls_count[MAP_LITTLE_ID].size()==1) { //1 sequential appl + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      UpdateCpuSet(this->appl_monitor, st_appls_count[MAP_LITTLE_ID][0],core);    
      core.clear();
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_LITTLE_ID][0],core);       
    } else if(st_appls_count[MAP_LITTLE_ID].size()==0 && mt_appls_count[MAP_LITTLE_ID].size()==2) { //2 multithreaded appls
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_LITTLE_ID][0],core);    
      core.clear();
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_LITTLE_ID][1],core);       
    } else if(st_appls_count[MAP_LITTLE_ID].size()>1 && mt_appls_count[MAP_LITTLE_ID].size()==1) { //N sequential appls + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_LITTLE_ID][0],core);    
      core.clear();
      core.push_back(2);
      core.push_back(3);
      for(int i=0; i<st_appls_count[MAP_LITTLE_ID].size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[MAP_LITTLE_ID][i],core);
    } else if(st_appls_count[MAP_LITTLE_ID].size()!=0 || mt_appls_count[MAP_LITTLE_ID].size()!=0){
      std::cout << "not supported mapping config " << std::endl;
      exit(0);
    }
    
    //analyze big workload:
    if(st_appls_count[MAP_BIG_ID].size()>0 && mt_appls_count[MAP_BIG_ID].size()==0) { //N sequential appls
      std::vector<int> core;
      core.push_back(4);
      core.push_back(5);
      core.push_back(6);
      core.push_back(7);
      for(int i=0; i<st_appls_count[MAP_BIG_ID].size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[MAP_BIG_ID][i],core);
    } else if(st_appls_count[MAP_BIG_ID].size()==0 && mt_appls_count[MAP_BIG_ID].size()==1) { //1 multithreaded appl
      std::vector<int> core;
      core.push_back(4);
      core.push_back(5);
      core.push_back(6);
      core.push_back(7);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_BIG_ID][0],core);    
    } else if(st_appls_count[MAP_BIG_ID].size()==1 && mt_appls_count[MAP_BIG_ID].size()==1) { //1 sequential appl + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(4);
      UpdateCpuSet(this->appl_monitor, st_appls_count[MAP_BIG_ID][0],core);    
      core.clear();
      core.push_back(5);
      core.push_back(6);
      core.push_back(7);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_BIG_ID][0],core);       
    } else if(st_appls_count[MAP_BIG_ID].size()==0 && mt_appls_count[MAP_BIG_ID].size()==2) { //2 multithreaded appls
      std::vector<int> core;
      core.push_back(4);
      core.push_back(5);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_BIG_ID][0],core);    
      core.clear();
      core.push_back(6);
      core.push_back(7);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_BIG_ID][1],core);       
    } else if(st_appls_count[MAP_BIG_ID].size()>1 && mt_appls_count[MAP_BIG_ID].size()==1) { //N sequential appls + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(4);
      core.push_back(5);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[MAP_BIG_ID][0],core);    
      core.clear();
      core.push_back(6);
      core.push_back(7);
      for(int i=0; i<st_appls_count[MAP_BIG_ID].size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[MAP_BIG_ID][i],core);
    } else if(st_appls_count[MAP_BIG_ID].size()!=0 || mt_appls_count[MAP_BIG_ID].size()!=0){
      std::cout << "not supported mapping config " << std::endl;
      exit(0);
    }
  }
}
