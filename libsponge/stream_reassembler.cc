#include "stream_reassembler.hh"

#include <iostream>
#include <vector>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

int StreamReassembler::check_write(const string &data, const uint64_t index) const {
    if (_last == int(index - 1)) {
        return 0;
    } else if (_last > int(index - 1)) {
        uint64_t data_last = index + data.size() - 1;
        if (_last < int(data_last)) {
            return 1;
        } else {
            return 2;
        }
    }
    return 3;
}

void StreamReassembler::push_stored_substring() {
    vector<size_t> s0;
    for (const auto &element : _umap) {
        string data = element.second;
        size_t index = element.first;
        int check_write = StreamReassembler::check_write(data, index);
        if (check_write == 0) {
            if (StreamReassembler::write_data(data, index, false)) {
                s0.push_back(index);
            }
        } else if (check_write == 1) {
            uint64_t data_last = index + data.size() - 1;
            size_t pos = _last - index + 1;
            size_t len = data_last - _last;
            string writable_data = data.substr(pos, len);
            if (StreamReassembler::write_data(writable_data, index, false)) {
                s0.push_back(index);
            }
        } else if (check_write == 2) {
            s0.push_back(index);
        }
    }
    for (const auto &idx : s0) {
        _umap.erase(idx);
    }
    vector<size_t> s1;
    for (const auto &element : _eofmap) {
        string data = element.second;
        size_t index = element.first;
        int check_write = StreamReassembler::check_write(data, index);
        if (check_write == 0) {
            if (StreamReassembler::write_data(data, index, true)) {
                s1.push_back(index);
            }
        } else if (check_write == 1) {
            uint64_t data_last = index + data.size() - 1;
            size_t pos = _last - index + 1;
            size_t len = data_last - _last;
            string writable_data = data.substr(pos, len);
            if (StreamReassembler::write_data(writable_data, index, true)) {
                s1.push_back(index);
            }
        } else if (check_write == 2) {
            s1.push_back(index);
        }
    }
    for (const auto &idx : s1) {
        _eofmap.erase(idx);
    }
}

void StreamReassembler::store_data(const std::string &data, const uint64_t index, const bool eof) {
    string storable_data = data;
    if (index + storable_data.size() > _last + _capacity + 1) {
        storable_data = data.substr(0, _last + _capacity + 1 - index);
    }
    if (eof) {
        _eofmap[index] = storable_data;
    } else {
        _umap[index] = storable_data;
    }
}

bool StreamReassembler::write_data(const std::string &data, const uint64_t index, const bool eof) {
    size_t remaining_capacity = _output.remaining_capacity();
    if (remaining_capacity == 0) {
        return false;
    }
    string writable_data = data;
    if (writable_data.size() <= remaining_capacity) {
        _output.write(writable_data);
        _last += int(writable_data.size());
        if (eof) {
            _output.end_input();
        }
    } else {
        writable_data = data.substr(0, remaining_capacity);
        _output.write(writable_data);
        _last += int(writable_data.size());
        string remained_data = data.substr(remaining_capacity);
        StreamReassembler::store_data(remained_data, index + remaining_capacity, eof);
    }
    return true;
}

void StreamReassembler::update_map() {
    if (_umap.size() > 1) {
        vector<size_t> s;
        size_t index;
        string data;
        for (auto i = _umap.begin(); i != _umap.end(); i++) {
            if (i == _umap.begin()) {
                index = i->first;
                data = i->second;
                continue;
            }
            if (i->first <= index + data.size()) {
                // able to merge
                size_t pos = index + data.size() - i->first;
                if (pos < i->second.size()) {
                    string mergable_data = i->second.substr(pos);
                    data += mergable_data;
                    _umap[index] = data;
                }
                s.push_back(i->first);
            } else {
                index = i->first;
                data = i->second;
            }
        }
        for (auto idx : s) {
            _umap.erase(idx);
        }
    }
    if (_eofmap.size() > 1) {
        vector<size_t> s;
        size_t index = 0;
        string data;
        for (auto i = _eofmap.begin(); i != _eofmap.end(); i++) {
            if (i == _eofmap.begin()) {
                index = i->first;
                data = i->second;
                continue;
            }
            if (i->first <= index + data.size()) {
                // able to merge
                size_t pos = index + data.size() - i->first;
                if (pos < i->second.size()) {
                    string mergable_data = i->second.substr(pos);
                    data += mergable_data;
                    _eofmap[index] = data;
                }
                s.push_back(i->first);
            } else {
                index = i->first;
                data = i->second;
            }
        }
        for (auto idx : s) {
            _eofmap.erase(idx);
        }
    }
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity), _capacity(capacity), _umap(), _eofmap() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    int check_write = StreamReassembler::check_write(data, index);
    if (check_write == 0) {
        StreamReassembler::write_data(data, index, eof);
        StreamReassembler::push_stored_substring();
    } else if (check_write == 1) {
        uint64_t data_last = index + data.size() - 1;
        size_t pos = _last - index + 1;
        size_t len = data_last - _last;
        string possible_writable_data = data.substr(pos, len);
        StreamReassembler::write_data(possible_writable_data, index, eof);
        StreamReassembler::push_stored_substring();
    } else if (check_write == 3) {
        StreamReassembler::store_data(data, index, eof);
        StreamReassembler::update_map();
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t num_bytes = 0;
    for (const auto &data : _umap) {
        num_bytes += data.second.size();
    }
    for (const auto &data : _eofmap) {
        num_bytes += data.second.size();
    }
    return num_bytes;
}

bool StreamReassembler::empty() const { return _umap.empty() && _eofmap.empty(); }
