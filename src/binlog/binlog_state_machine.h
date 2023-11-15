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

#pragma once

#include <string>
#include <mutex>
#include "log.h"
#include "config.h"
#include "file_lock.h"

namespace oceanbase {
namespace binlog {
#define STATE_FILE_DEFAULT "state"
enum ConverterState { INIT, RUNNING, FAILED, STOP, DELETE, UNKNOWN };

ConverterState value_of(uint64_t state);
std::string print(ConverterState state);

class StateMachine {
public:
  StateMachine(const std::string& cluster, const std::string& tenant, int pid, const std::string& work_path,
      ConverterState converter_state, const std::string& config);
  StateMachine(std::string _cluster, std::string _tenant);
  StateMachine() = default;

  const std::string& get_cluster() const;

  void set_cluster(std::string cluster);

  const std::string& get_tenant() const;

  void set_tenant(std::string tenant);

  int get_pid() const;

  void set_pid(int pid);

  const std::string& get_work_path() const;

  void set_work_path(std::string work_path);

  ConverterState get_converter_state() const;

  void set_converter_state(ConverterState converter_state);

  std::string get_config();

  void set_config(std::string config);

  std::string to_string();

  void parse(const std::string& content);

  bool equal(StateMachine& state_machine);

  void clone(StateMachine& state_machine);

  std::string get_unique_id();

private:
  std::string _cluster;
  std::string _tenant;
  int _pid = 0;
  std::string _work_path;
  ConverterState _converter_state = INIT;
  std::string _config;
};

class StateMachineManager {
public:
  int add_state(const std::string& file_name, StateMachine state_machine);

  int update_state(const std::string& file_name, StateMachine state_machine);

  int fetch_state_vector(std::string file_name, std::vector<StateMachine*>& state_machines);

  int fetch_state_vector_no_lock(std::string file_name, std::vector<StateMachine*>& state_machines);

private:
  std::mutex _op_mutex;
};

std::string get_default_state_file_path();

}  // namespace binlog
}  // namespace oceanbase
