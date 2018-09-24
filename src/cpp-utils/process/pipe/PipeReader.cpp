#include "PipeReader.h"
#include <utility>
#include "../../data/Data.h"

using cpputils::Data;
using std::string;
using namespace cpputils::process;

PipeReader::PipeReader(PipeDescriptor fd): _stream(std::move(fd), "r") {
}

string PipeReader::receive() {
    uint64_t len;
    size_t res = fread(&len, sizeof(len), 1, _stream.stream());
    if (res != 1) {
        throw PipeNotReadableError();
    }

    // Protect from memory attacks
    if (len > MAX_READ_SIZE) {
        throw std::runtime_error("Message too large.");
    }

    if (len == 0) {
        return string();
    }

    Data message(len);
    res = fread(message.data(), len, 1, _stream.stream());
    if (res != 1) {
        throw std::runtime_error("Reading message from pipe failed.");
    }
    return string(static_cast<const char*>(message.data()), message.size());
}
