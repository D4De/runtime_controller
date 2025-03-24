#include "OrinHWLayer.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio> // sprintf
#include <cstdlib> // atoi
#include <fcntl.h>
#include <cstring>  // strncmp
#include <string>
#include <iostream> // cerr
#include <fstream>  // cerr

void readStringFromAFile(const char* filename, char* value) {
  /* read the string contained in a file (in the virtual filesystem) */
  FILE* fp;
  fp = fopen(filename, "r");
  if(fp) {
    fscanf(fp, "%s", value);
    fclose(fp);
  } else {
    std::cout << "Error while accessing " << filename << " in read mode" << std::endl;
  }
}

void writeStringIntoAFile(const char* filename, const char* value) {
  /* write a string into a file (of the virtual filesystem) */
  FILE* fp;
  fp = fopen(filename, "w");
  if(fp) {
    fprintf(fp, "%s", value);
    fclose(fp);
  } else {
    std::cout << "Error while accessing " << filename << " in write mode" << std::endl;
  }
}

int readIntFromAFile(const char* filename) {
  /* read an int value contained in a file (in the virtual filesystem) */
  FILE* fp;
  int value;
  fp = fopen(filename, "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    fclose(fp);
  } else {
    value = -1;
    std::cout << "Error while accessing " << filename << " in read mode" << std::endl;
  }
  return value;
}

float readFloatFromAFile(const char* filename) {
  /* read an int value contained in a file (in the virtual filesystem) */
  FILE* fp;
  float value;
  fp = fopen(filename, "r");
  if(fp) {
    fscanf(fp, "%f", &value);
    fclose(fp);
  } else {
    value = -1;
    std::cout << "Error while accessing " << filename << " in read mode" << std::endl;
  }
  return value;
}


void writeIntIntoAFile(const char* filename, const int value) {
  /* write an int value into a file (of the virtual filesystem) */
  FILE* fp;
  fp = fopen(filename, "w");
  if(fp) {
    fprintf(fp, "%d", value);
    fclose(fp);
  } else {
    std::cout << "Error while accessing " << filename << " in write mode" << std::endl;
  }
}

std::vector<int> readVectorOfIntFromAFile(const char* filename) {
  /* read a list of int values contained in a file (in the virtual filesystem) */
  std::vector<int> vec;
  FILE* fp;
  int value;
  fp = fopen(filename, "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    while(!feof(fp)) {
      vec.push_back(value);       
      fscanf(fp, "%d", &value);
    }
    fclose(fp);
  }
  return vec;    
}

OrinHWLayer::OrinHWLayer() {
  std::string path;
  //save original CPU frequency scaling configuration
  for(int i=0; i<NUM_OF_CPU_CLUSTERS; i++){
    path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_GOVERNOR;
    readStringFromAFile(path.c_str(), this->original_cpu_status[i].scaling_governor);
    path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MAX_FREQ;
    readStringFromAFile(path.c_str(), this->original_cpu_status[i].scaling_max_freq);
    path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MIN_FREQ;
    readStringFromAFile(path.c_str(), this->original_cpu_status[i].scaling_min_freq);
  } 
  //save original GPU frequency scaling configuration
  path = GPU_PATH + std::string(GPU_GOVERNOR);
  readStringFromAFile(path.c_str(), this->original_gpu_status.scaling_governor);
  path = GPU_PATH + std::string(GPU_MAX_FREQ);
  readStringFromAFile(path.c_str(), this->original_gpu_status.scaling_max_freq);
  path = GPU_PATH + std::string(GPU_MIN_FREQ);
  readStringFromAFile(path.c_str(), this->original_gpu_status.scaling_min_freq);

  //update the flag in the object to know which frequency control configuration is set
  this->frequencyControlEnabled = OrinHWLayer::NO_FREQUENCY_CONTROL;

  //read sensors for the first time  
  this->readSensors();

  //initialize attributes for power consumption
  this->vddInWavg[0] = this->vddInWs = this->getVddInW();
  for(int i = 1; i < POW_WINDOW_SIZE; i++)
    this->vddInWavg[i] = 0;
  this->slidingIndex = 1;
}

OrinHWLayer::~OrinHWLayer() {
  //restore original frequency configuration
  this->enableFrequencyControl(OrinHWLayer::NO_FREQUENCY_CONTROL);
}

void OrinHWLayer::printFrequencyControlConfig() {
  std::string path;
  char str[STRLEN+1];
  //read and print CPU frequency scaling configuration
  std::cout << "CPU frequency configuration:" << std::endl;
  for(int i=0; i<NUM_OF_CPU_CLUSTERS; i++){
    path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_GOVERNOR;
    readStringFromAFile(path.c_str(), str);
    std::cout << "cluster: " << i << " - gov: " << str;
    path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MAX_FREQ;
    readStringFromAFile(path.c_str(), str);
    std::cout << " - max: " << str;
    path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MIN_FREQ;
    readStringFromAFile(path.c_str(), str);
    std::cout << " - min: " << str << std::endl;
  } 
  std::cout << std::endl;
  //read and print GPU frequency scaling configuration
  path = GPU_PATH + std::string(GPU_GOVERNOR);
  readStringFromAFile(path.c_str(), str);
  std::cout << "CPU frequency configuration:" << std::endl << "* gov: " << str;
  path = GPU_PATH + std::string(GPU_MAX_FREQ);
  readStringFromAFile(path.c_str(), str);
  std::cout << " - max: " << str;
  path = GPU_PATH + std::string(GPU_MIN_FREQ);
  readStringFromAFile(path.c_str(), str);
  std::cout << " - min: " << str << std::endl;
}

void OrinHWLayer::enableFrequencyControl(frequencyControl_t config) {
  /*based on the received flag, this function sets the specified governor to 
    enable the controller to change the CPU frequency or restores the default
    frequency governor configuration read at the beginning of the controller execution */
  std::string path;
  //CPU configuration
  if(config == OrinHWLayer::CPU_FREQUENCY_CONTROL || config == OrinHWLayer::ALL_FREQUENCY_CONTROL){
    for(int i=0; i<NUM_OF_CPU_CLUSTERS; i++){
      path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_GOVERNOR;
      writeStringIntoAFile(path.c_str(), PERFORMANCE_GOVERNOR);
      path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MAX_FREQ;
      writeStringIntoAFile(path.c_str(), INITIAL_CPU_FREQUENCY);
      path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MIN_FREQ;
      writeStringIntoAFile(path.c_str(), INITIAL_CPU_FREQUENCY);
    } 
  } else { //TODO I may avoid to overwrite the same configuration if CPU freq control is already disabled
    for(int i=0; i<NUM_OF_CPU_CLUSTERS; i++){
      path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_GOVERNOR;
      writeStringIntoAFile(path.c_str(), this->original_cpu_status[i].scaling_governor);
      path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MAX_FREQ;
      writeStringIntoAFile(path.c_str(), this->original_cpu_status[i].scaling_max_freq);
      path = CPU_PATH + std::to_string(CLUSTER_REF_CORES[i]) + CPU_SCALING_MIN_FREQ;
      writeStringIntoAFile(path.c_str(), this->original_cpu_status[i].scaling_min_freq);
    }
  } 
  //GPU configuration
  if(config == OrinHWLayer::GPU_FREQUENCY_CONTROL || config == OrinHWLayer::ALL_FREQUENCY_CONTROL){
    path = GPU_PATH + std::string(GPU_GOVERNOR);
    writeStringIntoAFile(path.c_str(), PERFORMANCE_GOVERNOR);
    path = GPU_PATH + std::string(GPU_MAX_FREQ);
    writeStringIntoAFile(path.c_str(), INITIAL_GPU_FREQUENCY);
    path = GPU_PATH + std::string(GPU_MIN_FREQ);
    writeStringIntoAFile(path.c_str(), INITIAL_GPU_FREQUENCY);
  } else { //TODO I may avoid to overwrite the same configuration if GPU freq control is already disabled
    path = GPU_PATH + std::string(GPU_GOVERNOR);
    writeStringIntoAFile(path.c_str(), this->original_gpu_status.scaling_governor);
    path = GPU_PATH + std::string(GPU_MAX_FREQ);
    writeStringIntoAFile(path.c_str(), this->original_gpu_status.scaling_max_freq);
    path = GPU_PATH + std::string(GPU_MIN_FREQ);
    writeStringIntoAFile(path.c_str(), this->original_gpu_status.scaling_min_freq);
  }
  //set the status flag
  this->frequencyControlEnabled = config;
}

bool OrinHWLayer::isCPUActive(int cpuNum) {
  //get online status for a specific CPU core
  if(cpuNum==0) //the first core is always on
    return true;
  else if(cpuNum<0 || cpuNum > ORIN_NUM_OF_CORES){
    std::cout << "NOT valid core number" << std::endl;
    return false; //do note. it does not exist
  }
  
  std::string path = CPU_PATH + std::to_string(cpuNum) + CPU_ONLINE;
  int value = readIntFromAFile(path.c_str());
  return (value==1)?true:false;
}

int OrinHWLayer::getCPUCurFreq(int cpuNum) {
  //get current actual frequency of a specific CPU core
  if(cpuNum < 0 || cpuNum > ORIN_NUM_OF_CORES){
    std::cout << "NOT valid core number" << std::endl;
    return -1; //do note. it does not exist
  }

  std::string path = CPU_PATH + std::to_string(cpuNum) + CPUINFO_CURR_FREQ;
  int value = readIntFromAFile(path.c_str());
  return value;
}

int OrinHWLayer::getCPUCurScalFreq(int cpuNum) {
  //get configured frequency of a specific CPU core (it may be different from the actual one)
  if(cpuNum < 0 || cpuNum > ORIN_NUM_OF_CORES){
    std::cout << "NOT valid core number" << std::endl;
    return -1; //do note. it does not exist
  }

  std::string path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_CUR_FREQ;
  int value = readIntFromAFile(path.c_str());
  return value; 
}


int OrinHWLayer::getGPUCurFreq() {
  //get current actual frequency of the GPU
  std::string path = GPU_PATH + std::string(GPU_CURR_FREQ);
  int value = readIntFromAFile(path.c_str());
  return value;
}

std::vector<int> OrinHWLayer::getAvailableCPUFreqs(int cpuNum) {
  //get all available frequencies for a CPU core (indeed they are the same for all the cores)
  std::vector<int> freqs;
  if(cpuNum>=0 && cpuNum < ORIN_NUM_OF_CORES){
    std::string path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_AVAILABLE_FREQUENCIES;
    freqs =  readVectorOfIntFromAFile(path.c_str());
  } else {
    std::cout << "NOT valid core number" << std::endl;
  }
  return freqs;
}

std::vector<int> OrinHWLayer::getAvailableGPUFreqs() {
  //get all available frequencies for the GPU
  std::vector<int> freqs;
  std::string path = GPU_PATH + std::string(GPU_AVAILABLE_FREQUENCIES);
  freqs =  readVectorOfIntFromAFile(path.c_str()); 
  return freqs;
}

void OrinHWLayer::setCPUActive(int cpuNum, bool status) {
  //set online status for a specific CPU core
  //the first core is always on. IDs not in [1;ORIN_NUM_OF_CORES] are not valid
  if(cpuNum==0 || cpuNum<0 || cpuNum > ORIN_NUM_OF_CORES){
    std::cout << "NOT valid core number" << std::endl;
    return;
  }
  std::string path = CPU_PATH + std::to_string(cpuNum) + CPU_ONLINE;
  writeIntIntoAFile(path.c_str(), status==true?1:0);
}

void OrinHWLayer::setCPUCoreFreq(int cpuNum, int freq) {
  //set the frequency for a given CPU core (affecting the entire cluster)
  //the first core is always on. IDs not in [1;ORIN_NUM_OF_CORES] are not valid
  if(cpuNum<0 || cpuNum > ORIN_NUM_OF_CORES){
    std::cout << "NOT valid core number" << std::endl;
    return;
  }
  if(this->frequencyControlEnabled != OrinHWLayer::ALL_FREQUENCY_CONTROL &&
     this->frequencyControlEnabled != OrinHWLayer::CPU_FREQUENCY_CONTROL){
    std::cout << "CPU frequency control not enabled" << std::endl;
    return;
  }

  //DO NOTE: actually on the ARM cluster the userspace governor sometimes doesn't work. 
  //so the trick is to use the performance governor and set the same frequency for the 
  //max and min bound.
  //the bothering part is that it is relevant the order of writing of the min and max 
  //value since min cannot be > of max

  //read min scaling freq
  std::string path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_MIN_FREQ;
  int minvalue = readIntFromAFile(path.c_str());
  
  //read max scaling freq
  path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_MAX_FREQ;
  int maxvalue = readIntFromAFile(path.c_str());

  //set the frequency in the two files without causing intermediate inconsistent 
  //configurations (i.e. max freq file containing a value lower than the one in min freq file)
  if(freq < minvalue) {
    path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_MIN_FREQ;
    writeIntIntoAFile(path.c_str(), freq);
    path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_MAX_FREQ;
    writeIntIntoAFile(path.c_str(), freq);
  } else {
    path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_MAX_FREQ;
    writeIntIntoAFile(path.c_str(), freq);
    path = CPU_PATH + std::to_string(cpuNum) + CPU_SCALING_MIN_FREQ;
    writeIntIntoAFile(path.c_str(), freq);
  }
}

void OrinHWLayer::setCPUClusterFreq(int clusterNum, int freq) {
  /* Orin NX has 2 frequency clusters in the CPU multicore with 
     the same number of cores. So this function sets the frequency
     of the first core to tune the first cluster and of the
     last core to tune the frequency of the second cluster */
  if(clusterNum == 0)
    this->setCPUCoreFreq(CLUSTER_REF_CORES[0], freq);
  else if(clusterNum == 1)
    this->setCPUCoreFreq(CLUSTER_REF_CORES[1], freq);
  else
    std::cout << "NOT valid cluster number" << std::endl;
}

void OrinHWLayer::setCPUFreq(int freq) {
  /* set the frequency of both the clusters */  
  this->setCPUCoreFreq(CLUSTER_REF_CORES[0], freq);
  this->setCPUCoreFreq(CLUSTER_REF_CORES[1], freq);
}

void OrinHWLayer::setGPUFreq(int freq) {
  // set GPU frequency
  if(this->frequencyControlEnabled != OrinHWLayer::ALL_FREQUENCY_CONTROL &&
     this->frequencyControlEnabled != OrinHWLayer::GPU_FREQUENCY_CONTROL){
    std::cout << "GPU frequency control not enabled" << std::endl;
    return;
  }

  //DO NOTE: actually on the GPU the userspace governor sometimes doesn't work. 
  //so the trick is to use the performance governor and set the same frequency for the 
  //max and min bound.
  //the bothering part is that it is relevant the order of writing of the min and max 
  //value since min cannot be > of max

  //read min scaling freq
  std::string path = GPU_PATH + std::string(GPU_MIN_FREQ);
  int minvalue = readIntFromAFile(path.c_str());
  
  //read max scaling freq
  path = CPU_PATH + std::string(GPU_MAX_FREQ);
  int maxvalue = readIntFromAFile(path.c_str());

  //set the frequency in the two files without causing intermediate inconsistent 
  //configurations (i.e. max freq file containing a value lower than the one in min freq file)
  if(freq < minvalue) {
    path = GPU_PATH + std::string(GPU_MIN_FREQ);
    writeIntIntoAFile(path.c_str(), freq);
    path = GPU_PATH + std::string(GPU_MAX_FREQ);
    writeIntIntoAFile(path.c_str(), freq);
  } else {
    path = GPU_PATH + std::string(GPU_MAX_FREQ);
    writeIntIntoAFile(path.c_str(), freq);
    path = GPU_PATH + std::string(GPU_MIN_FREQ);
    writeIntIntoAFile(path.c_str(), freq);
  }
}

float OrinHWLayer::getVddInV() {
  // get last read vddInV
  return this->vddInV;
}

float OrinHWLayer::getVddInA() {
  // get last read vddInA
  return this->vddInA;
}

float OrinHWLayer::getVddInW() {
  // get last read/computed vddInW
  return this->vddInW;
}

void OrinHWLayer::readSensors(){
  //collect instantaneous values
  this->vddInV = readFloatFromAFile(VDD_IN_V) /1000; //to have V from mV
  this->vddInA = readFloatFromAFile(VDD_IN_A) /1000; //to have A from mA
  //compute W since it is not automatically computed by the driver
  this->vddInW = this->vddInA * this->vddInV; //in W
}

void OrinHWLayer::updateSensing() {
  // read from drivers
  this->readSensors();
  // update data structures for sliding windows
  this->vddInWs = this->vddInWs - this->vddInWavg[this->slidingIndex] + this->vddInW;
  this->vddInWavg[this->slidingIndex] = this->vddInW;
  this->slidingIndex = (this->slidingIndex+1)%POW_WINDOW_SIZE;
}

float OrinHWLayer::getVddInWavg() {
  // get average vddInW for the last time window
  return this->vddInWs / POW_WINDOW_SIZE;
}
