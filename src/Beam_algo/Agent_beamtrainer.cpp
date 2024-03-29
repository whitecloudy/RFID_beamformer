#include "Beam_algo/Agent_beamtrainer.hpp"
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <cassert>

#define __SCRAMBLE_VAR      (30)

using namespace std::complex_literals;

Agent_communicator::Agent_communicator(int num, int port) : max_num(num), counter(0)
{
  if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
    perror("socket creation failed"); 
    exit(EXIT_FAILURE); 
  }  

  memset(&servaddr, 0, sizeof(servaddr)); 

  servaddr.sin_family = AF_INET; 
  servaddr.sin_port = htons(port); 
  servaddr.sin_addr.s_addr = inet_addr("115.145.172.98"); 

  cliaddr.sin_family = AF_INET;
  cliaddr.sin_port = htons(port);
  cliaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(sockfd, (struct sockaddr *)&cliaddr, sizeof(cliaddr)) < 0){
    perror("bind error");
    exit(EXIT_FAILURE);
  }

  buf = new char[MAXIMUM_DATA];

  memset(buf, 0, sizeof(MAXIMUM_DATA));
}

Agent_communicator::~Agent_communicator()
{
  std::cout << "We are called"<<std::endl;
  delete buf;
  this->send_end();
  close(sockfd);
}

bool Agent_communicator::is_opt_ready(void)
{
  return opt_flag;
}

int Agent_communicator::reset(void)
{
  std::string send_data;

  send_data += '2';
  int result = sendto(sockfd, send_data.c_str(), send_data.size()+1, MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof(servaddr));

  counter = 0;
  opt_flag = false;
  return 0;
}

std::vector<int> recv_data_converter(std::vector<std::complex<float>> data)
{
  std::vector<int> phaseVector(6);

  for(int i = 0; i<6; i++)
  {
    phaseVector[i] = beam_util::complex2Phase(std::conj(data[i]));
    // std::cout << phaseVector[i] << " ";
  }
  // std::cout << "\n";

  return phaseVector;
}


int Agent_communicator::send_data(std::vector<int> phase_vec, float tag_i, float tag_q, float noise_i, float noise_q)
{
  int result = 0;

  if(counter < max_num)
  {
    std::string send_data;

    send_data += '0';

    for(int i = 0; i < phase_vec.size(); i++)
    {
      std::complex<double> phase = beam_util::phase2NormalComplex(phase_vec[i]);
      send_data += std::to_string(phase.real());
      send_data += ',';
      send_data += std::to_string(phase.imag());
      send_data += ',';
    }

    send_data += std::to_string(tag_i);
    send_data += ',';
    send_data += std::to_string(tag_q);
    send_data += ',';
    send_data += std::to_string(noise_i);
    send_data += ',';
    send_data += std::to_string(noise_q);
    
    int result = sendto(sockfd, send_data.c_str(), send_data.size(), MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof(servaddr));
    counter+=1;
  }
  if(counter == max_num)
  {
    std::cout << "<<<<<<<<<<<<<<<<<<<<<<< Let's Recv Channel! >>>>>>>>>>>>>>>>>>>>>>>"<<std::endl;
    nn_opt_data = recv_data_converter(recv_data());
    heur_opt_data =  recv_data_converter(recv_data());
    mmse_opt_data =  recv_data_converter(recv_data());
    dir_opt_data =  recv_data_converter(recv_data());

    std::cout << "Done"<<std::endl;
    opt_flag = true;
    counter+=1;
  }

  return result;
}


std::vector<std::complex<float>> Agent_communicator::recv_data(void)
{
  int slen = sizeof(struct sockaddr_in);
  int n = 0;

  while(true)
  {
    n = recvfrom(sockfd, buf, MAXIMUM_DATA, MSG_WAITALL, (struct sockaddr *) &servaddr, (socklen_t*)&slen);

    std::cout << "RECEIVED : "<<n<<std::endl;
    if(n != 0)
      break;
  }
  // std::cout << (int)(buf[n-1]) << std::endl;
  buf[n] = '\0';

  std::string recv_str(buf);

  std::cout << recv_str << std::endl;

  int s = 0;
  int length = 0;
  float real, imag;
  bool is_real = true;

  std::vector<std::complex<float>> result_v;

  for(int i = 0; i<n+1; i++)
  {
    if(recv_str[i] == ',' || recv_str[i] == '\0')
    {
      length = i-s;
      float data = stof(recv_str.substr(s, length));
      s = i+1;

      if(is_real)
      {
        real = data;
        is_real = false;
      }else
      {
        imag = data;
        is_real = true;
        result_v.push_back(std::complex<float>(real, imag));
      }
    }
  }
  // assert(!is_real);

  // result_v.push_back(std::complex<float>(real, imag));

  return result_v;
}

int Agent_communicator::send_end(void)
{
  std::string send_data;

  send_data += '1';
  int result = sendto(sockfd, send_data.c_str(), send_data.size()+1, MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof(servaddr));

  return 0;
}



std::vector<int> Agent_communicator::get_nn_opt(void)
{
  return nn_opt_data;
}


std::vector<int> Agent_communicator::get_heur_opt(void)
{
  return heur_opt_data;
}


std::vector<int> Agent_communicator::get_mmse_opt(void)
{
  return mmse_opt_data;
}


std::vector<int> Agent_communicator::get_dir_opt(void)
{
  return dir_opt_data;
}


Agent_beamtrainer::Agent_beamtrainer(int ant_num, std::vector<int> ant_array, int actual_round, int round_max) : Directional_beamtrainer(ant_num, ant_array), curCenterPhaseVector(ant_num), comm6(6, 11045), comm8(8, 11047), comm10(10, 11049), comm12(12, 11051)
{
  this->round_max = round_max;
  this->best_beam_max = 3;

  opt_number = 17;
  optimalPhaseVector.resize(opt_number);
}

void Agent_beamtrainer::printClassName(void){
  std::cout << "Agent_beamtrainer Selected!!"<<std::endl;
}

const std::vector<int> Agent_beamtrainer::startTraining(void){
  //reset all the values
  reset_Agent_beamtrainer();

  isTraining = true;

  trainingPhaseVector = getDirectional(cur_angle_x, cur_angle_y);
  curPhaseVector = trainingPhaseVector;

  return curPhaseVector;
}


void Agent_beamtrainer::reset_Agent_beamtrainer(void)
{
  reset_Directional_beamtrainer();

  round_count = 0;
  best_beam_count = 0;

  comm6.reset();
  comm8.reset();
  comm10.reset();
  comm12.reset();

  rankBeamL.clear();
  bestBeamNum.clear();

  curCenterPhaseVector.clear();

  beamSearchFlag = true;
}


/*
 *  Handle the tag's respond
 */
const std::vector<int> Agent_beamtrainer::getRespond(struct average_corr_data recvData, std::vector<int> usedVector){
  if(beamSearchFlag)  //We used directional beam
  { 
    for (auto beamIter = rankBeamL.begin(); true; beamIter++)
    {
      //stack directional beams by amplitude order
      if(((*beamIter).amp < recvData.avg_corr)||(beamIter == rankBeamL.end()))
      {
        struct beamStruct tmpData;

        tmpData.beamNum = getBeamNum();
        tmpData.amp = recvData.avg_corr;
        rankBeamL.insert(beamIter, tmpData);

        break;
      }
    }

    if(!optimal_used)
    {
    }

    trainingPhaseVector = getNextBeam();

    int beamNum = getBeamNum();

    //When we done searching with center Beam
    if((beamNum == 0)&&(rankBeamL.size() != 0))
    {
      round_count = round_max;
      best_beam_count = 0;

      for(int i = 0; i < rankBeamL.size(); i++)
      {
        bestBeamNum.push_back(rankBeamL.front().beamNum);
        rankBeamL.pop_front();
      }

      rankBeamL.clear();

      curCenterPhaseVector = beamNum2phaseVec(bestBeamNum[best_beam_count % bestBeamNum.size()]);
      trainingPhaseVector = randomScramble(curCenterPhaseVector, __SCRAMBLE_VAR);

      beamSearchFlag = false;
    }
  }else
  {
    if(!optimal_used)
    {
      comm6.send_data(usedVector, recvData.avg_i, recvData.avg_q, recvData.stddev_i, recvData.stddev_q);
      comm8.send_data(usedVector, recvData.avg_i, recvData.avg_q, recvData.stddev_i, recvData.stddev_q);
      comm10.send_data(usedVector, recvData.avg_i, recvData.avg_q, recvData.stddev_i, recvData.stddev_q);
      comm12.send_data(usedVector, recvData.avg_i, recvData.avg_q, recvData.stddev_i, recvData.stddev_q);
    }

    if((comm6.is_opt_ready() && comm8.is_opt_ready() && comm10.is_opt_ready() && comm12.is_opt_ready()) && !optimal_used)
    {
      optimalPhaseVector[0] = comm6.get_nn_opt();
      optimalPhaseVector[1] = comm6.get_heur_opt();
      optimalPhaseVector[2] = comm6.get_mmse_opt();
      optimalPhaseVector[3] = comm6.get_dir_opt();
      optimalPhaseVector[4] = comm8.get_nn_opt();
      optimalPhaseVector[5] = comm8.get_heur_opt();
      optimalPhaseVector[6] = comm8.get_mmse_opt();
      optimalPhaseVector[7] = comm8.get_dir_opt();
      optimalPhaseVector[8] = comm10.get_nn_opt();
      optimalPhaseVector[9] = comm10.get_heur_opt();
      optimalPhaseVector[10] = comm10.get_mmse_opt();
      optimalPhaseVector[11] = comm10.get_dir_opt();
      optimalPhaseVector[12] = comm12.get_nn_opt();
      optimalPhaseVector[13] = comm12.get_heur_opt();
      optimalPhaseVector[14] = comm12.get_mmse_opt();
      optimalPhaseVector[15] = comm12.get_dir_opt();
      optimalPhaseVector[16] = beamNum2phaseVec(bestBeamNum[0]);

      // comm6.reset();
      // comm8.reset();
      // comm10.reset();
      // comm12.reset();

      optimal_available = true;
    }

    if(round_count > 0)                  //scramble with random var
    {
      //TODO : someday should i have to get std value with parameter??
      trainingPhaseVector = randomScramble(curCenterPhaseVector, __SCRAMBLE_VAR);
      round_count--;
    }
    else    //shift center beam
    {
      round_count = round_max;
      best_beam_count++; 

      curCenterPhaseVector = beamNum2phaseVec(bestBeamNum[best_beam_count % bestBeamNum.size()]);
      trainingPhaseVector = randomScramble(curCenterPhaseVector, __SCRAMBLE_VAR);
    } 

  }

  optimal_used = false;
  curPhaseVector = trainingPhaseVector;
  return curPhaseVector;
}


/*
 * Handle when the tag does not respond
 */
const std::vector<int> Agent_beamtrainer::cannotGetRespond(std::vector<int> usedVector){
  if(beamSearchFlag)
  {
    trainingPhaseVector = getNextBeam();

    int beamNum = getBeamNum();

    if((beamNum == 0)&&(rankBeamL.size() != 0))
    {
      round_count = round_max;
      best_beam_count = 0;

      for(int i = 0; i < rankBeamL.size(); i++)
      {
        bestBeamNum.push_back(rankBeamL.front().beamNum);
        rankBeamL.pop_front();
      }

      rankBeamL.clear();

      curCenterPhaseVector = beamNum2phaseVec(bestBeamNum[best_beam_count % bestBeamNum.size()]);
      trainingPhaseVector = randomScramble(curCenterPhaseVector, __SCRAMBLE_VAR);

      beamSearchFlag = false;
    }
  }else
  {
    if(round_count > 0)                  //scramble with random var
    {
      //TODO : someday should i have to get std value with parameter??
      trainingPhaseVector = randomScramble(curCenterPhaseVector, __SCRAMBLE_VAR);
      round_count--;
    }
    else    //shift center beam
    {
      round_count = round_max;
      best_beam_count++; 

      curCenterPhaseVector = beamNum2phaseVec(bestBeamNum[best_beam_count % bestBeamNum.size()]);
      trainingPhaseVector = randomScramble(curCenterPhaseVector, __SCRAMBLE_VAR);
    } 
  }

  optimal_used = false;
  curPhaseVector = trainingPhaseVector;
  return curPhaseVector;
}



/*
 * Scramble the phase vector with random phase
 *
 */
std::vector<int> Agent_beamtrainer::randomScramble(std::vector<int> center, double std){
  static std::default_random_engine random_gen;

  std::normal_distribution<double> normal_random(0, std);

  for(int idx = 0; idx < center.size(); idx++){
    center[idx] += normal_random(random_gen);
  }

  return center;
}
