#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <vector>
#include <iostream>

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
    , _outstanding_segments(){}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t count = 0;
    for (auto item : _outstanding_segments) {
//        cout << "bytes in flight" << endl;
//        cout << item.second.header().syn << endl;
//        cout << item.second.length_in_sequence_space() << endl;
        count += item.second.length_in_sequence_space();
    }
    return count;
}

void TCPSender::fill_window() {
    cout << "in fill window" << endl;
    TCPSegment seg = TCPSegment();
    uint16_t window_size;
    if (_window_size == 0) {
        window_size = 1;
    } else {
        window_size = _window_size;
    }
    // whether the segment includes SYN
    if (_absolute_ackno == 0) {
        // whether the segment includes FIN
        if (window_size >= _stream.buffer_size()) {
            if (_stream.buffer_size() > TCPConfig::MAX_PAYLOAD_SIZE - 2) {
                window_size = TCPConfig::MAX_PAYLOAD_SIZE - 2;
            }
        } else {
            if (window_size > TCPConfig::MAX_PAYLOAD_SIZE - 1) {
                window_size = TCPConfig::MAX_PAYLOAD_SIZE - 1;
            }
        }
    } else {
        if (window_size >= _stream.buffer_size()) {
            if (_stream.buffer_size() > TCPConfig::MAX_PAYLOAD_SIZE - 1) {
                window_size = TCPConfig::MAX_PAYLOAD_SIZE - 1;
            }
        } else {
            if (window_size > TCPConfig::MAX_PAYLOAD_SIZE) {
                window_size = TCPConfig::MAX_PAYLOAD_SIZE;
            }
        }
    }
    // whether there are bytes outstanding but they should have been in the window
    if (_absolute_ackno > 0 && _absolute_ackno - 1 < _stream.bytes_read()) {
        size_t outstanding_bytes = _stream.bytes_read() - _absolute_ackno + 1;
        if (window_size > outstanding_bytes) {
            window_size -= outstanding_bytes;
        } else {
            window_size = 1;
        }
    }

    // write segment header
    if (_absolute_ackno == 0) {
        seg.header().syn = true;
    }
    if (_stream.eof()) {
        seg.header().fin = true;
    }
    if (seg.header().syn) {
        seg.header().seqno = _isn;
    } else {
        if (_absolute_ackno - 1 < _stream.bytes_read()) {
            size_t outstanding_bytes = _stream.bytes_read() - _absolute_ackno + 1;
            seg.header().seqno = _ackno + outstanding_bytes;
        } else if (_absolute_ackno - 1 == _stream.bytes_read()) {
            seg.header().seqno = _ackno;
        } else {
            cout << "Should never reach here" << endl;
            seg.header().seqno = _ackno;
        }
    }

    // write payload
    string data = _stream.read(window_size);
    Buffer buffer = Buffer(std::move(data));
    seg.payload() = buffer;
    if (seg.length_in_sequence_space() > TCPConfig::MAX_PAYLOAD_SIZE) {
        cout << "Segment length too long" << endl;
    }

    cout << "segment" << endl;
    cout << "syn: " << seg.header().syn << endl;
    cout << "fin: " << seg.header().fin << endl;
    cout << "payload: " << seg.payload().copy() << endl;
    cout << "window_size: " << window_size << endl;
    cout << "sequence length: " << seg.length_in_sequence_space() << endl;
    cout << endl;
    if (seg.length_in_sequence_space() > 0) {
        uint64_t absolute_seqno = unwrap(seg.header().seqno, _isn, _absolute_ackno - 1);
        uint64_t absolute_ackno = absolute_seqno + seg.length_in_sequence_space();
        if (seg.payload().copy().empty() && _outstanding_segments.find(absolute_ackno) != _outstanding_segments.end()) {
            return;
        }
        _next_seqno += seg.length_in_sequence_space();
        _segments_out.push(seg);
        _outstanding_segments[absolute_ackno] = seg;
        if (!_timer.is_alive()) {_timer.start(_rto);}
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t absolute_ackno = unwrap(ackno, _isn, _absolute_ackno - 1);
    cout << "ack receive" << endl;
    cout << ackno << endl;
    cout << absolute_ackno << endl;
    cout << _next_seqno << endl;
    cout << window_size << endl;
    cout << endl;
    if (absolute_ackno > _absolute_ackno) {
        _rto = _initial_retransmission_timeout;
        _timer.stop();
        _timer.start(_rto);
        _consecutive_retransmissions = 0;
    }
    _ackno = ackno;
    _absolute_ackno = absolute_ackno;
    _window_size = window_size;
    vector<uint64_t> st;
    for (auto item : _outstanding_segments) {
        if (absolute_ackno >= item.first) {
            st.push_back(item.first);
        }
    }
    for (auto key : st) {
        _outstanding_segments.erase(key);
    }
    if (_outstanding_segments.empty()) {
        _timer.stop();
    }
    if (absolute_ackno == _next_seqno) {
        cout << "call fill window" << endl;
        TCPSender::fill_window();
    }
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
