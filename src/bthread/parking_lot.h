// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

// bthread - A M:N threading library to make applications more concurrent.

// Date: 2017/07/27 23:07:06

#ifndef BTHREAD_PARKING_LOT_H
#define BTHREAD_PARKING_LOT_H

#include "butil/atomicops.h"
#include "bthread/sys_futex.h"

namespace bthread {

// Park idle workers.
class BAIDU_CACHELINE_ALIGNMENT ParkingLot {
public:
    class State {
    public:
        State(): val(0) {}
        bool stopped() const { return val & 1; }
    private:
    friend class ParkingLot;
        State(int val) : val(val) {}
        int val;
    };

    ParkingLot() : _pending_signal(0) {}

    // Wake up at most `num_task' workers.
    // Returns #workers woken up.   唤醒num_task个等待在_pending_signal上的线程
    int signal(int num_task) {
        // 在调用之前对_pending_signal执行了原子加，加的是num_task << 1，
        // 之所以要左移是因为第一位是用于表明是否停止的标识位。启动一个bthread就会调用一次signal(1)。
        _pending_signal.fetch_add((num_task << 1), butil::memory_order_release);
        return futex_wake_private(&_pending_signal, num_task);
    }

    // Get a state for later wait().
    // get_state是获取用于wait的状态，就是直接返回_pending_signal的值，返回类型是State
    State get_state() {
        return _pending_signal.load(butil::memory_order_acquire);
    }

    // Wait for tasks.
    // If the `expected_state' does not match, wait() may finish directly.
    void wait(const State& expected_state) {
        futex_wait_private(&_pending_signal, expected_state.val, NULL);
    }

    // Wakeup suspended wait() and make them unwaitable ever. 
    void stop() {
        _pending_signal.fetch_or(1);
        futex_wake_private(&_pending_signal, 10000);
    }
private:
    // higher 31 bits for signalling, LSB for stopping.
    butil::atomic<int> _pending_signal;  // wait和signal的futex变量, ，留了最低位作为一个是否停止的标识， 只会递增
};

}  // namespace bthread

#endif  // BTHREAD_PARKING_LOT_H
