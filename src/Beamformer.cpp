#include "Beamformer.hpp"
#include <cstring>
#include <random>
#include <fstream>
#include <sys/time.h>

#define __COLLECT_DATA__
//#define __TIME_STAMP__

#define PREDEFINED_RN16_ (0x5555)
#define EXPECTED_TAG_NUM_ (1)

#define SIC_PORT_NUM_ ant_nums[ant_amount-1]


double normal_random(double mean, double std_dev){
  static std::random_device r;
  static std::default_random_engine generator(r());
  std::normal_distribution<double> distribution(mean, std_dev);

  return distribution(generator);
}


int Beamformer::stage_start(struct average_corr_data * data)
{
  char buffer[IO_BUF_SIZE] = {};

  if(ipc.data_recv(buffer) == -1){
    std::cerr <<"Breaker is activated"<<std::endl;
    return 1;
  }

  if(data != NULL)
    memcpy(data, buffer, sizeof(*data));

  return 0;
}


int Beamformer::stage_finish(void)
{
  phase_ctrl->data_apply();

  if(ipc.send_ack(needSIC) == -1)
  {
    return 1;
  }
  return 0;
}



int Beamformer::run_beamformer(void){

  struct average_corr_data data;

  /******************** SIC measure stage *************/
  if(stage_start(NULL))
    return 0;

  SIC_port_measure();

  if(stage_finish())
    return 0;

  if(stage_start(&data))
    return 0;

  sic_ctrl = std::unique_ptr<SIC_controller>(new SIC_controller(std::complex<float>(data.cw_i, data.cw_q)));
  SIC_port_measure_over();

  //initial phase here
  if(stage_finish())
    return 0;


  /*****************************************************/


  //loop until it is over
  while(1){
    /******************* SIC stage *******************/
    do
    {
      if(stage_start(&data))
        return 0;

      //dataLogging(data);

      if(data.successFlag != _GATE_FAIL)
      {
        SIC_handler(data);    
      }
      else
      {
        SIC_adjustment();
      }

      //send ack so that Gen2 program can recognize that the beamforming has been done
      if(stage_finish())
        return 0;

    }while(data.successFlag == _GATE_FAIL);
    /*************************************************/



    /******************* Signal stage *****************/
    do
    {
      if(stage_start(&data))
        return 0;

      if(data.successFlag != _GATE_FAIL)
      {
        //At the end of turn
        Signal_handler(data);
      }
      else
      {
        SIC_adjustment();
      }

      if(stage_finish())
        return 0;

    }while((data.successFlag == _GATE_FAIL)||!needSIC);
    /******************************************************/
  }//end of while(1)

  //print wait
  weights_printing(cur_weights);

  return 0;
}



Beamformer::Beamformer(std::vector<int> ant_nums_p, BEAM_ALGO::algorithm beam_algo, int sic_ant_num, std::vector<int> ant_array, int k)
{
  this->phase_ctrl = std::unique_ptr<Phase_Attenuator_controller>(new Phase_Attenuator_controller(0));
  this->ant_nums = ant_nums_p;
  this->ant_amount = ant_nums.size();
  this->training_round_max = 27 + k;
  this->BWtrainer = std::unique_ptr<Beamtrainer>(BEAM_ALGO::get_beam_class(ant_amount, training_round_max, beam_algo, ant_array));

  this->BWtrainer->printClassName();


  if(sic_ant_num != -1){
    sic_enabled = true;
    this->ant_nums.push_back(sic_ant_num); //TODO : SIC antenna shall be handled seperately
  }

  log.open("log.csv");
  optimal_log.open("log_optimal.csv");

  for(int i = 0; i<ant_amount; i++){
    log<<"phase "<<ant_nums[i]<<", ";
    optimal_log<<"phase "<<ant_nums[i]<<", ";
  }

  log<<"SIC phase, SIC power, avg corr,corr i, corr q, cw i, cw q,std i,std q ,RN16, round"<<std::endl;
  optimal_log<<"SIC phase, SIC power, avg corr,corr i, corr q, cw i, cw q,std i,std q ,RN16, round"<<std::endl;

  this->ant_amount++;
}



Beamformer::~Beamformer(){
  log.close();
}

int Beamformer::init_beamformer(void){
  std::vector<int> weightVector = BWtrainer->startTraining();
  vector2cur_weights(weightVector);
  for(int i = 0; i<ant_amount-1; i++){
    phase_ctrl->ant_on(ant_nums[i]);
  }

  status = TRAINING;
  status_count = training_round_max;

  std::cout<<"Init"<<std::endl;
  return weights_apply(cur_weights);
}


int Beamformer::weights_addition(int * dest_weights, int * weights0, int * weights1){
  for(int i = 0; i<ant_amount-1; i++){
    dest_weights[ant_nums[i]] = weights0[ant_nums[i]] + weights1[ant_nums[i]];
    while(dest_weights[ant_nums[i]] < 0)
      dest_weights[ant_nums[i]]+= 360;
    dest_weights[ant_nums[i]] %= 360;
  }

  return 0;
}


int Beamformer::weights_addition(int * dest_weights, int * weights){
  for(int i = 0; i<ant_amount-1; i++){
    dest_weights[ant_nums[i]] += weights[ant_nums[i]];
    while(dest_weights[ant_nums[i]] < 0)
      dest_weights[ant_nums[i]]+= 360;
    dest_weights[ant_nums[i]] %= 360;
  }
  return 0;
}


int Beamformer::weights_apply(int * weights){
  for(int i = 0; i<ant_amount-1; i++){
    phase_ctrl->phase_control(ant_nums[i], weights[ant_nums[i]]);
  }
  return 0;
}


int Beamformer::weights_apply(void){
  int * weights = cur_weights;
  for(int i = 0; i<ant_amount; i++){
    phase_ctrl->phase_control(ant_nums[i], weights[ant_nums[i]]);
  }
  return 0;
}


int Beamformer::vector2cur_weights(std::vector<int> weightVector){
  for(int i = 0; i<ant_amount-1;i++){
    cur_weights[ant_nums[i]] = weightVector[i];
  }
  return 0;
}



void Beamformer::weights_printing(int * weights){
  for(int i = 0; i<ant_amount; i++){
    std::cout<<"ANT num : "<<ant_nums[i]<<std::endl;
    std::cout<<"Phase : "<<weights[ant_nums[i]]<<std::endl<<std::endl;
  }
}


int Beamformer::start_beamformer(void){
  if(init_beamformer())
    std::cerr << "Beamformer init failed"<<std::endl;
  std::cout << "Beamformer init finished"<<std::endl;
  if(ipc.wait_sync())
    return -1;

  return run_beamformer();
}


int Beamformer::SIC_port_measure(void){
  //We must measure SIC port before we start.
  for(int i = 0; i<16; i++){
    phase_ctrl->ant_off(i);
  }
  cur_weights[SIC_PORT_NUM_] = 0;
  phase_ctrl->ant_on(SIC_PORT_NUM_);
  phase_ctrl->phase_control(SIC_PORT_NUM_, SIC_REF_POWER, 0);

  std::cout << "SIC Phase Set"<<std::endl;
  return 0;
}



int Beamformer::SIC_port_measure_over(void){
  //We must measure SIC port before we start.
  for(int i = 0; i<ant_amount-1; i++){
    phase_ctrl->ant_on(ant_nums[i]);
  }

  weights_apply(cur_weights);

  std::cout << "SIC over"<<std::endl;
  return 0;
}

int Beamformer::SIC_handler(const struct average_corr_data & data){
  sic_ctrl->setCurrentAmp(std::complex<float>(data.cw_i, data.cw_q));
  cur_weights[SIC_PORT_NUM_] = sic_ctrl->getPhase();   //get SIC phase
  phase_ctrl->phase_control(SIC_PORT_NUM_, sic_ctrl->getPower(), cur_weights[SIC_PORT_NUM_]); //change phase and power

  needSIC = false;
  return 0;
}

int Beamformer::SIC_adjustment(void)
{
  needSIC = false;
  sic_ctrl->setPower(sic_ctrl->getPower() + 0.1);
  phase_ctrl->phase_control(SIC_PORT_NUM_, sic_ctrl->getPower(), sic_ctrl->getPhase());

  return 0;
}



int Beamformer::Signal_handler(const struct average_corr_data & data){
  uint16_t tag_id = 0;
  std::vector<int> weightVector;

  for(int i = 0; i<16; i++){
    tag_id = tag_id << 1;
    tag_id += data.RN16[i];
  }

  /*************************Add algorithm here***************************/

  dataLogging(data, sic_ctrl->getPower(),  BWtrainer->isOptimalUsed(), BWtrainer->which_optimal());

  counter++;

  if(status == TRAINING)
  {
    if(data.successFlag == _SUCCESS)
    {
      for(int i = 0; i<16; i++)
      {
        tag_id = tag_id << 1;
        tag_id += data.RN16[i];
      }

      printf("Got RN16 : %x\n",tag_id);
      printf("avg corr : %f\n",data.avg_corr);
      printf("avg iq : %f, %f\n",data.avg_i, data.avg_q);
      printf("avg amp : %f, %f\n\n",data.cw_i, data.cw_q);

      if(tag_id == PREDEFINED_RN16_)
      {
        BWtrainer->getRespond(data);
      }
      else
      {
        BWtrainer->cannotGetRespond();
      }
    }else{
      printf("Couldn't get RN16\n\n");
      BWtrainer->cannotGetRespond();
    }
    needSIC = true;
  }

  if(status_count > 1)
  {
    status_count -= 1;

    if(status == TRAINING)
    {
      weightVector = BWtrainer->getTrainingPhaseVector();
      needSIC = true;
    }else if(status == BEAMFORMING)
    {
      weightVector = BWtrainer->getOptimalPhaseVector();
      needSIC = false;
    }
  }else
  {
    needSIC = true;
    if(status == BEAMFORMING)
    {
      status = TRAINING;
      status_count = training_round_max;
      BWtrainer->startTraining();
      weightVector = BWtrainer->getTrainingPhaseVector();
    }else if(status == TRAINING)
    {
      beamforming_count += 1;

      if(BWtrainer->isOptimalCalculated())
      {
        status = BEAMFORMING;
        status_count = BEAMFORMING_ROUND;
        weightVector = BWtrainer->getOptimalPhaseVector();
      }else
      {
        status = TRAINING;
        status_count = training_round_max;
        BWtrainer->startTraining();
        weightVector = BWtrainer->getTrainingPhaseVector();
      }
    }
  }

  vector2cur_weights(weightVector);

  if(weights_apply(cur_weights)){
    std::cerr<<"weight apply failed"<<std::endl;
    return 1;
  }

  if(needSIC)
  {
    sic_ctrl->setPower(-22);
    sic_ctrl->setPhase(SIC_REF_PHASE);
    phase_ctrl->phase_control(SIC_PORT_NUM_, -22, SIC_REF_PHASE);
  }
  /*****************************************************************/

  return 0;
}



int Beamformer::dataLogging(const struct average_corr_data & data, double sic_power, bool optimal, const int which_op){
  uint16_t tag_id = 0;

  for(int i = 0; i<16; i++){
    tag_id = tag_id << 1;
    tag_id += data.RN16[i];
  }

  if(data.successFlag == _SUCCESS){
    if(optimal)
    {
      for(int i = 0; i<ant_amount;i++){
        optimal_log<<cur_weights[ant_nums[i]]<<", ";
      }
      optimal_log<<sic_power<< ", "<<data.avg_corr<<", "<<data.avg_i<<", "<<data.avg_q<<", "<<data.cw_i<<", "<<data.cw_q<<", "<<data.stddev_i<<", "<<data.stddev_q<<", "<<tag_id<<", "<<data.round<< ", "<<which_op<<", " <<counter << ", " <<beamforming_count<<std::endl;
    }else
    {
      for(int i = 0; i<ant_amount;i++){
        log<<cur_weights[ant_nums[i]]<<", ";
      }

      log<<sic_power<< ", "<<data.avg_corr<<", "<<data.avg_i<<", "<<data.avg_q<<", "<<data.cw_i<<", "<<data.cw_q<<", "<<data.stddev_i<<", "<<data.stddev_q<<", "<<tag_id<<", "<<data.round<<", "<<counter<< ", " << beamforming_count<<std::endl;
    }
  }else if(data.successFlag == _PREAMBLE_FAIL){
    if(optimal)
    {
      for(int i = 0; i<ant_amount;i++){
        optimal_log<<cur_weights[ant_nums[i]]<<", ";
      }
      optimal_log<<sic_power<< ", "<<0.0<<", "<<0.0<<", "<<0.0<<","<<data.cw_i<<", "<<data.cw_q<<", "<<data.stddev_i<<", "<<data.stddev_q<<", "<<"-"<<","<<data.round<<", "<<which_op<<", "<<counter<< ", " << beamforming_count<<std::endl;
    }else
    {
      for(int i = 0; i<ant_amount;i++){
        log<<cur_weights[ant_nums[i]]<<", ";
      }
      log<<sic_power<< ", "<<0.0<<", "<<0.0<<", "<<0.0<<","<<data.cw_i<<", "<<data.cw_q<<", "<<data.stddev_i<<", "<<data.stddev_q<<", "<<"-"<<","<<data.round<<", "<<counter<< ", " << beamforming_count<<std::endl;
    }
  }else if(data.successFlag == _GATE_FAIL){
    if(optimal)
    {
      for(int i = 0; i<ant_amount;i++){
        optimal_log<<cur_weights[ant_nums[i]]<<", ";
      }
      optimal_log<<sic_power<< ", "<<0.0<<", "<<0.0<<", "<<0.0<<","<<data.cw_i<<", "<<data.cw_q<<", "<<data.stddev_i<<", "<<data.stddev_q<<", "<<"GATE Failed"<<","<<data.round<<", "<<counter<<std::endl;
    }else
    {
      for(int i = 0; i<ant_amount;i++){
        log<<cur_weights[ant_nums[i]]<<", ";
      }
      log<<sic_power<< ", "<<0.0<<", "<<0.0<<", "<<0.0<<","<<data.cw_i<<", "<<data.cw_q<<", "<<data.stddev_i<<", "<<data.stddev_q<<", "<<"GATE Failed"<<","<<data.round<<std::endl;
    }

  }



  return 0;
}
