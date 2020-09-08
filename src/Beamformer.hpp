#ifndef __BEAMFORMER__
#define __BEAMFORMER__

#include "AD5371_ctrl/Controller_Global.h"
#include "AD5371_ctrl/Phase_Attenuator_controller.hpp"
#include "Beam_algo/Beam_algorithm.hpp"
#include "Beam_algo/Beamtrainer.hpp"
#include "Beam_algo/Random_beamtrainer.hpp"
#include "SIC_ctrl/SIC_controller.hpp"
#include "Reader_comm/IPC_controller.hpp"
#include "struct_global.hpp"
#include <iostream>
#include <string>
#include <cstdio>
#include <thread>
#include <fstream>
#include <memory>

class Beamformer{
  private:
    std::unique_ptr<Phase_Attenuator_controller> phase_ctrl;
    std::unique_ptr<SIC_controller> sic_ctrl;
    std::unique_ptr<Beamtrainer> BWtrainer;
    
    IPC_controller ipc;
    std::ofstream log;

    bool sic_enabled = false;
    int ant_amount;   //TODO : this is actually not nessesary.... need to be depricated
    std::vector<int> ant_nums;
    int cur_weights[ANT_num] = {};  //TODO : change this more neet way
    

  private:
    int weights_addition(int * dest_weights, int * weights0, int * weights1);
    int weights_addition(int * dest_weights, int * weights);

    void weights_printing(int * weights);
    int vector2cur_weights(std::vector<int> weightVector);

    int weights_apply(int * weights);
    int weights_apply(void);

    int init_beamformer(void);
    int run_beamformer(void);
    int calculate_beamforming_weights(void);

    int SIC_port_measure(void);
    int SIC_port_measure_over(void);
    int SIC_handler(struct average_corr_data &);
    int Signal_handler(struct average_corr_data &);
    int dataLogging(struct average_corr_data &);

  public:
    Beamformer(std::vector<int> ant_nums, BEAM_ALGO::algorithm beam_algo, int sic_ant_num);
    ~Beamformer();
    int start_beamformer(void);

};

#endif

