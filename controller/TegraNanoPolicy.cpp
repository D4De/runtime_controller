#include "TegraNanoPolicy.h"
#include <iostream>
#include <chrono>
#include <set>
#include "CGroupUtils.h"

TegraNanoPolicy::TegraNanoPolicy() {
  this->appl_monitor = monitorInit(TEGRA_NUM_OF_CPU_CORES);
  this->sensors = new GetNode_TegraNano();
  this->utilization = new Utilization(TEGRA_NUM_OF_CPU_CORES);
  this->sensors->setAutoCPUFreqScaling(false);
  this->availablecpufreqs = this->sensors->getAvailableCPUFreq(0);
    
  this->csvSysFilename = ""; 
  this->csvSysFile = NULL;
  this->csvApplFilename = ""; 
  this->csvApplFile = NULL;
}

TegraNanoPolicy::TegraNanoPolicy(std::string csvSysFilename, std::string csvApplFilename) : TegraNanoPolicy() {
  this->csvSysFilename = csvSysFilename; 
  this->csvSysFile = new std::ofstream(csvSysFilename.c_str());
  if (! this->csvSysFile->is_open()) {
    std::cout << "Cannot open System CSV file" << std::endl;
    exit(0);
  }
  *this->csvSysFile << "TIME;ITERATION;POWER;FREQ;";
  for(int i=0; i<TEGRA_NUM_OF_CPU_CORES; i++)
    *this->csvSysFile << "UTIL" << i << ";";

  *this->csvSysFile << "INW;GPUW;CPUW;" << std::endl;

  this->csvApplFilename = csvApplFilename; 
  this->csvApplFile = new std::ofstream(csvApplFilename.c_str());
  if (! this->csvApplFile->is_open()) {
    std::cout << "Cannot open Applications CSV file" << std::endl;
    exit(0);
  }
}

TegraNanoPolicy::~TegraNanoPolicy(){  
  killAttachedApplications(this->appl_monitor);
  monitorDestroy(this->appl_monitor);
  this->sensors->setAutoCPUFreqScaling(true);

  delete this->sensors;
  delete this->utilization;
  if(this->csvSysFile){
    this->csvSysFile->close();
    delete this->csvSysFile;
  }
  if(this->csvApplFile){
    this->csvApplFile->close();
    delete this->csvApplFile;
  }
}

void TegraNanoPolicy::run(int cycle){
  //update status
  std::vector<pid_t> newAppls = updateAttachedApplications(this->appl_monitor);
  std::vector<int> u = utilization->getCPUUtilization();  
  this->sensors->updateSensing();

  int curr_freq; //DO NOTE 1 cluster GPU is not used, big cluster is not present on the board.
  curr_freq = sensors->getCPUCurScalFreq(TEGRA_LITTLE_CORE_ID);
  
  //report current status
  if(newAppls.size()>0){    
    std::cout << "New applications: ";
    for(int i=0; i < newAppls.size(); i++)
      std::cout << newAppls[i] << " ";
    std::cout << std::endl;
  }  
  printAttachedApplications(this->appl_monitor);
  
//  std::cout << "Power consumption: " << sensors->getBigW() << " (big) " << sensors->getLittleW() << " (LITTLE)" << std::endl;        
  
  std::cout << "CPU usage: ";
  for(int j=0; j < u.size(); j++){
    std::cout << u[j] << " ";
  }
  std::cout << std::endl;
  
  std::cout << "Freqs (current): " << sensors->getCPUCurFreq(TEGRA_LITTLE_CORE_ID) << std::endl;        
  std::cout << "Freqs (scaling): " << curr_freq << std::endl;        
  
  if(this->csvSysFile){
    std::string currtime;
    //get current time
    std::chrono::time_point<std::chrono::system_clock> curr_time = std::chrono::system_clock::now();
    std::time_t curr_time_c = std::chrono::system_clock::to_time_t(curr_time);
    currtime = std::string(std::ctime(&curr_time_c));
    currtime = currtime.substr(0, currtime.size()-1);

    float currpower = this->sensors->getChipW();    

    *this->csvSysFile << currtime << ";" << cycle << ";" << currpower << ";" << curr_freq << ";";
    for(int i=0; i<TEGRA_NUM_OF_CPU_CORES; i++)
      *this->csvSysFile << u[i] << ";";
    
    *this->csvSysFile << this->sensors->getVddInW() << ";" << this->sensors->getGpuW() << ";" << this->sensors->getCpuW() << ";";

    *this->csvSysFile << std::endl;
  }
  
  //run resource management policy
  std::set<pid_t> alreadyAnalyzedPid;
  int target_freq;
  target_freq = TEGRA_LITTLE_MIN_FREQ; 
  //number of sequential and multithreaded applications per cluster
  std::vector<pid_t> st_appls_count;
  std::vector<pid_t> mt_appls_count;
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
  std::map<pid_t, TegraNanoPolicy::appl_state>::iterator erase_it = appl_info.begin(); 
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
        std::cout << "GPU mapping not implemented yet; big cluster is not present!" << std::endl;
        exit(0);      
      }
      appl_info[curr_pid].mt = (appl_monitor->appls[i].maxThreads >1);
      new_appl = true;
    } 
    
    //update curr throughput and corresponding requirement for ALL applications
    appl_info[curr_pid].thr = getCurrThroughput(&temp_data);
    appl_info[curr_pid].thr_ref = getReqThroughput(&temp_data);
      
    //count how many sequential/multithreaded appls
    if(appl_info[curr_pid].mt)
      mt_appls_count.push_back(curr_pid);
    else
      st_appls_count.push_back(curr_pid);        

    //identify target frequency for the cluster the application is mapped on based on the current throughput, quota and frequency
    int my_target_freq;
    if(appl_info[curr_pid].thr!=0){
      my_target_freq = curr_freq * appl_info[curr_pid].thr_ref / appl_info[curr_pid].thr;
      my_target_freq = my_target_freq * (1.0/appl_info[curr_pid].thr - temp_data.usleepTime/1000000.0) * appl_info[curr_pid].thr;
    } else {
      if(appl_info[curr_pid].mapping == MAP_LITTLE_ID)
        my_target_freq = TEGRA_LITTLE_MAX_FREQ; //during first iteration I don't have any computed throughput so I put an high frequency to get faster an initial throughput 
      else { 
        std::cout << "GPU mapping not implemented yet; big cluster is not present!" << std::endl;
        exit(0); 
      }     
    }
      
    
    if(appl_info[curr_pid].mapping == MAP_LITTLE_ID) {
      /*on Jetson nano there is no constant step between neighbour frequency levels. so we have to iterate to find the first interesting value...*/
      int k, roundedFreq;
      roundedFreq = this->availablecpufreqs[0];
      k=0;
      while(roundedFreq<my_target_freq && k<availablecpufreqs.size()){
        roundedFreq = this->availablecpufreqs[k];
        k++;
      }
      my_target_freq = roundedFreq;
    } else { 
      std::cout << "GPU mapping not implemented yet; big cluster is not present!" << std::endl;
      exit(0); 
    }    
    //update cluster frequency if the one required by the current application is higher
    if(my_target_freq > target_freq){
      target_freq = my_target_freq;
    }   

    std::cout << "appl1: " << appl_info[curr_pid].thr_ref << " " << appl_info[curr_pid].thr << " " << curr_freq << " " << my_target_freq << std::endl;
    std::cout << "appl2: " << 1.0/appl_info[curr_pid].thr_ref << " " << 1.0/appl_info[curr_pid].thr << " " << temp_data.usleepTime << std::endl;
  } 
  
  //In a second csv file we save the data about the applications. they have to be postprocessed to be attached to the other system file
  if(this->csvApplFile){
    *this->csvApplFile << cycle << ";";
    for(std::map<pid_t, TegraNanoPolicy::appl_state>::iterator mapIt = appl_info.begin(); mapIt != appl_info.end(); mapIt++) {
      *this->csvApplFile << mapIt->first << ";" << mapIt->second.thr << ";" << mapIt->second.mapping << ";";  
    }
    *this->csvApplFile << std::endl;
  }
  
  //intra-cluster mapping (allocation of cores for applications running on the same cluster)
  //since there are only 4 cores per cluster I have identified a set of possible situations 
  //and for each of them a fixed mapping solution is proposed   
  if(new_appl || ended_appl) {
    std::cout << "LITTLE " << st_appls_count.size() << " " << mt_appls_count.size() << std::endl;
    //analyze LITTLE workload
    if(st_appls_count.size()>0 && mt_appls_count.size()==0) { //N sequential appls
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      for(int i=0; i<st_appls_count.size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[i], core);
    } else if(st_appls_count.size()==0 && mt_appls_count.size()==1) { //1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[0], core);    
    } else if(st_appls_count.size()==1 && mt_appls_count.size()==1) { //1 sequential appl + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      UpdateCpuSet(this->appl_monitor, st_appls_count[0], core);    
      core.clear();
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[0], core);       
    } else if(st_appls_count.size()==0 && mt_appls_count.size()==2) { //2 multithreaded appls
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[0], core);    
      core.clear();
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[1], core);       
    } else if(st_appls_count.size()==2 && mt_appls_count.size()==1) { //2 sequential appls + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[0], core);    
      core.clear();
      core.push_back(2);
      core.push_back(3);
      for(int i=0; i<st_appls_count.size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[i], core);    
    } else if(st_appls_count.size()>2 && mt_appls_count.size()==1) { //N sequential appls + 1 multithreaded appl
      std::vector<int> core;
      core.push_back(0);
      core.push_back(1);
      core.push_back(2);
      core.push_back(3);
      UpdateCpuSet(this->appl_monitor, mt_appls_count[0], core);    
      for(int i=0; i<st_appls_count.size(); i++)    
        UpdateCpuSet(this->appl_monitor, st_appls_count[i], core);
    } else if(st_appls_count.size()!=0 || mt_appls_count.size()!=0){
      std::cout << "not supported mapping config " << std::endl;
      exit(0);
    }    
  }
    
  if(!new_appl) { //DO NOTE: if an application entered the system do not apply DVFS since we need 1 cycle to profile
    if(target_freq > TEGRA_LITTLE_MAX_FREQ)
      target_freq = TEGRA_LITTLE_MAX_FREQ;
    else if(target_freq < TEGRA_LITTLE_MIN_FREQ)
      target_freq = TEGRA_LITTLE_MIN_FREQ;

    if(target_freq != curr_freq){
      std::cout << "---> change freq on LITTLE: " << target_freq << std::endl;
      sensors->setCPUFreq(TEGRA_LITTLE_CORE_ID, target_freq);    
    }
  }
}
