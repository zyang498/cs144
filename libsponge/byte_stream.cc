#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : size(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t remaining_capacity = ByteStream::remaining_capacity();
    if (!data.empty() && remaining_capacity > 0) {
        end_of_file = false;
    }
    if (remaining_capacity >= data.size()) {
        stream += data;
        written_bytes += data.size();
        return data.size();
    } else {
        string write_data = data.substr(0, remaining_capacity);
        stream += write_data;
        written_bytes += remaining_capacity;
        return remaining_capacity;
    }
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string read_data = stream.substr(0, len);
    return read_data;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if (len >= buffer_size()) {
        read_bytes += buffer_size();
        stream = "";
        if (input_ended()) {
            end_of_file = true;
        }
    } else {
        string popped_stream = stream.substr(len);
        stream = popped_stream;
        read_bytes += len;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string read_data = peek_output(len);
    pop_output(len);
    return read_data;
}

void ByteStream::end_input() {
    input_end = true;
    if (remaining_capacity() == size) {
        end_of_file = true;
    }
}

bool ByteStream::input_ended() const { return input_end; }

size_t ByteStream::buffer_size() const { return stream.size(); }

bool ByteStream::buffer_empty() const { return stream.empty(); }

bool ByteStream::eof() const { return end_of_file; }

size_t ByteStream::bytes_written() const { return written_bytes; }

size_t ByteStream::bytes_read() const { return read_bytes; }

size_t ByteStream::remaining_capacity() const { return size - stream.size(); }
