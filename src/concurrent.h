#include <vector>
#include <stdlib.h>
#include <iostream>
#include <functional>
#include <ctime>
#include <list>
#include <mutex>

template <class T>
class CuckooConcurrentHashSet {
    const int PROBE_SIZE = 8;
    const int THRESHOLD = PROBE_SIZE / 2;
    int limit;
    int capacity;
    std::vector<std::vector<std::list<T>>> table;
    std::vector<std::vector<std::recursive_mutex*>> locks;

    int hash0(const T val) {
        std::hash<T> hasher;
        return hasher(val) % capacity;
    }

    int hash1(const T val) {
        std::hash<T> hasher;
        return (hasher(val) >> 16) % capacity; // Shift to avoid identical hashes
    }

    bool relocate(int i, int hi) {
        int hj = 0;
        int j = 1 - i;
        for (int round = 0; round < limit; round++) {
            T val = table[i][hi].front();
            switch (i) {
                case 0: hj = hash1(val) % capacity; break;
                case 1: hj = hash0(val) % capacity; break;
            }
            acquire(val);
            auto it = std::find(table[i][hi].begin(), table[i][hi].end(), val);
            if (it != table[i][hi].end()) {
                table[i][hi].erase(it);
                if (table[j][hj].size() < THRESHOLD) {
                    table[j][hj].push_back(val);
                    release(val);
                    return true;
                } else if (table[j][hj].size() < PROBE_SIZE) {
                    table[j][hj].push_back(val);
                    i = 1 - i;
                    hi = hj;
                    j = 1 - j;
                    release(val);
                } else {
                    table[i][hi].push_back(val);
                    release(val);
                    return false;
                }
            } else if (table[i][hi].size() >= THRESHOLD) {
                release(val);
                continue;
            } else {
                release(val);
                return true;
            }
        }
        return false;
    }

    void acquire(const T val) {
        locks[0][hash0(val) % locks[0].size()]->lock();
        locks[1][hash1(val) % locks[1].size()]->lock();
    }

    void release(const T val) {
        locks[0][hash0(val) % locks[0].size()]->unlock();
        locks[1][hash1(val) % locks[1].size()]->unlock();
    }

    void resize() {
        for (auto lock : locks[0]) {
            lock->lock();
        }
        int oldCapacity = capacity;
        capacity *= 2;
        limit *= 2;
        std::vector<std::vector<std::list<T>>> old_table(table);
        table.clear();
        for (int i = 0; i < 2; i++) {
            std::vector<std::list<T>> row;
            for (int j = 0; j < capacity; j++) {
                std::list<T> probe_set;
                row.push_back(probe_set);
            }
            table.push_back(row);
        }
        for (auto row : old_table) {
            for (auto probe_set : row) {
                for (auto entry : probe_set) {
                    add(entry);
                }
            }
        }
        for (auto lock : locks[0])
            lock->unlock();
    }

    bool present(const T val) {
        std::list<T> set0 = table[0][hash0(val) % capacity];
        if (std::find(set0.begin(), set0.end(), val) != set0.end()) {
            return true;
        } else {
            std::list<T> set1 = table[1][hash1(val) % capacity];
            if (std::find(set1.begin(), set1.end(), val) != set1.end()) {
                return true;
            }
        }
        return false;
    }

public:
    CuckooConcurrentHashSet(int capacity) : capacity(capacity), limit(capacity / 2) {
        for (int i = 0; i < 2; i++) {
            std::vector<std::list<T>> row;
            std::vector<std::recursive_mutex*> locks_row;
            for (int j = 0; j < capacity; j++) {
                std::list<T> probe_set;
                row.push_back(probe_set);
                locks_row.emplace_back(new std::recursive_mutex());
            }
            table.emplace_back(row);
            locks.emplace_back(locks_row);
        }
    }

    ~CuckooConcurrentHashSet() {
        for (auto row : table) {
            for (auto probe_set : row) {
                probe_set.clear();
            }
            row.clear();
        }
        table.clear();
    }

    bool add(const T val) {
        acquire(val);
        int h0 = hash0(val) % capacity;
        int h1 = hash1(val) % capacity;
        int i = -1;
        int h = -1;
        bool mustResize = false;
        if (present(val)) {
            release(val);
            return false;
        }
        if (table[0][h0].size() < THRESHOLD) {
            table[0][h0].push_back(val);
            release(val);
            return true;
        } else if (table[1][h1].size() < THRESHOLD) {
            table[1][h1].push_back(val);
            release(val);
            return true;
        } else if (table[0][h0].size() < PROBE_SIZE) {
            table[0][h0].push_back(val);
            i = 0;
            h = h0;
        } else if (table[1][h1].size() < PROBE_SIZE) {
            table[1][h1].push_back(val);
            i = 1;
            h = h1;
        } else {
            mustResize = true;
        }
        release(val);

        if (mustResize) {
            resize();
            add(val);
        } else if (!relocate(i, h)) {
            resize();
        }
        return true;
    }

    bool remove(const T val) {
        acquire(val);
        int h0 = hash0(val) % capacity;
        auto it0 = std::find(table[0][h0].begin(), table[0][h0].end(), val);
        if (it0 != table[0][h0].end()) {
            table[0][h0].erase(it0);
            release(val);
            return true;
        } else {
            int h1 = hash1(val) % capacity;
            auto it1 = std::find(table[1][h1].begin(), table[1][h1].end(), val);
            if (it1 != table[1][h1].end()) {
                table[1][h1].erase(it1);
                release(val);
                return true;
            }
        }
        release(val);
        return false;
    }

    bool contains(const T val) {
        acquire(val);
        std::list<T> set0 = table[0][hash0(val) % capacity];
        if (std::find(set0.begin(), set0.end(), val) != set0.cend()) {
            release(val);
            return true;
        } else {
            std::list<T> set1 = table[1][hash1(val) % capacity];
            if (std::find(set1.begin(), set1.end(), val) != set1.cend()) {
                release(val);
                return true;
            }
        }
        release(val);
        return false;
    }

    int size() {
        int size = 0;
        for (auto row : table) {
            for (auto probe_set : row) {
                size += probe_set.size();
            }
        }
        return size;
    }

    bool populate(const std::vector<T> entries) {
        for (T entry : entries) {
            if (!add(entry)) {
                std::cout << "Duplicate entry attempted for populate!" << std::endl;
                return false;
            }
        }
        return true;
    }
};