#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <algorithm>
#include <iostream>
#include <random>
#include <vector>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(retx_timeout)
    , _outstanding_segments() {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t count = 0;
    for (auto item : _outstanding_segments) {
        count += item.second.length_in_sequence_space();
    }
    return count;
}

void TCPSender::fill_window() {
    // if sent fin and received corresponding ack, return
    if (_fin_sent && _next_seqno == _absolute_ackno) {
        return;
    }
    // if sent data but didn't receive corresponding ack, return
    if (_next_seqno > _absolute_ackno && _next_seqno - _absolute_ackno == _window_size) {
        return;
    }
    if (_next_seqno < _absolute_ackno) {
        cout << "Should never enter here: received ack lower than next seq" << endl;
        return;
    }
    if (_next_seqno - _absolute_ackno > _window_size) {
        cout << "Should never enter here: sent data larger than window" << endl;
        return;
    }
    TCPSegment seg = TCPSegment();
    uint16_t window_size;
    uint16_t data_size;
    int flag = 0;

    // write seq no
    if (_absolute_ackno == 0) {
        seg.header().seqno = _isn;
    } else {
        if (_absolute_ackno - 1 < _stream.bytes_read()) {
            size_t outstanding_bytes = _stream.bytes_read() - _absolute_ackno + 1;
            seg.header().seqno = _ackno + outstanding_bytes;
        } else {
            seg.header().seqno = _ackno;
        }
    }

    // determine window size
    if (_window_size == 0) {
        window_size = 1;
    } else {
        window_size = _window_size;
    }
    // whether there are bytes outstanding and they should have been in the window
    if (_absolute_ackno > 0 && _absolute_ackno - 1 < _stream.bytes_read()) {
        size_t outstanding_bytes = _stream.bytes_read() - _absolute_ackno + 1;
        if (_window_size > outstanding_bytes) {
            window_size = min<size_t>(window_size, _window_size - outstanding_bytes);
        } else {
            window_size = 1;
        }
    }
    // data size is the payload size
    data_size = window_size;

    // write syn and check whether need to reduce data size
    if (_absolute_ackno == 0) {
        seg.header().syn = true;
        if (data_size > 0) {
            data_size -= 1;
        }
    }

    // write payload
    string data = _stream.read(min<size_t>(data_size, TCPConfig::MAX_PAYLOAD_SIZE));
    Buffer buffer = Buffer(std::move(data));
    seg.payload() = buffer;

    // write fin when stream reaches eof and there are additional spaces in window size
    if (_stream.eof() && seg.length_in_sequence_space() < window_size) {
        seg.header().fin = true;
    }

    // if window size still has space and the stream still has data, call fill_window() again
    if (!_stream.buffer_empty() && seg.length_in_sequence_space() < window_size) {
        flag = 1;
    }

    if (seg.length_in_sequence_space() > 0) {
        uint64_t absolute_seqno = unwrap(seg.header().seqno, _isn, _absolute_ackno - 1);
        uint64_t absolute_ackno = absolute_seqno + seg.length_in_sequence_space();
        // if payload is empty and the absolute ackno of the segment is already in the outstanding segment, return
        if (seg.payload().copy().empty() && _outstanding_segments.find(absolute_ackno) != _outstanding_segments.end()) {
            return;
        }
        _next_seqno += seg.length_in_sequence_space();
        _segments_out.push(seg);
        _outstanding_segments[absolute_ackno] = seg;
        if (!_timer.is_alive()) {
            _timer.start(_rto);
        }
        if (seg.header().fin) {
            _fin_sent = true;
        }
    }

    if (flag == 1) {
        TCPSender::fill_window();
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = unwrap(ackno, _isn, _absolute_ackno - 1);
    if (absolute_ackno > _next_seqno) {
        return;
    }
    if (absolute_ackno > _absolute_ackno) {
        _rto = _initial_retransmission_timeout;
        _timer.stop();
        _timer.start(_rto);
        _consecutive_retransmissions = 0;
        _ackno = ackno;
        _absolute_ackno = absolute_ackno;
        _window_size = window_size;
    } else if (absolute_ackno == _absolute_ackno) {
        _window_size = window_size;
    } else {
        return;
    }
    vector<uint64_t> st;
    for (auto item : _outstanding_segments) {
        if (_absolute_ackno >= item.first) {
            st.push_back(item.first);
        }
    }
    for (auto key : st) {
        _outstanding_segments.erase(key);
    }
    if (_outstanding_segments.empty()) {
        _timer.stop();
    }
    TCPSender::fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.is_alive()) {
        return;
    }
    _timer.increment_time(ms_since_last_tick);
    if (_timer.is_expired()) {
        for (auto item : _outstanding_segments) {
            _segments_out.push(item.second);
            break;
        }
        if (_window_size > 0) {
            _consecutive_retransmissions += 1;
            _rto *= 2;
        }
        _timer.start(_rto);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg = TCPSegment();
    if (seg.header().syn) {
        seg.header().seqno = _isn;
    } else {
        seg.header().seqno = _ackno;
    }
    _segments_out.push(seg);
}
