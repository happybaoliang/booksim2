// $Id$

/*
 Copyright (c) 2007-2012, Trustees of The Leland Stanford Junior University
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this 
 list of conditions and the following disclaimer.
 Redistributions in binary form must reproduce the above copyright notice, this
 list of conditions and the following disclaimer in the documentation and/or
 other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <iostream>
#include <vector>
#include <cassert>
#include <limits>
#include "random_utils.hpp"
#include "injection.hpp"

using namespace std;

InjectionProcess::InjectionProcess(int nodes, double rate)
  : _nodes(nodes), _rate(rate)
{
  if(nodes <= 0) {
    cout << "Error: Number of nodes must be greater than zero." << endl;
    exit(-1);
  }
  if((rate < 0.0) || (rate > 1.0)) {
    cout << "Error: Injection process must have load between 0.0 and 1.0."
	 << endl;
    exit(-1);
  }
}

void InjectionProcess::reset()
{

}

InjectionProcess * InjectionProcess::New(string const & inject, int nodes, 
					 double load, 
					 Configuration const * const config)
{
  string process_name;
  string param_str;
  size_t left = inject.find_first_of('(');
  if(left == string::npos) {
    process_name = inject;
  } else {
    process_name = inject.substr(0, left);
    size_t right = inject.find_last_of(')');
    if(right == string::npos) {
      param_str = inject.substr(left+1);
    } else {
      param_str = inject.substr(left+1, right-left-1);
    }
  }
  vector<string> params = tokenize_str(param_str);

  InjectionProcess * result = NULL;
  if(process_name == "bernoulli") {
    result = new BernoulliInjectionProcess(nodes, load);
  } else if(process_name == "on_off") {
    bool missing_params = false;
    double alpha = numeric_limits<double>::quiet_NaN();
    if(params.size() < 1) {
      if(config) {
	alpha = config->GetFloat("burst_alpha");
      } else {
	missing_params = true;
      }
    } else {
      alpha = atof(params[0].c_str());
    }
    double beta = numeric_limits<double>::quiet_NaN();
    if(params.size() < 2) {
      if(config) {
	beta = config->GetFloat("burst_beta");
      } else {
	missing_params = true;
      }
    } else {
      beta = atof(params[1].c_str());
    }
    double r1 = numeric_limits<double>::quiet_NaN();
    if(params.size() < 3) {
      r1 = config ? config->GetFloat("burst_r1") : -1.0;
    } else {
      r1 = atof(params[2].c_str());
    }
    if(missing_params) {
      cout << "Missing parameters for injection process: " << inject << endl;
      exit(-1);
    }
    if((alpha < 0.0 && beta < 0.0) || 
       (alpha < 0.0 && r1 < 0.0) || 
       (beta < 0.0 && r1 < 0.0) || 
       (alpha >= 0.0 && beta >= 0.0 && r1 >= 0.0)) {
      cout << "Invalid parameters for injection process: " << inject << endl;
      exit(-1);
    }
    vector<int> initial(nodes);
    if(params.size() > 3) {
      initial = tokenize_int(params[2]);
      initial.resize(nodes, initial.back());
    } else {
      for(int n = 0; n < nodes; ++n) {
	initial[n] = RandomInt(1);
      }
    }
    result = new OnOffInjectionProcess(nodes, load, alpha, beta, r1, initial);
  } else if(process_name == "customizedinjectionprocess") {
    result=new CustomizedInjectionProcess(nodes,load);
  } else {
    cout << "Invalid injection process: " << inject << endl;
    exit(-1);
  }
  return result;
}

//=============================================================

BernoulliInjectionProcess::BernoulliInjectionProcess(int nodes, double rate)
  : InjectionProcess(nodes, rate)
{

}

bool BernoulliInjectionProcess::test(int source,int cl)
{
  assert((source >= 0) && (source < _nodes));
  return (RandomFloat() < _rate);
}

//=============================================================

OnOffInjectionProcess::OnOffInjectionProcess(int nodes, double rate, 
					     double alpha, double beta, 
					     double r1, vector<int> initial)
  : InjectionProcess(nodes, rate), 
    _alpha(alpha), _beta(beta), _r1(r1), _initial(initial)
{
  assert(alpha <= 1.0);
  assert(beta <= 1.0);
  assert(r1 <= 1.0);
  if(alpha < 0.0) {
    assert(beta >= 0.0);
    assert(r1 >= 0.0);
    _alpha = beta * rate / (r1 - rate);
  } else if(beta < 0.0) {
    assert(alpha >= 0.0);
    assert(r1 >= 0.0);
    _beta = alpha * (r1 - rate) / rate;
  } else {
    assert(r1 < 0.0);
    _r1 = rate * (alpha + beta) / alpha;
  }
  reset();
}

void OnOffInjectionProcess::reset()
{
  _state = _initial;
}

bool OnOffInjectionProcess::test(int source,int cl)
{
  assert((source >= 0) && (source < _nodes));

  // advance state
  _state[source] = 
    _state[source] ? (RandomFloat() >= _beta) : (RandomFloat() < _alpha);

  // generate packet
  return _state[source] && (RandomFloat() < _r1);
}

CustomizedInjectionProcess::CustomizedInjectionProcess(int nodes,double load): InjectionProcess(nodes,load){
    assert((load>0)&&(load<1));
    for (int i=0;i<26;i++)
        counter[i]=0;
    _nodes=nodes;
};

void CustomizedInjectionProcess::reset()
{
    for (int i=0;i<26;i++)
        counter[i]=0;
}

bool CustomizedInjectionProcess::test(int source,int cl){
    if ((source==1)&&(cl==0)){
        counter[0]++;
        if(counter[0]%500==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==2)&&(cl==1)){
        counter[1]++;
        if(counter[1]%500==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==3)&&(cl==2)){
        counter[2]++;
        if (counter[2]%500==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==3)&&(cl==3)){
        counter[3]++;
        if (counter[3]%500==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==3)&&(cl==4)){
        counter[4]++;
        if(counter[4]%256==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==4)&&(cl==5)){
        counter[5]++;
        if(counter[5]%16==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==4)&&(cl==6)){
        counter[6]++;
        if (counter[6]%16==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==4)&&(cl==7)){
        counter[7]++;
        if (counter[7]%16==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==4)&&(cl==8)){
        counter[8]++;
        if(counter[8]%16==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==4)&&(cl==9)){
        counter[9]++;
        if(counter[9]%16==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==4)&&(cl==10)){
        counter[10]++;
        if (counter[10]%16==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==5)&&(cl==11)){
        counter[11]++;
        if (counter[11]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==6)&&(cl==12)){
        counter[12]++;
        if(counter[12]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==6)&&(cl==13)){
        counter[13]++;
        if(counter[13]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==8)&&(cl==14)){
        counter[14]++;
        if (counter[14]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==9)&&(cl==15)){
        counter[15]++;
        if (counter[15]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==10)&&(cl==16)){
        counter[16]++;
        if(counter[16]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==11)&&(cl==17)){
        counter[17]++;
        if(counter[17]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==11)&&(cl==18)){
        counter[18]++;
        if (counter[18]%32==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==12)&&(cl==19)){
        counter[19]++;
        if (counter[19]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==12)&&(cl==20)){
        counter[20]++;
        if(counter[20]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==13)&&(cl==21)){
        counter[21]++;
        if(counter[21]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==13)&&(cl==22)){
        counter[22]++;
        if (counter[22]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==14)&&(cl==23)){
        counter[23]++;
        if (counter[23]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==14)&&(cl==24)){
        counter[24]++;
        if (counter[24]%125==1){
            return true;
        }else{
            return false;
        }
    }
    if ((source==15)&&(cl==25)){
        counter[25]++;
        if (counter[25]%32==1){
            return true;
        }else{
            return false;
        }
    }
    return false;
}
