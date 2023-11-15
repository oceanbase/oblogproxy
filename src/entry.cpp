/**
 * Copyright (c) 2021 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */

#include <csignal>
#include "log.h"
#include "option.h"
#include "config.h"
#include "ob_aes256.h"
#include "arranger.h"
#include "binlog_server.h"
#include "environmental.h"

using namespace oceanbase::logproxy;
// using namespace oceanbase::binlog;
int main(int argc, char** argv)
{
  Config& conf = Config::instance();
  conf.process_name_address.set((uint64_t)argv[0]);

  OmsOptions options;
  options.add(OmsOption('h', "help", false, "help", [&](const std::string&) {
    options.usage();
    exit(0);
  }));
  options.add(OmsOption('v', "version", false, "program version", [&](const std::string&) {
    printf("version: " __OMS_VERSION__ "\n");
    exit(0);
  }));
  options.add(OmsOption('f', "file", true, "configuration json file", [&](const std::string& optarg) {
    if (conf.load(optarg) != OMS_OK) {
      OMS_INFO("failed to load config: {}", optarg);
      exit(-1);
    }
  }));
  options.add(OmsOption('P', "listen_port", true, "listen port", [&](const std::string& optarg) {
    conf.service_port.set(::strtol(optarg.c_str(), nullptr, 10));
  }));
  options.add(OmsOption('D', "debug", false, "debug mode not send message", [&](const std::string& optarg) {
    conf.debug.set(true);
    printf("enable DEBUG mode");
  }));
  options.add(OmsOption('V', "verbose", false, "print plentiful log", [&](const std::string& optarg) {
    conf.verbose.set(true);
    printf("enable VERBOSE log");
  }));
  options.add(OmsOption('x', "encrypt", true, "encrypt text", [&](const std::string& optarg) {
    AES aes;
    char* encrypted = nullptr;
    int encrypted_len = 0;
    int ret = aes.encrypt(optarg.c_str(), optarg.size(), &encrypted, encrypted_len);
    if (ret != OMS_OK) {
      printf("Failed to encrypt\n");
      exit(-1);
    }
    std::string hex;
    dumphex(encrypted, encrypted_len, hex);
    printf("%s\n", hex.c_str());
    free(encrypted);
    exit(0);
  }));
  options.add(OmsOption('y', "decrypt", true, "decrypt text", [&](const std::string& optarg) {
    AES aes;
    std::string binary;
    hex2bin(optarg.c_str(), optarg.size(), binary);
    if (binary.empty()) {
      printf("Failed to decrypt, invalid hex input\n");
      exit(-1);
    }

    char* plain = nullptr;
    int plain_len = 0;
    int ret = aes.decrypt(binary.c_str(), binary.size(), &plain, plain_len);
    if (ret != OMS_OK) {
      printf("Failed to decrypt\n");
      exit(-1);
    }
    printf("%s\n", std::string(plain, plain_len).c_str());
    free(plain);
    exit(0);
  }));

  std::string file = "./conf/conf.json";

  struct option* long_options = options.long_pattern();
  std::string pattern = options.pattern();

  // read file config first
  std::map<int, std::string> opts;
  for (int opt = 0; (opt = getopt_long(argc, argv, pattern.c_str(), long_options, nullptr)) != -1;) {
    if (opt == 'f') {
      options.get('f')->func(std::string(optarg));
    } else {
      opts.emplace(opt, optarg == nullptr ? "" : optarg);
    }
  }

  // command line options overwrite configuration options
  for (auto& opt_entry : opts) {
    OmsOption* option = options.get(opt_entry.first);
    if (option == nullptr) {
      options.usage();
      return 0;
    } else {
      option->func(option->is_arg ? opt_entry.second : "");
    }
  }

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);

  init_log(argv[0]);
  print_env_info();
  int ret = OMS_OK;
  if (conf.binlog_mode.val()) {
    ret = oceanbase::binlog::BinlogServer::run_foreground();
  } else {
    if (Arranger::instance().init() != OMS_OK) {
      ::exit(-1);
    }
    ret = Arranger::instance().run_foreground();
  }
  OMS_WARN("!!!LogProxy Exit, ret: {}", ret);
  if (ret != OMS_OK) {
    ::exit(-1);
  }
  return 0;
}