#ifndef _DIRECTIONAL_WITH_REFINING_BEAMTRAINER2_
#define _DIRECTIONAL_WITH_REFINING_BEAMTRAINER2_

#include <iostream>
#include "Beam_algo/Beamtrainer.hpp"
#include "Beam_algo/Directional_beamtrainer.hpp"
#include "common/CA_calculator.hpp"

#include <armadillo>
#include <random>
#include <memory>
#include <list>


class Directional_with_refining_beamtrainer2 : public Directional_beamtrainer{
  private:
    int training_frame;

    int x_step;
    int y_step;

    double cur_refine_x;
    double cur_refine_y;

    double cur_center_x;
    double cur_center_y;

    int round_count;

    CA_calculator ca_cal;


    bool beamSearchFlag = true;

    struct beamStruct
    {
      int x;
      int y;
      double amp = 0.0;
    };
    struct beamStruct bestBeam;
    bool bestBeamFlag = false;

    std::vector<int> curCenterPhaseVector;

    void reset_Directional_with_refining_beamtrainer(void);

  private:
    const std::vector<int> reset_refining_beam_angles();
    const std::vector<int> getNextRefiningBeam();

  public:
    Directional_with_refining_beamtrainer2(int ant_num, std::vector<int> ant_array, int training_frame);

    const std::vector<int> startTraining(void);
    const std::vector<int> getRespond(struct average_corr_data recvData, std::vector<int> usedVector = curPhaseVector);
    const std::vector<int> cannotGetRespond(std::vector<int> usedVector = curPhaseVector);

    void printClassName(void);
};

#endif
