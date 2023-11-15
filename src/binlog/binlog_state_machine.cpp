/**
 * Copyright (c) 2023 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include <filesystem>
#include <utility>
#include <fstream>
#include "binlog_state_machine.h"
#include "str.h"
#include "common.h"
#include "fs_util.h"
#include "guard.hpp"

namespace oceanbase {
namespace binlog {
namespace fs = std::filesystem;

ConverterState value_of(uint64_t state)
{
  switch (state) {
    case 0:
      return INIT;
    case 1:
      return RUNNING;
    case 2:
      return FAILED;
    case 3:
      return STOP;
    case 4:
      return DELETE;
    default:
      return UNKNOWN;
  }
}

std::string print(ConverterState state)
{
  switch (state) {
    case INIT:
      return "INIT";
    case RUNNING:
      return "RUNNING";
    case FAILED:
      return "FAILED";
    case STOP:
      return "STOP";
    case DELETE:
      return "DELETE";
    default:
      return "UNKNOWN";
  }
}

StateMachine::StateMachine(std::string _cluster, std::string _tenant)
{
  this->_cluster = std::move(_cluster);
  this->_tenant = std::move(_tenant);
}

const std::string& StateMachine::get_cluster() const
{
  return _cluster;
}

void StateMachine::set_cluster(std::string cluster)
{
  _cluster = std::move(cluster);
}

const std::string& StateMachine::get_tenant() const
{
  return _tenant;
}

void StateMachine::set_tenant(std::string tenant)
{
  _tenant = std::move(tenant);
}

int StateMachine::get_pid() const
{
  return _pid;
}

void StateMachine::set_pid(int pid)
{
  _pid = pid;
}

const std::string& StateMachine::get_work_path() const
{
  return _work_path;
}

void StateMachine::set_work_path(std::string work_path)
{
  _work_path = std::move(work_path);
}

ConverterState StateMachine::get_converter_state() const
{
  return _converter_state;
}

void StateMachine::set_converter_state(ConverterState converter_state)
{
  _converter_state = converter_state;
}

std::string StateMachine::to_string()
{
  std::stringstream str;
  str << _cluster << "\t" << _tenant << "\t" << _pid << "\t" << _work_path << "\t" << _converter_state << "\t"
      << _config << std::endl;
  return str.str();
}

void StateMachine::parse(const std::string& content)
{
  std::vector<std::string> set;
  logproxy::split(content, '\t', set);
  if (set.size() == 6) {
    this->set_cluster(set.at(0));
    this->set_tenant(set.at(1));
    this->set_pid(std::atoi(set.at(2).c_str()));
    this->set_work_path(set.at(3));
    this->set_converter_state(value_of(std::stol(set.at(4))));
    this->set_config(set.at(5));
    OMS_DEBUG("State machine information:{}", this->to_string());
  }
}

std::string StateMachine::get_config()
{
  return _config;
}

void StateMachine::set_config(std::string config)
{
  _config = std::move(config);
}

StateMachine::StateMachine(const std::string& cluster, const std::string& tenant, int pid, const std::string& work_path,
    ConverterState converter_state, const std::string& config)
    : _cluster(cluster),
      _tenant(tenant),
      _pid(pid),
      _work_path(work_path),
      _converter_state(converter_state),
      _config(config)
{}

bool StateMachine::equal(StateMachine& state_machine)
{
  if (std::equal(this->get_cluster().begin(), this->get_cluster().end(), state_machine.get_cluster().c_str()) &&
      std::equal(this->get_tenant().begin(), this->get_tenant().end(), state_machine.get_tenant().c_str())) {
    return true;
  }
  return false;
}

void StateMachine::clone(StateMachine& state_machine)
{
  this->set_cluster(state_machine.get_cluster());
  this->set_tenant(state_machine.get_tenant());
  this->set_config(state_machine.get_config());
  this->set_converter_state(state_machine.get_converter_state());
  this->set_pid(state_machine.get_pid());
  this->set_work_path(state_machine.get_work_path());
}

std::string StateMachine::get_unique_id()
{
  return get_cluster() + "_" + get_tenant();
}

int StateMachineManager::add_state(const std::string& file_name, StateMachine state_machine)
{
  std::lock_guard<std::mutex> op_lock(_op_mutex);
  FILE* fp = logproxy::FsUtil::fopen_binary(file_name);
  if (fp == nullptr) {
    OMS_STREAM_ERROR << "Failed to open file:" << file_name;
    return OMS_FAILED;
  }

  logproxy::FsUtil::append_file(
      fp, (unsigned char*)state_machine.to_string().c_str(), state_machine.to_string().size());
  logproxy::FsUtil::fclose_binary(fp);
  OMS_STREAM_INFO << "add state machine file:" << file_name << " value:" << state_machine.to_string();
  return OMS_OK;
}

int StateMachineManager::update_state(const std::string& file_name, StateMachine state_machine)
{
  std::lock_guard<std::mutex> op_lock(_op_mutex);
  std::vector<StateMachine*> state_machines;
  fetch_state_vector_no_lock(get_default_state_file_path(), state_machines);
  std::string temp = file_name + "_" + std::to_string(getpid()) + ".tmp";
  FILE* fptmp = logproxy::FsUtil::fopen_binary(temp, "a+");
  if (fptmp == nullptr) {
    OMS_STREAM_ERROR << "Failed to open file:" << temp;
    logproxy::release_vector(state_machines);
    return OMS_FAILED;
  }

  for (StateMachine* p_state_machine : state_machines) {
    if (p_state_machine->equal(state_machine)) {
      p_state_machine->clone(state_machine);
    }
    // Write to tmp file
    std::string value = p_state_machine->to_string();
    logproxy::FsUtil::append_file(fptmp, (unsigned char*)value.c_str(), value.size());
  }

  logproxy::FsUtil::fclose_binary(fptmp);
  std::error_code error_code;

  fs::rename(file_name, file_name + "-bak", error_code);
  if (error_code) {
    OMS_STREAM_ERROR << "Failed to rename file:" << file_name << " by " << error_code.message() << temp;
    return OMS_FAILED;
  }

  fs::rename(temp, file_name, error_code);
  if (error_code) {
    OMS_STREAM_ERROR << "Failed to rename file:" << file_name << " by " << error_code.message() << temp;
    return OMS_FAILED;
  }
  if (fs::exists(file_name + "-bak")) {
    fs::remove(file_name + "-bak");
  }
  logproxy::release_vector(state_machines);
  return OMS_OK;
}

int StateMachineManager::fetch_state_vector(std::string file_name, std::vector<StateMachine*>& state_machines)
{
  std::lock_guard<std::mutex> op_lock(_op_mutex);
  std::ifstream ifs(file_name);
  if (!ifs.good()) {
    OMS_ERROR("Failed to open: {},reason:{}", file_name, logproxy::system_err(errno));
    return OMS_FAILED;
  }

  for (std::string line; std::getline(ifs, line);) {
    auto* state_machine = new StateMachine();
    state_machine->parse(line);
    if (!state_machine->get_cluster().empty() && !state_machine->get_config().empty()) {
      state_machines.emplace_back(state_machine);
    } else {
      delete (state_machine);
    }
  }
  return OMS_OK;
}

int StateMachineManager::fetch_state_vector_no_lock(std::string file_name, std::vector<StateMachine*>& state_machines)
{

  std::ifstream ifs(file_name);
  if (!ifs.good()) {
    OMS_STREAM_ERROR << "Failed to open: " << file_name << " errno:" << errno;
    return OMS_FAILED;
  }

  for (std::string line; std::getline(ifs, line);) {
    auto* state_machine = new StateMachine();
    state_machine->parse(line);
    if (!state_machine->get_cluster().empty() && !state_machine->get_config().empty()) {
      state_machines.emplace_back(state_machine);
    } else {
      delete (state_machine);
    }
  }
  return OMS_OK;
}

std::string get_default_state_file_path()
{
  return logproxy::Config::instance().binlog_log_bin_basename.val() + "/" + STATE_FILE_DEFAULT;
}

}  // namespace binlog
}  // namespace oceanbase
