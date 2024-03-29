#include "Beam_algo/Beam_algorithm.hpp"
#include <iostream>

#include "Beam_algo/Random_beamtrainer.hpp"
#include "Beam_algo/Directional_beamtrainer.hpp"
#include "Beam_algo/Directional_with_refining_beamtrainer.hpp"
#include "Beam_algo/Directional_with_refining_beamtrainer2.hpp"
#include "Beam_algo/Fixed_beamtrainer.hpp"
#include "Beam_algo/CA_with_directional_beamtrainer.hpp"
#include "Beam_algo/Agent_beamtrainer.hpp"
#include "Beam_algo/CA_with_random_beamtrainer.hpp"
#include "Beam_algo/Kalmaned_with_random_beamtrainer.hpp"
#include "Beam_algo/Test_beamtrainer.hpp"

namespace BEAM_ALGO
{

  Beamtrainer * get_beam_class(int ant_num, int k, algorithm algo, std::vector<int> ant_array)
  {
    Beamtrainer * class_ptr;

    switch(algo)
    {
      case RANDOM_BEAM:
        class_ptr = new Random_beamtrainer(ant_num);
        break;
      case DIRECTIONAL_BEAM:
        class_ptr = new Directional_beamtrainer(ant_num, ant_array);
        break;
      case DIRECTIONAL_REFINE_BEAM:
        class_ptr = new Directional_with_refining_beamtrainer(ant_num, ant_array, k);
        break;
      case DIRECTIONAL_REFINE_BEAM2:
        class_ptr = new Directional_with_refining_beamtrainer2(ant_num, ant_array, k);
        break;
      case CA_WITH_DIRECTIONAL:
        class_ptr = new CA_with_directional_beamtrainer(ant_num, ant_array);
        break;
      case CA_WITH_RANDOM:
        class_ptr = new CA_with_random_beamtrainer(ant_num);
        break;
      case CA_KALMANED_WITH_RANDOM:
        class_ptr = new Kalmaned_with_random_beamtrainer(ant_num);
        break;
      case FIXED_BEAM:
        class_ptr = new Fixed_beamtrainer(ant_num);
        break;
      case AGENT:
        /*
        {
          int maximum = 0;
          std::cout << "How many beam? : ";
          std::cin >> maximum;
          class_ptr = new Agent_beamtrainer(ant_num, ant_array, maximum);
          break;
        }*/
        class_ptr = new Agent_beamtrainer(ant_num, ant_array, 12);
        break;
      case TEST:
        class_ptr = new Test_beamtrainer(ant_num);
        break;
      default:
        std::cerr << "Warning: No Algorithm selected" << std::endl;
        class_ptr = NULL;
        break;
    }

    return class_ptr;
  }


  algorithm parse_beam_algorithm(std::string input)
  {
    algorithm algo;
    if(!input.compare("random"))                        algo = RANDOM_BEAM;
    else if(!input.compare("directional"))              algo = DIRECTIONAL_BEAM;
    else if(!input.compare("directional_refine"))       algo = DIRECTIONAL_REFINE_BEAM;
    else if(!input.compare("directional_refine2"))       algo = DIRECTIONAL_REFINE_BEAM2;
    else if(!input.compare("fixed"))                    algo = FIXED_BEAM;
    else if(!input.compare("ca_with_directional"))      algo = CA_WITH_DIRECTIONAL;
    else if(!input.compare("ca_with_random"))           algo = CA_WITH_RANDOM;
    else if(!input.compare("kalmaned_with_random"))     algo = CA_KALMANED_WITH_RANDOM;
    else if(!input.compare("agent"))                    algo = AGENT;
    else if(!input.compare("test"))                     algo = TEST;
    else{
      std::cerr<<"Error : No such algorithm"<<std::endl;
      exit(1);
    }

    return algo;
  }

}


