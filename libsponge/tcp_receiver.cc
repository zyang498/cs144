#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (seg.header().syn && !_syn) {
        _syn = true;
        _isn = seg.header().seqno;
    }
    string data = seg.payload().copy();
    uint64_t f_unassembled = _reassembler.stream_out().bytes_written();
    uint64_t index = 0;
    if (seg.header().syn) {
        index = unwrap(seg.header().seqno + 1, _isn, f_unassembled - 1) - 1;
    } else {
        index = unwrap(seg.header().seqno, _isn, f_unassembled - 1) - 1;
    }
    if (_syn) {
        _reassembler.push_substring(data, index, seg.header().fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_syn) {
        uint64_t f_unassembled = _reassembler.stream_out().bytes_written();
        WrappingInt32 ack(0);
        if (_reassembler.stream_out().input_ended()) {
            ack = wrap(f_unassembled + 2, _isn);
        } else {
            ack = wrap(f_unassembled + 1, _isn);
        }
        return ack;
    }
    return {};
}

size_t TCPReceiver::window_size() const {
    uint64_t f_unassembled = _reassembler.stream_out().bytes_written();
    uint64_t f_unread = _reassembler.stream_out().bytes_read();
    uint64_t f_unacceptable = f_unread + _capacity;
    return f_unacceptable - f_unassembled;
}
