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

#include "thread_pool_executor.h"

#include <thread>
#include <string>
#include <iostream>
#include "gtest/gtest.h"

using std::cout;
using std::endl;

using namespace oceanbase::binlog;

// std::mutex cout_mutex;

static void void_f0()
{
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func void_f0 is executed by thread " << std::this_thread::get_id() << endl;
}

static void void_f1(int a = 100)
{
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func void_f1 is executed by thread " << std::this_thread::get_id();
  cout << ", a = " << a << endl;
}

static void void_f2(int a, int b)
{
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func void_f2 is executed by thread " << std::this_thread::get_id();
  cout << ", a = " << a;
  cout << ", b = " << b;
  cout << endl;
}

static int int_f0()
{
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func int_f0 is executed by thread " << std::this_thread::get_id() << endl;
  return 0;
}

static int int_f1(int a = -1)
{
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func int_f1 is executed by thread " << std::this_thread::get_id();
  cout << ", a = " << a << endl;
  return a;
}

static int int_f2(int a, int b)
{
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func int_f2 is executed by thread " << std::this_thread::get_id();
  cout << ", a = " << a;
  cout << ", b = " << b;
  cout << endl;
  return a + b;
}

static std::string string_f0()
{
  usleep(10000);
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func string_f0 is executed by thread " << std::this_thread::get_id() << endl;
  return "string_f0";
}

static std::string string_f1(std::string a = "Hello")
{
  usleep(10000);
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func string_f1 is executed by thread " << std::this_thread::get_id();
  cout << ", a = " << a << endl;
  return a;
}

static std::string string_f2(std::string&& a, std::string&& b)
{
  usleep(10000);
  //  std::lock_guard<std::mutex> lock(cout_mutex);
  cout << "func string_f2 is executed by thread " << std::this_thread::get_id();
  cout << ", a = " << a;
  cout << ", b = " << b;
  cout << endl;
  return a + b;
}

TEST(ThreadPoolExecutor, ThreadPoolExecutor)
{
  ThreadPoolExecutor executor(2);

    auto f_void_f0 = executor.submit(void_f0);
    auto f_void_f1 = executor.submit(void_f1, 1000);
    auto f_void_f2 = executor.submit(void_f2, 200, 300);

    auto f_int_f0 = executor.submit(int_f0);
    auto f_int_f1 = executor.submit(int_f1, 10000);
    auto f_int_f2 = executor.submit(int_f2, 20, 30);
  {
    std::string str = "yang1";
    auto f_string_f1 = executor.submit(string_f1, str);
    str = "";
  }

  {
    std::string str = "yang";
    auto f_string_f0 = executor.submit(string_f0);
  }

  {
    std::string str = "test2";
    auto f_string_f1 = executor.submit(string_f1, str);
    str = "";
  }

  {
    std::string str = "yang";
    auto f_string_f2 = executor.submit(string_f2, "yang", "lin");
  }

    f_void_f0.get();
    f_void_f1.get();
    f_void_f2.get();

    //    {
    //      //    std::lock_guard<std::mutex> lock(cout_mutex);
    //      cout << "f_int_f0.get() = " << f_int_f0.get() << endl;
    //    }
    //    {
    //      //    std::lock_guard<std::mutex> lock(cout_mutex);
    //      cout << "f_int_f1.get() = " << f_int_f1.get() << endl;
    //    }
    //    {
    //      //    std::lock_guard<std::mutex> lock(cout_mutex);
    //      cout << "f_int_f2.get() = " << f_int_f2.get() << endl;
    //    }
    //
    //    {
    //      //    std::lock_guard<std::mutex> lock(cout_mutex);
    //      cout << "f_string_f0.get() = " << f_string_f0.get() << endl;
    //    }
    //    {
    //      //    std::lock_guard<std::mutex> lock(cout_mutex);
    //      cout << "f_string_f1.get() = " << f_string_f1.get() << endl;
    //    }
    //    {
    //      //    std::lock_guard<std::mutex> lock(cout_mutex);
    //      cout << "f_string_f2.get() = " << f_string_f2.get() << endl;
    //    }
}
