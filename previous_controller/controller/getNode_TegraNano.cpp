#include "getNode_TegraNano.h"
#include <sys/ioctl.h>
#include <unistd.h>
#include <cstdio> // sprintf
#include <cstdlib> // atoi
#include <fcntl.h>
#include <cstring>  // strncmp
#include <string>
#include <iostream> // cerr
#include <fstream>  // cerr

GetNode_TegraNano::GetNode_TegraNano() {
  //paths to access CPU frequency/status infos
  for (int i = 0; i < TEGRA_NUM_OF_CPU_CORES; i++) {
    std::string temp;
    temp = CPU_PATH + std::to_string(i) + CPU_CURR_STATUS;
    cpu_online_list[i] = temp;
    temp = CPU_PATH + std::to_string(i) + CPU_CURR_FREQ;
    cpu_freq_list[i] = temp;
    temp = CPU_PATH + std::to_string(i) + CPU_CURR_SCAL_FREQ;
    cpu_scal_freq_list[i] = temp;
    temp = CPU_PATH + std::to_string(i) + CPU_AVAILABLE_FREQS;
    cpu_avail_freq_list[i] = temp;
    temp = CPU_PATH + std::to_string(i) + CPU_FREQ_SCALING_GOV;
    cpu_freq_gov_list[i] = temp;
    temp = CPU_PATH + std::to_string(i) + CPU_SET_FREQ_MAX;
    cpu_set_freq_max_list[i] = temp;
    temp = CPU_PATH + std::to_string(i) + CPU_SET_FREQ_MIN;
    cpu_set_freq_min_list[i] = temp;
  }
  
  //read sensors for the first time  
  this->update_read();

  //initialize attributes for power consumption
  this->gpuWavg[0] = this->gpuWs = this->getGpuW();
  this->vddInWavg[0] = this->vddInWs = this->getVddInW();
  this->cpuWavg[0] = this->cpuWs = this->getCpuW();
  
  for(int i = 1; i < POW_WINDOW_SIZE; i++) {
    this->gpuWavg[i] = 0;
    this->vddInWavg[i] = 0;
    this->cpuWavg[i] = 0;
  }
  this->slidingIndex = 1;
}

GetNode_TegraNano::~GetNode_TegraNano() {
}

bool GetNode_TegraNano::isAutoCPUFreqScaling(){
  FILE* fp;
  char value;
  fp = fopen(AUTO_CPU_FREQ_SCALING, "r");
  if(fp) {
    fscanf(fp, "%c", &value);
    fclose(fp);
  } else
    value = 'N'; //error opening the file
  return (value=='Y'?true:false);
}

bool GetNode_TegraNano::isCPUActive(int cpuNum) {
  if(cpuNum==0) //the first core is always on
    return true;
  else if(cpuNum<0 || cpuNum > TEGRA_NUM_OF_CPU_CORES)
    return false; //do note. it does not exist

  FILE* fp;
  int value;
  fp = fopen(cpu_online_list[cpuNum].c_str(), "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    fclose(fp);
  } else
    value = -1; //error opening the file
  return (value==1)?true:false;
}

int GetNode_TegraNano::getCPUCurFreq(int cpuNum) { //DO NOTE: values are returned in KHz
  if(cpuNum<0 || cpuNum > TEGRA_NUM_OF_CPU_CORES)
    return -1; //do note. it does not exist

  FILE* fp;
  int value;
  fp = fopen(cpu_freq_list[cpuNum].c_str(), "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    fclose(fp);
  } else
    value = -1; //error opening the file
  return value; //DO NOTE: values are returned in KHz
}

int GetNode_TegraNano::getCPUCurScalFreq(int cpuNum) { //DO NOTE: values are returned in KHz
  if(cpuNum<0 || cpuNum > TEGRA_NUM_OF_CPU_CORES)
    return -1; //do note. it does not exist

  FILE* fp;
  int value;
  fp = fopen(cpu_scal_freq_list[cpuNum].c_str(), "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    fclose(fp);
  } else
    value = -1; //error opening the file
  return value; //DO NOTE: values are returned in KHz
}


int GetNode_TegraNano::getGPUCurFreq() { //DO NOTE: values are returned in KHz
  FILE* fp;
  int value;
  fp = fopen(GPU_CURR_FREQ, "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    value /= 1000; //DO NOTE: values are returned in KHz
    fclose(fp);
  } else
    value = -1; //error opening the file
  return value;
}

int GetNode_TegraNano::getMemCurFreq() { //DO NOTE: values are returned in KHz
  FILE* fp;
  int value;
  fp = fopen(MEM_CURR_FREQ, "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    value /= 1000; //DO NOTE: values are returned in KHz
    fclose(fp);
  } else
    value = -1; //error opening the file
  return value;
}

std::vector<int> GetNode_TegraNano::getAvailableCPUFreq(int cpuNum) {
  std::vector<int> freqs;
  if(cpuNum>=0 && cpuNum < TEGRA_NUM_OF_CPU_CORES){
    FILE* fp;
    int value;
    fp = fopen(cpu_avail_freq_list[cpuNum].c_str(), "r");
    if(fp) {
      fscanf(fp, "%d", &value);
      while(!feof(fp)) {
        freqs.push_back(value);       
        fscanf(fp, "%d", &value);
      }
      fclose(fp);
    }
  }
  return freqs; //DO NOTE: values are returned in KHz  
}

std::vector<int> GetNode_TegraNano::getAvailableGPUFreq() {
  std::vector<int> freqs;
  FILE* fp;
  int value;
  fp = fopen(GPU_AVAILABLE_FREQS, "r");
  if(fp) {
    fscanf(fp, "%d", &value);
    while(!feof(fp)) {
      freqs.push_back(value/1000);       
      fscanf(fp, "%d", &value);
    }
    fclose(fp);
  }
  return freqs; //DO NOTE: values are returned in KHz  
}

std::vector<int> GetNode_TegraNano::getAvailableMemFreq() {
  std::vector<int> freqs;
  //TODO 
  std::cout << "NOT IMPLEMENTED!" << std::endl;
  exit(0);
  return freqs; //DO NOTE: values are returned in KHz  
}

void GetNode_TegraNano::setAutoCPUFreqScaling(bool status) {
  //it is necessary to modify two files to disable/enable automatic dvfs governor.

  //we need to call the nvpmodel to reset the overall configuration of the various governors (e.g. max/min freqs for governor, etc etc.)
  system(NVPMODEL_M0);

  FILE* fp;
  fp = fopen(AUTO_CPU_FREQ_SCALING, "w");
  if(fp) {
    fprintf(fp, "%c", (status==true)?'Y':'N');
    fclose(fp);
  }
  //then change the governor
  fp = fopen(cpu_freq_gov_list[0].c_str(), "w");
  if(fp) {
    fprintf(fp, "%s", (status==true)?SCHEDUTIL_GOVERNOR:PERFORMANCE_GOVERNOR);
    fclose(fp);
  }
  if(!status)
    for (int i = 0; i < TEGRA_NUM_OF_CPU_CORES; i++)
      this->setCPUActive(i, true);
}

void GetNode_TegraNano::setCPUActive(int cpuNum, bool status) {
  //the first core is always on. IDs not in [1;TEGRA_NUM_OF_CPU_CORES] are not valid
  if(cpuNum==0 || cpuNum<0 || cpuNum > TEGRA_NUM_OF_CPU_CORES)
    return;

  FILE* fp;
  fp = fopen(cpu_online_list[cpuNum].c_str(), "w");
  if(fp) {
    fprintf(fp, "%d", (status==true)?1:0);
    fclose(fp);
  }
}

void GetNode_TegraNano::setCPUFreq(int cpuNum, int freq) {
  //the first core is always on. IDs not in [1;TEGRA_NUM_OF_CPU_CORES] are not valid
  if(cpuNum==0 || cpuNum<0 || cpuNum > TEGRA_NUM_OF_CPU_CORES)
    return;

  FILE* fp;

  //DO NOTE: actually on the ARM cluster the userspace governor does not work. so the trick is to 
  //use the performance governor and set the same frequency for the max and min bound.
  //the bothering part is that it is relevant the order of writing of the min and max 
  //value since min cannot be > of max
  //TODO REFACTOR THE CODE

  //read min scaling freq
  int minvalue;
  fp = fopen(cpu_set_freq_min_list[cpuNum].c_str(), "r");
  if(fp) {
    fscanf(fp, "%d", &minvalue);
    fclose(fp);
  } else
    minvalue = -1; //error opening the file

  //read max scaling freq
  int maxvalue;
  fp = fopen(cpu_set_freq_max_list[cpuNum].c_str(), "r");
  if(fp) {
    fscanf(fp, "%d", &maxvalue);
    fclose(fp);
  } else
    maxvalue = -1; //error opening the file

  if(freq < minvalue) {
    fp = fopen(cpu_set_freq_min_list[cpuNum].c_str(), "w");
    if(fp) {
      fprintf(fp, "%d", freq);
      fclose(fp);
    }
    fp = fopen(cpu_set_freq_max_list[cpuNum].c_str(), "w");
    if(fp) {
      fprintf(fp, "%d", freq);
      fclose(fp);
    }
  } else {
    fp = fopen(cpu_set_freq_max_list[cpuNum].c_str(), "w");
    if(fp) {
      fprintf(fp, "%d", freq);
      fclose(fp);
    }
    fp = fopen(cpu_set_freq_min_list[cpuNum].c_str(), "w");
    if(fp) {
      fprintf(fp, "%d", freq);
      fclose(fp);
    }    
  }

}

void GetNode_TegraNano::setGPUFreq(int freq) {
  //TODO
  std::cout << "NOT IMPLEMENTED!" << std::endl;
  exit(0);
}

void GetNode_TegraNano::setMemFreq(int freq) {
  //TODO
  std::cout << "NOT IMPLEMENTED!" << std::endl;
  exit(0);
}

float GetNode_TegraNano::read_sensor(const char *node) {
  float value;
  FILE* fp;
  fp = fopen(node, "r");
  if(fp) {
    fscanf(fp, "%f", &value);
    fclose(fp);
  } else
    value = -1;
  return value;
}

float GetNode_TegraNano::getGpuV() {
  return this->gpuV;
}

float GetNode_TegraNano::getGpuA() {
  return this->gpuA;
}

float GetNode_TegraNano::getGpuW() {
  return this->gpuW;
}

float GetNode_TegraNano::getVddInV() {
  return this->vddInV;
}

float GetNode_TegraNano::getVddInA() {
  return this->vddInA;
}

float GetNode_TegraNano::getVddInW() {
  return this->vddInW;
}

float GetNode_TegraNano::getCpuV() {
  return this->cpuV;
}

float GetNode_TegraNano::getCpuA() {
  return this->cpuA;
}

float GetNode_TegraNano::getCpuW() {
  return this->cpuW;
}


void GetNode_TegraNano::update_read(){
  //collect instantaneous values
  this->gpuV = this->read_sensor(SENSOR_GPU_V);
  this->gpuA = this->read_sensor(SENSOR_GPU_A);
  this->gpuW = this->read_sensor(SENSOR_GPU_W);

  this->vddInV = this->read_sensor(SENSOR_IN_V);
  this->vddInA = this->read_sensor(SENSOR_IN_A);
  this->vddInW = this->read_sensor(SENSOR_IN_W);

  this->cpuV = this->read_sensor(SENSOR_CPU_V);
  this->cpuA = this->read_sensor(SENSOR_CPU_A);
  this->cpuW = this->read_sensor(SENSOR_CPU_W);
}

void GetNode_TegraNano::updateSensing() {
  this->update_read();

  //update average values
  this->gpuWs = this->gpuWs - this->gpuWavg[this->slidingIndex] + this->gpuW;
  this->vddInWs = this->vddInWs - this->vddInWavg[this->slidingIndex] + this->vddInW;
  this->cpuWs = this->cpuWs - this->cpuWavg[this->slidingIndex] + this->cpuW;

  this->gpuWavg[this->slidingIndex] = this->gpuW;
  this->vddInWavg[this->slidingIndex] = this->vddInW;
  this->cpuWavg[this->slidingIndex] = this->cpuW;

  this->slidingIndex = (this->slidingIndex+1)%POW_WINDOW_SIZE;
}

float GetNode_TegraNano::getGpuWavg() {
  return this->gpuWs / POW_WINDOW_SIZE;
}

float GetNode_TegraNano::getVddInWavg() {
  return this->vddInWs / POW_WINDOW_SIZE;
}

float GetNode_TegraNano::getCpuWavg() {
  return this->cpuWs / POW_WINDOW_SIZE;
}

float GetNode_TegraNano::getChipW() {
  return this->vddInW;
}

float GetNode_TegraNano::getChipWavg() {
  return this->getVddInWavg();
}

