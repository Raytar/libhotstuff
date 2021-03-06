/**
 * Copyright 2018 VMware
 * Copyright 2018 Ted Yin
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <cassert>
#include <chrono>
#include <random>
#include <mutex>
#include <signal.h>
#include <sys/time.h>
#include <unistd.h>

#include "salticidae/type.h"
#include "salticidae/netaddr.h"
#include "salticidae/network.h"
#include "salticidae/util.h"

#include "hotstuff/util.h"
#include "hotstuff/type.h"
#include "hotstuff/client.h"

using salticidae::Config;

using hotstuff::ReplicaID;
using hotstuff::NetAddr;
using hotstuff::EventContext;
using hotstuff::MsgReqCmd;
using hotstuff::MsgRespCmd;
using hotstuff::CommandDummy;
using hotstuff::HotStuffError;
using hotstuff::uint256_t;
using hotstuff::opcode_t;
using hotstuff::command_t;

EventContext ec;
ReplicaID proposer;
size_t max_async_num;
int max_iter_num;
int exit_after;
uint32_t cid;
uint32_t cnt = 0;
uint32_t nfaulty;
struct timespec sleep_time;
bool stop = false;

struct Request {
    command_t cmd;
    size_t confirmed;
    salticidae::ElapsedTime et;
    Request(const command_t &cmd): cmd(cmd), confirmed(0) { et.start(); }
};

using Net = salticidae::MsgNetwork<opcode_t>;

std::unordered_map<ReplicaID, Net::conn_t> conns;
std::unordered_map<const uint256_t, Request> waiting;
std::mutex waiting_lock;
std::vector<NetAddr> replicas;
std::vector<std::pair<std::chrono::time_point<std::chrono::high_resolution_clock>, double>> elapsed;
Net mn(ec, Net::Config());

void connect_all() {
    for (size_t i = 0; i < replicas.size(); i++)
        conns.insert(std::make_pair(i, mn.connect_sync(replicas[i])));
}

bool try_send(bool check = true) {
    std::lock_guard<std::mutex> lg(waiting_lock);
    if ((!check || waiting.size() < max_async_num) && max_iter_num)
    {
        auto cmd = new CommandDummy(cid, cnt++);
        MsgReqCmd msg(*cmd);
        for (auto &p: conns) mn.send_msg(msg, p.second);
#ifndef HOTSTUFF_ENABLE_BENCHMARK
        HOTSTUFF_LOG_INFO("send new cmd %.10s",
                            get_hex(cmd->get_hash()).c_str());
#endif
        waiting.insert(std::make_pair(
            cmd->get_hash(), Request(cmd)));
        
        if (max_iter_num > 0)
            max_iter_num--;

        return true;
    }
    return false;
}

void client_resp_cmd_handler(MsgRespCmd &&msg, const Net::conn_t &) {
    std::lock_guard<std::mutex> lg(waiting_lock);
    auto &fin = msg.fin;
    HOTSTUFF_LOG_DEBUG("got %s", std::string(msg.fin).c_str());
    const uint256_t &cmd_hash = fin.cmd_hash;
    auto it = waiting.find(cmd_hash);
    auto &et = it->second.et;
    if (it == waiting.end()) return;
    et.stop();
    if (++it->second.confirmed <= nfaulty) return; // wait for f + 1 ack
#ifndef HOTSTUFF_ENABLE_BENCHMARK
    HOTSTUFF_LOG_INFO("got %s, wall: %.3f, cpu: %.3f",
                        std::string(fin).c_str(),
                        et.elapsed_sec, et.cpu_elapsed_sec);
#else
    auto now = std::chrono::high_resolution_clock::now();
    elapsed.push_back(std::make_pair(now, et.elapsed_sec));
#endif
    waiting.erase(it);
}

std::pair<std::string, std::string> split_ip_port_cport(const std::string &s) {
    auto ret = salticidae::trim_all(salticidae::split(s, ";"));
    return std::make_pair(ret[0], ret[1]);
}

void send_requests() {
    auto start_time = std::chrono::steady_clock::now();
    while (!stop) {
        try_send();
        if (!max_iter_num) {
            return;
        }

        if (sleep_time.tv_nsec > 0) {
            nanosleep(&sleep_time, nullptr);
        }

        if (exit_after > 0) {
            auto now = std::chrono::steady_clock::now();
            auto diff = std::chrono::duration_cast<std::chrono::seconds>(now-start_time).count();
            if (diff >= exit_after) {
                raise(SIGINT);
            }
        }
    }
}

int main(int argc, char **argv) {
    Config config("hotstuff.conf");

    auto opt_idx = Config::OptValInt::create(0);
    auto opt_replicas = Config::OptValStrVec::create();
    auto opt_max_iter_num = Config::OptValInt::create(100);
    auto opt_max_async_num = Config::OptValInt::create(10);
    auto opt_cid = Config::OptValInt::create(-1);
    auto opt_request_rate = Config::OptValInt::create(-1);
    auto opt_exit_after = Config::OptValInt::create(-1);

    auto shutdown = [&](int) { ec.stop(); };
    salticidae::SigEvent ev_sigint(ec, shutdown);
    salticidae::SigEvent ev_sigterm(ec, shutdown);
    ev_sigint.add(SIGINT);
    ev_sigterm.add(SIGTERM);

    mn.reg_handler(client_resp_cmd_handler);
    mn.start();

    config.add_opt("idx", opt_idx, Config::SET_VAL);
    config.add_opt("cid", opt_cid, Config::SET_VAL);
    config.add_opt("replica", opt_replicas, Config::APPEND);
    config.add_opt("iter", opt_max_iter_num, Config::SET_VAL);
    config.add_opt("max-async", opt_max_async_num, Config::SET_VAL);
    config.add_opt("request-rate", opt_request_rate, Config::SET_VAL);
    config.add_opt("exit-after", opt_exit_after, Config::SET_VAL);

    config.parse(argc, argv);
    auto idx = opt_idx->get();
    max_iter_num = opt_max_iter_num->get();
    max_async_num = opt_max_async_num->get();
    exit_after = opt_exit_after->get();
    auto request_rate = opt_request_rate->get();
    if (request_rate > 0) {
        sleep_time.tv_nsec = 1e9 / (request_rate);
    }
    std::vector<std::string> raw;
    for (const auto &s: opt_replicas->get())
    {
        auto res = salticidae::trim_all(salticidae::split(s, ","));
        if (res.size() < 1)
            throw HotStuffError("format error");
        raw.push_back(res[0]);
    }

    if (!(0 <= idx && (size_t)idx < raw.size() && raw.size() > 0))
        throw std::invalid_argument("out of range");
    cid = opt_cid->get() != -1 ? opt_cid->get() : idx;
    for (const auto &p: raw)
    {
        auto _p = split_ip_port_cport(p);
        size_t _;
        replicas.push_back(NetAddr(NetAddr(_p.first).ip, htons(stoi(_p.second, &_))));
    }

    nfaulty = (replicas.size() - 1) / 3;
    HOTSTUFF_LOG_INFO("nfaulty = %zu", nfaulty);
    connect_all();

    auto start = std::chrono::high_resolution_clock::now();

    // send requests in a separate thread
    std::thread req_thread(send_requests);

    ec.dispatch();
    stop = true;
    req_thread.join();

#ifdef HOTSTUFF_ENABLE_BENCHMARK
    auto prev_time = start;
    for (const auto &e: elapsed)
    {
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(e.first-prev_time).count();
        fprintf(stderr, "%ld %ld\n", ns, (long)(e.second * 1e6));
        prev_time = e.first;
    }
#endif
    return 0;
}
