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

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _map() {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _f_unread = _output.bytes_read();
    size_t f_unacceptable = _f_unread + _capacity;
    if (eof) {
        _eof = true;
        _eof_index = index + data.size();
    }
    if ((index == 0 && _f_unassembled == 0) || (index <= _f_unassembled && index + data.size() > _f_unassembled)) {
        // write data
        size_t start_index = max(index, _f_unassembled);
        size_t len = min(f_unacceptable, index + data.size()) - start_index;
        size_t pos = start_index - index;
        size_t end_index = start_index + len;
        string writable_data = data.substr(pos, len);
        vector<size_t> st;
        for (auto i : _map) {
            if (i.first == end_index) {
                writable_data += i.second;
                st.push_back(i.first);
                end_index++;
            } else if (i.first < end_index) {
                st.push_back(i.first);
            }
        }
        _output.write(writable_data);
        _f_unassembled += writable_data.size();
        if (_f_unassembled > f_unacceptable) {
            cout << "Write exceed capacity!" << endl;
        }
        for (auto i : st) {
            _map.erase(i);
        }
    } else if (index > _f_unassembled && index < f_unacceptable) {
        // store data
        size_t start_index = index;
        size_t len = min(f_unacceptable, index + data.size()) - start_index;
        string storable_data = data.substr(0, len);
        for (auto c : storable_data) {
            _map[start_index] = c;
            start_index++;
        }
    }
    if (_eof && _f_unassembled >= _eof_index) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _map.size(); }

bool StreamReassembler::empty() const { return _map.empty() && _output.buffer_empty(); }
